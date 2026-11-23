import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

const api = vi.hoisted(() => ({
  health: vi.fn(), telemetry: vi.fn(), state: vi.fn(), hardware: vi.fn(),
  soc: vi.fn(), soh: vi.fn(), rul: vi.fn(), command: vi.fn(), updateCycles: vi.fn(),
}));

vi.mock("../src/services/api/client", () => ({ api }));

const healthy = { message: "ok", implementation: "c", control_enabled: true };
const sample = { voltage: 12.4, current: 2.1, temperature: 28, time: 1, hardware_connection: true };

async function loadStore() {
  vi.resetModules();
  const module = await import("../src/features/dashboard/dashboard-store");
  return module.useDashboardStore();
}

describe("dashboard store integration", () => {
  beforeEach(() => {
    api.health.mockResolvedValue(healthy);
    api.telemetry.mockResolvedValue(sample);
    api.state.mockResolvedValue({ state: "idle", warning_messages: ["Low voltage"] });
    api.hardware.mockResolvedValue({ hardware_connection: true });
    api.soc.mockResolvedValue({ s_o_c: 75 });
    api.soh.mockResolvedValue({ s_o_h: 92 });
    api.rul.mockResolvedValue({ r_u_l: 180 });
    api.command.mockResolvedValue({ message: "command accepted" });
    api.updateCycles.mockResolvedValue({ message: "cycles updated" });
  });

  afterEach(() => {
    vi.useRealTimers();
    vi.clearAllMocks();
  });

  it("aggregates successful API responses into one dashboard snapshot", async () => {
    const store = await loadStore();

    await store.refresh();

    expect(store.isLive.value).toBe(true);
    expect(store.state.connected).toBe(true);
    expect(store.state.hardwareConnected).toBe(true);
    expect(store.state.controlEnabled).toBe(true);
    expect(store.state.telemetry).toEqual(sample);
    expect(store.state.soc).toBe(75);
    expect(store.state.soh).toBe(92);
    expect(store.state.rul).toBe(180);
    expect(store.state.warnings).toEqual(["Low voltage"]);
    expect(store.state.voltageHistory).toEqual([12.4]);
    expect(store.state.loading).toBe(false);
    expect(store.state.lastUpdated).toBeInstanceOf(Date);
  });

  it("keeps working fields when only one telemetry endpoint fails", async () => {
    api.soc.mockRejectedValue(new Error("SOC endpoint unavailable"));
    const store = await loadStore();

    await store.refresh();

    expect(store.state.connected).toBe(true);
    expect(store.state.telemetry).toEqual(sample);
    expect(store.state.soc).toBeNull();
    expect(store.state.soh).toBe(92);
    expect(store.state.error).toBe("");
  });

  it("reports an unreachable API when every endpoint fails", async () => {
    for (const method of Object.values(api)) method.mockRejectedValue(new Error("offline"));
    const store = await loadStore();

    await store.refresh();

    expect(store.isLive.value).toBe(false);
    expect(store.state.error).toContain("API is unreachable");
    expect(store.state.loading).toBe(false);
  });

  it("bounds telemetry history to the configured maximum", async () => {
    const store = await loadStore();

    for (let index = 0; index < 65; index += 1) {
      api.telemetry.mockResolvedValueOnce({ ...sample, voltage: index });
      await store.refresh();
    }

    expect(store.state.voltageHistory).toHaveLength(60);
    expect(store.state.voltageHistory[0]).toBe(5);
    expect(store.state.voltageHistory.at(-1)).toBe(64);
  });

  it("prevents overlapping refresh calls", async () => {
    let release: ((value: typeof healthy) => void) | undefined;
    api.health.mockImplementationOnce(() => new Promise((resolve) => { release = resolve; }));
    const store = await loadStore();

    const first = store.refresh();
    await store.refresh();
    expect(api.health).toHaveBeenCalledTimes(1);

    release?.(healthy);
    await first;
  });

  it("validates polling intervals and restarts an active poller", async () => {
    vi.useFakeTimers();
    const store = await loadStore();
    store.startPolling();
    store.setPollInterval(3000);
    expect(store.state.pollInterval).toBe(1000);
    store.setPollInterval(5000);
    expect(store.state.pollInterval).toBe(5000);
    store.stopPolling();
    const callsBefore = api.health.mock.calls.length;
    await vi.advanceTimersByTimeAsync(6000);
    expect(api.health.mock.calls.length).toBe(callsBefore);
  });

  it("executes a control command then refreshes dashboard state", async () => {
    const store = await loadStore();

    await expect(store.runCommand("stop")).resolves.toBe("command accepted");

    expect(api.command).toHaveBeenCalledWith("stop");
    expect(api.health).toHaveBeenCalledTimes(1);
    expect(store.state.telemetry).toEqual(sample);
  });

  it("updates cycle count then refreshes dashboard state", async () => {
    const store = await loadStore();

    await expect(store.updateCycleCount(42)).resolves.toBe("cycles updated");

    expect(api.updateCycles).toHaveBeenCalledWith(42);
    expect(api.health).toHaveBeenCalledTimes(1);
  });
});
