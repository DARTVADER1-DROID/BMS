<script setup lang="ts">
import { ref } from "vue";
import { useDashboardStore } from "../dashboard/dashboard-store";
import { MAX_CYCLE_COUNT, parseCycleCount } from "./cycle-count";
const input = ref<string | number>("");
const busy = ref(false);
const message = ref("");
const error = ref("");
const lastSubmitted = ref<number | null>(null);
const { updateCycleCount } = useDashboardStore();

async function submit(): Promise<void> {
  const count = parseCycleCount(String(input.value));
  if (count === null) {
    error.value = `Enter a whole number between 0 and ${MAX_CYCLE_COUNT.toLocaleString()}.`;
    message.value = "";
    return;
  }
  busy.value = true;
  error.value = "";
  message.value = "";
  try {
    await updateCycleCount(count);
    lastSubmitted.value = count;
    message.value = "Cycle count updated successfully.";
  } catch (reason) {
    error.value = reason instanceof Error ? reason.message : "Cycle count update failed.";
  } finally {
    busy.value = false;
  }
}
function usePreset(value: number): void { input.value = String(value); error.value = ""; }
</script>
<template>
  <section class="page-heading">
    <div><p class="eyebrow">BATTERY LIFECYCLE</p><h1>Cycle counter</h1><p class="muted">Record the completed charge–discharge cycle count.</p></div>
  </section>
  <section class="cycle-layout">
    <article class="panel cycle-panel">
      <p class="eyebrow">UPDATE CYCLE COUNT</p><h2>Enter completed cycles</h2>
      <p class="muted">The backend accepts whole numbers from 0 to 1,000,000,000.</p>
      <form @submit.prevent="submit">
        <label for="cycle-count">New cycle count</label>
        <input id="cycle-count" v-model="input" class="cycle-input" type="number" min="0" :max="MAX_CYCLE_COUNT" step="1" inputmode="numeric" placeholder="Enter cycle count" />
        <div class="preset-row">
          <button v-for="preset in [10, 25, 50, 100, 200, 500]" :key="preset" class="preset-button" type="button" @click="usePreset(preset)">{{ preset }}</button>
        </div>
        <p v-if="error" class="form-error" role="alert">{{ error }}</p>
        <p v-if="message" class="form-success" role="status">{{ message }}</p>
        <button class="button button-primary full-width" type="submit" :disabled="busy">{{ busy ? "Updating…" : "Update cycle count" }}</button>
      </form>
    </article>
    <article class="panel cycle-current">
      <p class="eyebrow">THIS SESSION</p>
      <div class="cycle-number">{{ lastSubmitted === null ? "—" : lastSubmitted.toLocaleString() }}</div>
      <p class="muted">Last successfully submitted value</p>
      <p class="notice notice-warn">The current C API has no GET endpoint for the stored cycle count. This value only reflects successful updates made during this browser session.</p>
    </article>
  </section>
</template>
