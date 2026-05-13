import { tagToSemver } from "./spec.js";
import { normalizeTagList } from "./ranges.js";
import { minimalUnsatisfiableCore, edgesInConflictCore } from "./conflict-core.js";
import { explainPairwiseCore } from "./semver-conflict-explain.js";

/**
 * Readable, multi-line resolution failure (stderr-friendly).
 */
export function formatResolutionFailure(depKey, edges, raws, tags, options = {}) {
  const { allowPrereleaseFallback = true } = options;
  const sorted = normalizeTagList(tags);
  const numericVers = sorted.map((t) => tagToSemver(t));
  const displayVers = [...numericVers].reverse().slice(0, 16);

  const meaningful = [...new Set(raws.map((r) => String(r).trim()))].filter((r) => r && r !== "*");
  const core = minimalUnsatisfiableCore(raws, sorted, { allowPrereleaseFallback });

  const lines = [
    "",
    "Dependency resolution failed",
    "",
    `Package: ${depKey}`,
    ""
  ];

  if (core && core.length) {
    const sameAsAll = meaningful.length > 0 && core.length === meaningful.length;
    lines.push(
      sameAsAll
        ? "Conflicting constraints:"
        : "Minimal conflicting subset (smallest set that still cannot be satisfied):"
    );
    const seenC = new Set();
    for (const e of edgesInConflictCore(depKey, edges, core)) {
      const sig = `${e.sourceKey}\t${e.constraintRaw}`;
      if (seenC.has(sig)) continue;
      seenC.add(sig);
      lines.push(`  - ${e.constraintRaw} (required by ${e.sourceKey})`);
    }
    lines.push("");
    if (core.length === 2) {
      lines.push(...explainPairwiseCore(core[0], core[1]));
      lines.push("");
    }
    if (!sameAsAll && meaningful.length > core.length) {
      lines.push("All constraints involved (for context):");
      const edgeList = [...edges].sort(
        (a, b) =>
          a.sourceKey.localeCompare(b.sourceKey) ||
          a.constraintRaw.localeCompare(b.constraintRaw) ||
          a.depKey.localeCompare(b.depKey)
      );
      const seenA = new Set();
      for (const e of edgeList) {
        if (e.depKey !== depKey) continue;
        const sig = `${e.sourceKey}\t${e.constraintRaw}`;
        if (seenA.has(sig)) continue;
        seenA.add(sig);
        lines.push(`  - ${e.constraintRaw} (required by ${e.sourceKey})`);
      }
      lines.push("");
    }
  } else {
    lines.push("Conflicting or unsatisfiable constraints:");
    const edgeList = [...edges].sort(
      (a, b) =>
        a.sourceKey.localeCompare(b.sourceKey) ||
        a.constraintRaw.localeCompare(b.constraintRaw) ||
        a.depKey.localeCompare(b.depKey)
    );
    const seen = new Set();
    for (const e of edgeList) {
      if (e.depKey !== depKey) continue;
      const sig = `${e.sourceKey}\t${e.constraintRaw}`;
      if (seen.has(sig)) continue;
      seen.add(sig);
      lines.push(`  - ${e.constraintRaw} (required by ${e.sourceKey})`);
    }
    lines.push("");
  }

  const merged = [...new Set(raws.map((r) => String(r).trim()))].sort().join(" AND ") || "*";
  lines.push(`Merged requirement: ${merged}`);
  lines.push("");
  lines.push("No published tag satisfies all constraints.");
  lines.push("");

  if (displayVers.length) {
    lines.push("Available versions (newest first, sampled):");
    for (const v of displayVers) {
      lines.push(`  ${v}`);
    }
    lines.push("");
  }

  lines.push("Suggestions:");
  lines.push("  - Widen or align semver ranges in kargo.toml so their intersection is non-empty.");
  lines.push("  - Publish a tag that satisfies every dependent.");

  const reqs = edges.filter((e) => e.depKey === depKey);
  if (reqs.length === 2) {
    const [a, b] = reqs;
    lines.push(
      `  - If both sides can move together: reconcile "${a.constraintRaw}" (${a.sourceKey}) with "${b.constraintRaw}" (${b.sourceKey}).`
    );
  }
  lines.push("");
  return lines.join("\n");
}
