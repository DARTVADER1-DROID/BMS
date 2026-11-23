<script setup lang="ts">
import { ref } from "vue";
import { useSession } from "../services/auth/session";
const { signIn, state } = useSession();
const username = ref("");
const password = ref("");
const busy = ref(false);
const message = ref("");
async function submit(): Promise<void> {
  if (busy.value) return;
  busy.value = true;
  message.value = "";
  try {
    await signIn(username.value, password.value);
    password.value = "";
  } catch (error) {
    message.value = error instanceof Error ? error.message : "Unable to sign in.";
  } finally {
    busy.value = false;
  }
}
</script>
<template>
  <main class="auth-screen">
    <form class="auth-card" @submit.prevent="submit">
      <div class="brand-mark">B</div>
      <p class="eyebrow">BATTERY MANAGEMENT SYSTEM</p>
      <h1>Welcome back</h1>
      <p class="muted">Sign in to access live battery telemetry and controls.</p>
      <label for="username">Username</label>
      <input id="username" v-model.trim="username" name="username" autocomplete="username" maxlength="128" required />
      <label for="password">Password</label>
      <input id="password" v-model="password" name="password" type="password" autocomplete="current-password" required />
      <p v-if="message || state.error" class="form-error" role="alert">{{ message || state.error }}</p>
      <button class="button button-primary full-width" type="submit" :disabled="busy">{{ busy ? "Signing in…" : "Sign in" }}</button>
    </form>
  </main>
</template>
