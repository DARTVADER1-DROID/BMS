import type { StateResponse } from "../../services/api/contracts";

export function normalizeWarnings(value: StateResponse["warning_messages"]): string[] {
  if (!value) return [];
  if (Array.isArray(value)) return value.filter(Boolean);
  if (typeof value === "string") return value.trim() ? [value] : [];
  return Object.values(value).filter(Boolean);
}

export function appendHistory(history: number[], nextValue: number, maximumLength: number): number[] {
  if (maximumLength < 1) return [];
  if (!Number.isFinite(nextValue)) return history.slice(-maximumLength);
  return [...history, nextValue].slice(-maximumLength);
}

export function formatMetric(value: number | null | undefined, decimals = 1): string {
  return typeof value === "number" && Number.isFinite(value) ? value.toFixed(decimals) : "—";
}
