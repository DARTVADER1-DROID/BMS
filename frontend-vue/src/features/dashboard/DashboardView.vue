<script setup lang="ts">
import { computed } from "vue";
import MetricCard from "../../components/MetricCard.vue";
import TelemetryChart from "../../components/TelemetryChart.vue";
import { formatMetric } from "./telemetry";
import { useDashboardStore } from "./dashboard-store";
const { state, runCommand, setPollInterval } = useDashboardStore();
const commandBusy = computed(() => !state.controlEnabled);
const stateClass = computed(() => state.batteryState.toLowerCase().replace(/[^a-z0-9-]/g, ""));
async function command(name: "charge" | "discharge" | "stop"): Promise<void> {
  try { await runCommand(name); }
  catch (error) { window.alert(error instanceof Error ? error.message : "Command failed"); }
}
</script>
<template>
  <section class="page-heading">
    <div><p class="eyebrow">LIVE MONITORING</p><h1>Battery overview</h1><p class="muted">Live electrical measurements, battery estimates and operating state.</p></div>
    <div class="status-stack">
      <span class="status-pill" :class="state.connected ? 'status-ok' : 'status-bad'"><i></i>{{ state.connected ? "API online" : "API offline" }}</span>
      <span class="status-pill" :class="state.hardwareConnected ? 'status-ok' : 'status-warn'"><i></i>{{ state.hardwareConnected ? "Hardware linked" : "Hardware disconnected" }}</span>
    </div>
  </section>
  <p v-if="state.error" class="notice notice-error" role="alert">{{ state.error }}</p>
  <p v-if="!state.controlEnabled" class="notice notice-warn">Battery controls are disabled by the backend until the hardware is commissioned.</p>
  <section class="metric-grid" aria-label="Battery metrics">
    <MetricCard label="Voltage" :value="formatMetric(state.telemetry?.voltage, 3)" unit="V" tone="green" hint="Live sensor reading" />
    <MetricCard label="Current" :value="formatMetric(state.telemetry?.current, 3)" unit="A" tone="amber" hint="Live sensor reading" />
    <MetricCard label="Temperature" :value="formatMetric(state.telemetry?.temperature, 2)" unit="°C" tone="pink" hint="Battery temperature" />
    <MetricCard label="State of charge" :value="formatMetric(state.soc, 1)" unit="%" tone="blue" hint="Estimated SOC" />
    <MetricCard label="State of health" :value="formatMetric(state.soh, 1)" unit="%" tone="green" hint="Estimated SOH" />
    <MetricCard label="Remaining life" :value="formatMetric(state.rul, 0)" unit="cycles" tone="amber" hint="Estimated remaining cycles" />
  </section>
  <section class="section-heading">
    <div><p class="eyebrow">SIGNAL HISTORY</p><h2>Recent telemetry</h2></div>
    <label class="poll-control">Refresh
      <select :value="state.pollInterval" @change="setPollInterval(Number(($event.target as HTMLSelectElement).value))">
        <option :value="1000">1 second</option><option :value="2000">2 seconds</option>
        <option :value="5000">5 seconds</option><option :value="10000">10 seconds</option>
      </select>
    </label>
  </section>
  <section class="chart-grid">
    <TelemetryChart title="Voltage" unit="V" :values="state.voltageHistory" stroke="#49e2a4" />
    <TelemetryChart title="Current" unit="A" :values="state.currentHistory" stroke="#f5bd55" />
    <TelemetryChart title="Temperature" unit="°C" :values="state.temperatureHistory" stroke="#f27b93" />
  </section>
  <section class="lower-grid">
    <article class="panel">
      <div class="panel-heading"><div><p class="eyebrow">OPERATIONS</p><h2>Battery controls</h2></div><span class="state-tag" :class="stateClass">{{ state.batteryState }}</span></div>
      <div class="control-grid">
        <button class="button button-charge" :disabled="commandBusy" @click="command('charge')">Start charging</button>
        <button class="button button-discharge" :disabled="commandBusy" @click="command('discharge')">Start discharging</button>
        <button class="button button-stop" :disabled="commandBusy" @click="command('stop')">Stop</button>
      </div>
      <p class="muted small">Commands require a valid signed session and CSRF token.</p>
    </article>
    <article class="panel">
      <div class="panel-heading"><div><p class="eyebrow">FAULT MONITOR</p><h2>Active warnings</h2></div><span class="count-tag">{{ state.warnings.length }}</span></div>
      <ul v-if="state.warnings.length" class="warning-list"><li v-for="warning in state.warnings" :key="warning">{{ warning }}</li></ul>
      <p v-else class="empty-state">✓ No active warnings reported by the backend.</p>
    </article>
  </section>
</template>
