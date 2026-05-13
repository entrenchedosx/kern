/**
 * Global CLI verbosity (set from entry.js when user passes leading -v / --verbose or KARGO_VERBOSE).
 */
export function kargoVerbose() {
  const v = process.env.KARGO_VERBOSE;
  return v === "1" || v === "true" || String(v).toLowerCase() === "yes";
}
