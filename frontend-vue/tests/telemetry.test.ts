import { describe, expect, it } from "vitest";
import { appendHistory, formatMetric, normalizeWarnings } from "../src/features/dashboard/telemetry";

describe("telemetry helpers", () => {
  it("normalizes supported warning payloads", () => {
    expect(normalizeWarnings("Over temperature")).toEqual(["Over temperature"]);
    expect(normalizeWarnings(["", "Low voltage"])).toEqual(["Low voltage"]);
    expect(normalizeWarnings({ first: "Sensor fault", second: "" })).toEqual(["Sensor fault"]);
    expect(normalizeWarnings(undefined)).toEqual([]);
  });
  it("keeps only finite samples within the history limit", () => {
    expect(appendHistory([1, 2, 3], 4, 3)).toEqual([2, 3, 4]);
    expect(appendHistory([1, 2], Number.NaN, 3)).toEqual([1, 2]);
    expect(appendHistory([1, 2], 3, 0)).toEqual([]);
  });
  it("formats finite values and represents missing values safely", () => {
    expect(formatMetric(12.345, 2)).toBe("12.35");
    expect(formatMetric(null)).toBe("—");
    expect(formatMetric(Number.POSITIVE_INFINITY)).toBe("—");
  });
});
