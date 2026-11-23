<script setup lang="ts">
import { computed } from "vue";
const props = defineProps<{ title: string; unit: string; values: number[]; stroke: string }>();
const points = computed(() => {
  if (props.values.length < 2) return "";
  const min = Math.min(...props.values);
  const range = Math.max(...props.values) - min || 1;
  return props.values.map((value, index) => {
    const x = index / (props.values.length - 1) * 600;
    const y = 118 - (value - min) / range * 100;
    return `${x.toFixed(1)},${y.toFixed(1)}`;
  }).join(" ");
});
const latest = computed(() => props.values.at(-1));
</script>
<template>
  <section class="chart-card">
    <div class="chart-heading">
      <h3>{{ title }}</h3>
      <span>{{ latest === undefined ? "Waiting for data" : `${latest.toFixed(2)} ${unit}` }}</span>
    </div>
    <svg class="chart-svg" viewBox="0 0 600 130" role="img" :aria-label="`${title} history chart`">
      <line v-for="y in [18, 52, 86, 120]" :key="y" x1="0" :y1="y" x2="600" :y2="y" class="chart-guide" />
      <polyline v-if="points" :points="points" fill="none" :stroke="stroke" stroke-width="3" stroke-linecap="round" stroke-linejoin="round" />
      <text v-if="!points" x="300" y="70" text-anchor="middle" class="chart-empty">Collecting telemetry…</text>
    </svg>
  </section>
</template>
