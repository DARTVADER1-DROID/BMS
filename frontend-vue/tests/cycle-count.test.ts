import { describe, expect, it } from "vitest";
import { MAX_CYCLE_COUNT, parseCycleCount } from "../src/features/cycles/cycle-count";

describe("cycle count validation", () => {
  it("accepts zero and the backend maximum", () => {
    expect(parseCycleCount("0")).toBe(0);
    expect(parseCycleCount(String(MAX_CYCLE_COUNT))).toBe(MAX_CYCLE_COUNT);
  });
  it("rejects empty, fractional, negative, unsafe and out-of-range values", () => {
    for (const value of ["", " ", "-1", "1.5", "abc", "1e3", "1000000001", "9007199254740992"]) {
      expect(parseCycleCount(value)).toBeNull();
    }
  });
  it("allows surrounding whitespace but not signs or decimals", () => {
    expect(parseCycleCount(" 42 ")).toBe(42);
    expect(parseCycleCount("+42")).toBeNull();
  });
});
