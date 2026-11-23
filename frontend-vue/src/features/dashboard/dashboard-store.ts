import { computed, reactive, readonly } from "vue";
import { api } from "../../services/api/client";
import type { TelemetryResponse } from "../../services/api/contracts";
import { appendHistory, normalizeWarnings } from "./telemetry";

const state = reactive({
  connected: false, hardwareConnected: false, controlEnabled: false,
  batteryState: "unknown", telemetry: null as TelemetryResponse | null,
  soc: null as number | null, soh: null as number | null, rul: null as number | null,
  warnings: [] as string[], voltageHistory: [] as number[],
  currentHistory: [] as number[], temperatureHistory: [] as number[],
  loading: false, lastUpdated: null as Date | null, pollInterval: 1000, error: "",
});
let timer: ReturnType<typeof setInterval> | null = null;
let polling = false;

async function refresh(): Promise<void> {
  if (polling) return;
  polling = true;
  state.loading = state.telemetry === null;
  try {
    const results = await Promise.allSettled([
      api.health(), api.telemetry(), api.state(), api.hardware(),
      api.soc(), api.soh(), api.rul(),
    ]);
    const [health, telemetry, batteryState, hardware, soc, soh, rul] = results;
    state.connected = health.status === "fulfilled";
    if (health.status === "fulfilled") state.controlEnabled = health.value.control_enabled;
    if (telemetry.status === "fulfilled") {
      state.telemetry = telemetry.value;
      state.voltageHistory = appendHistory(state.voltageHistory, telemetry.value.voltage, 60);
      state.currentHistory = appendHistory(state.currentHistory, telemetry.value.current, 60);
      state.temperatureHistory = appendHistory(state.temperatureHistory, telemetry.value.temperature, 60);
    }
    if (batteryState.status === "fulfilled") {
      state.batteryState = batteryState.value.state || "unknown";
      state.warnings = normalizeWarnings(batteryState.value);
    }
    if (hardware.status === "fulfilled") state.hardwareConnected = hardware.value.hardware_connection === true;
    if (soc.status === "fulfilled") state.soc = soc.value.s_o_c;
    if (soh.status === "fulfilled") state.soh = soh.value.s_o_h;
    if (rul.status === "fulfilled") state.rul = rul.value.r_u_l;
    state.error = results.every((result) => result.status === "rejected")
      ? "The API is unreachable. Check the backend and network connection."
      : "";
    state.lastUpdated = new Date();
  } finally {
    state.loading = false;
    polling = false;
  }
}

function stopPolling(): void {
  if (timer !== null) clearInterval(timer);
  timer = null;
}
function startPolling(): void {
  stopPolling();
  void refresh();
  timer = setInterval(() => void refresh(), state.pollInterval);
}
function setPollInterval(milliseconds: number): void {
  if (![1000, 2000, 5000, 10000].includes(milliseconds)) return;
  state.pollInterval = milliseconds;
  if (timer !== null) startPolling();
}
async function runCommand(command: "charge" | "discharge" | "stop"): Promise<string> {
  const result = await api.command(command);
  await refresh();
  return result.message || `${command} command accepted`;
}
async function updateCycleCount(count: number): Promise<string> {
  const result = await api.updateCycles(count);
  await refresh();
  return result.message || "Cycle count updated";
}

export function useDashboardStore() {
  return {
    state: readonly(state),
    isLive: computed(() => state.connected && state.telemetry !== null),
    refresh, startPolling, stopPolling, setPollInterval, runCommand, updateCycleCount,
  };
}
