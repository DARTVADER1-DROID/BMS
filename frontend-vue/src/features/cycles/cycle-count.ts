export const MAX_CYCLE_COUNT = 1_000_000_000;

export function parseCycleCount(input: string): number | null {
  if (!/^\d+$/.test(input.trim())) return null;
  const count = Number(input);
  if (!Number.isSafeInteger(count) || count < 0 || count > MAX_CYCLE_COUNT) return null;
  return count;
}
