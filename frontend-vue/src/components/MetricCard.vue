<script setup lang="ts">
defineProps<{
  label: string;
  value: string;
  unit?: string;
  hint?: string;
  tone?: "green" | "amber" | "blue" | "pink";
}>();
</script>
<template>
  <article class="metric-card" :class="`tone-${tone || 'green'}`">
    <p class="metric-label">{{ label }}</p>
    <div class="metric-value"><strong>{{ value }}</strong><span v-if="unit">{{ unit }}</span></div>
    <p v-if="hint" class="metric-hint">{{ hint }}</p>
  </article>
</template>
