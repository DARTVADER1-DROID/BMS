<script setup lang="ts">
import MetricCard from "../../components/MetricCard.vue";
import { formatMetric } from "../dashboard/telemetry";
import { useDashboardStore } from "../dashboard/dashboard-store";
const { state, refresh } = useDashboardStore();
</script>
<template>
  <section class="page-heading">
    <div><p class="eyebrow">BATTERY ANALYTICS</p><h1>Diagnostics</h1><p class="muted">Health estimates, hardware link and safety warnings.</p></div>
    <button class="button button-secondary" @click="refresh">Refresh now</button>
  </section>
  <section class="metric-grid metric-grid-three">
    <MetricCard label="State of charge" :value="formatMetric(state.soc, 1)" unit="%" tone="blue" />
    <MetricCard label="State of health" :value="formatMetric(state.soh, 1)" unit="%" tone="green" />
    <MetricCard label="Remaining useful life" :value="formatMetric(state.rul, 0)" unit="cycles" tone="amber" />
  </section>
  <section class="lower-grid">
    <article class="panel">
      <p class="eyebrow">CONNECTION</p><h2>System status</h2>
      <dl class="details-list">
        <div><dt>Backend</dt><dd :class="state.connected ? 'text-good' : 'text-bad'">{{ state.connected ? "Online" : "Offline" }}</dd></div>
        <div><dt>Hardware</dt><dd :class="state.hardwareConnected ? 'text-good' : 'text-warn'">{{ state.hardwareConnected ? "Connected" : "Disconnected" }}</dd></div>
        <div><dt>Operating state</dt><dd>{{ state.batteryState }}</dd></div>
        <div><dt>Control commissioning</dt><dd>{{ state.controlEnabled ? "Enabled" : "Disabled" }}</dd></div>
      </dl>
    </article>
    <article class="panel">
      <p class="eyebrow">SAFETY</p><h2>Warning log</h2>
      <ul v-if="state.warnings.length" class="warning-list"><li v-for="warning in state.warnings" :key="warning">{{ warning }}</li></ul>
      <p v-else class="empty-state">No active warnings reported.</p>
    </article>
  </section>
</template>
