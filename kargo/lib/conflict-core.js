import { pickHighestSatisfying } from "./ranges.js";

/**
 * @param {string[]} constraintRaws
 */
function meaningfulConstraints(constraintRaws) {
  return [...new Set(constraintRaws.map((r) => String(r).trim()))]
    .filter((r) => r && r !== "*")
    .sort((a, b) => a.localeCompare(b));
}

/**
 * True iff no remote tag satisfies all constraints in `subset` (same rules as the resolver).
 * @param {string[]} subset
 * @param {string[]} tags
 * @param {{ allowPrereleaseFallback?: boolean }} options
 */
export function isConstraintSetUnsatisfiable(subset, tags, options = {}) {
  const raws = subset.length ? subset : ["*"];
  return pickHighestSatisfying(tags, raws, options) === null;
}

/**
 * Shrinks to an irredundant unsatisfiable subset (each constraint participates in the conflict).
 * Returns null if the full set is satisfiable or only '*' was present.
 * @param {string[]} constraintRaws
 * @param {string[]} tags
 * @param {{ allowPrereleaseFallback?: boolean }} options
 */
export function minimalUnsatisfiableCore(constraintRaws, tags, options = {}) {
  const meaningful = meaningfulConstraints(constraintRaws);
  if (meaningful.length === 0) return null;
  if (!isConstraintSetUnsatisfiable(meaningful, tags, options)) return null;

  let S = [...meaningful];
  let changed = true;
  while (changed && S.length > 1) {
    changed = false;
    for (let i = 0; i < S.length; i += 1) {
      const rest = S.filter((_, j) => j !== i);
      if (rest.length === 0) break;
      if (isConstraintSetUnsatisfiable(rest, tags, options)) {
        S = rest;
        changed = true;
        break;
      }
    }
  }
  return S.length ? S : null;
}

/**
 * Edges for depKey whose constraint string appears in the core (deterministic order).
 * @param {string} depKey
 * @param {Array<{ depKey: string; sourceKey: string; constraintRaw: string }>} edges
 * @param {string[]} coreRaws
 */
export function edgesInConflictCore(depKey, edges, coreRaws) {
  const coreSet = new Set(coreRaws);
  return [...edges]
    .filter((e) => e.depKey === depKey && coreSet.has(String(e.constraintRaw).trim()))
    .sort(
      (a, b) =>
        a.constraintRaw.localeCompare(b.constraintRaw) || a.sourceKey.localeCompare(b.sourceKey)
    );
}
