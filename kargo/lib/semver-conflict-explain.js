import semver from "semver";
import { rangeToHalfOpenInterval, describeConstraintAcceptSet } from "./ranges.js";

/**
 * When exactly two constraints form the minimal core, explain disjoint intervals when provable.
 * @returns {string[]}
 */
export function explainPairwiseCore(a, b) {
  const lines = ["Why these two conflict:", `  • ${a}: ${describeConstraintAcceptSet(a)}`, `  • ${b}: ${describeConstraintAcceptSet(b)}`];
  const ia = rangeToHalfOpenInterval(String(a).trim());
  const ib = rangeToHalfOpenInterval(String(b).trim());
  if (ia && ib) {
    const disjoint =
      semver.compare(ia.lo, ib.hiExclusive) >= 0 || semver.compare(ib.lo, ia.hiExclusive) >= 0;
    if (disjoint) {
      lines.push(
        "  → Those intervals do not overlap: no single semver can satisfy both at once."
      );
    } else {
      lines.push(
        "  → (Intervals overlap on paper but no published tag matched — e.g. gaps on the remote.)"
      );
    }
  } else {
    lines.push(
      "  → Ranges use forms we cannot simplify to a single interval; compare the semver expressions above."
    );
  }
  return lines;
}
