import semver from "semver";
import { KargoCliError, EXIT } from "./cli-error.js";
import { tagToSemver } from "./spec.js";

/**
 * @param {string} refPart tag fragment after @, or range string
 * @returns {{ kind: "latest" } | { kind: "exact"; semver: string } | { kind: "range"; raw: string }}
 */
export function classifyVersionRef(refPart) {
  if (refPart == null) return { kind: "latest" };
  const r = String(refPart).trim();
  if (r === "" || r === "*") return { kind: "latest" };
  if (/[|^~>=\s*]/.test(r) || r.includes("||")) {
    if (!semver.validRange(r)) {
      throw new KargoCliError(`kargo: invalid semver range "${r}"`, EXIT.USER);
    }
    return { kind: "range", raw: r };
  }
  const sv = tagToSemver(r);
  if (!semver.valid(sv)) {
    if (!semver.validRange(r)) {
      throw new KargoCliError(`kargo: invalid version or range "${r}"`, EXIT.USER);
    }
    return { kind: "range", raw: r };
  }
  return { kind: "exact", semver: sv };
}

/** @param {string} raw */
export function constraintRawFromClassified(c) {
  if (c.kind === "latest") return "*";
  if (c.kind === "exact") return `=${c.semver}`;
  return c.raw;
}

/**
 * @param {string} version semver without v
 * @param {string} constraintRaw * or semver range string or =x.y.z
 * @param {{ includePrerelease?: boolean }} [opts]
 */
export function versionSatisfiesConstraint(version, constraintRaw, opts = {}) {
  const raw = String(constraintRaw).trim();
  // `*` adds no restriction; stricter constraints from other edges still apply.
  if (raw === "" || raw === "*") return true;
  return semver.satisfies(version, raw, {
    includePrerelease: opts.includePrerelease === true
  });
}

/**
 * Dedupe and sort tags ascending by semver (deterministic; same input → same order).
 * Canonical form: `v` + semver string from the tag.
 * @param {string[]} semverTags e.g. ["v1.0.0", "1.2.0"]
 * @returns {string[]}
 */
export function normalizeTagList(semverTags) {
  const seen = new Set();
  const out = [];
  for (const t of semverTags) {
    const sv = tagToSemver(t);
    if (!semver.valid(sv)) continue;
    if (seen.has(sv)) continue;
    seen.add(sv);
    out.push(`v${sv}`);
  }
  out.sort((a, b) => semver.compare(tagToSemver(a), tagToSemver(b)));
  return out;
}

/**
 * @param {string[]} sortedTags tags already normalizeTagList'd
 * @param {string[]} constraintRaws
 * @param {boolean} includePrerelease satifies() flag (when true, prerelease versions may match)
 * @param {boolean} onlyStable if true, skip versions where semver.prerelease(v) is set
 * @returns {{ tag: string; sv: string } | null}
 */
export function pickHighestSatisfyingFromSorted(sortedTags, constraintRaws, includePrerelease, onlyStable) {
  // Empty `*` only widens; combined with real ranges, every() still enforces those ranges.
  const raws = constraintRaws.length ? constraintRaws : ["*"];
  const candidates = [];
  for (const t of sortedTags) {
    const sv = tagToSemver(t);
    if (!semver.valid(sv)) continue;
    if (onlyStable && semver.prerelease(sv)) continue;
    if (
      raws.every((raw) =>
        versionSatisfiesConstraint(sv, raw, { includePrerelease })
      )
    ) {
      candidates.push({ tag: t, sv });
    }
  }
  if (!candidates.length) return null;
  candidates.sort((a, b) => semver.rcompare(a.sv, b.sv));
  return candidates[0];
}

/**
 * Prefer stable releases; if none satisfy and allowPrereleaseFallback, retry with prereleases.
 * @returns {{ choice: { tag: string; sv: string } | null; usedPrereleaseFallback: boolean; sortedTags: string[] }}
 */
export function pickHighestSatisfyingDetailed(semverTags, constraintRaws, options = {}) {
  const { allowPrereleaseFallback = true } = options;
  const sorted = normalizeTagList(semverTags);
  let best = pickHighestSatisfyingFromSorted(sorted, constraintRaws, false, true);
  if (best) return { choice: best, usedPrereleaseFallback: false, sortedTags: sorted };
  if (allowPrereleaseFallback) {
    best = pickHighestSatisfyingFromSorted(sorted, constraintRaws, true, false);
    if (best) return { choice: best, usedPrereleaseFallback: true, sortedTags: sorted };
  }
  return { choice: null, usedPrereleaseFallback: false, sortedTags: sorted };
}

/**
 * @param {string[]} semverTags
 * @param {string[]} constraintRaws
 * @param {{ allowPrereleaseFallback?: boolean }} [options]
 */
export function pickHighestSatisfying(semverTags, constraintRaws, options = {}) {
  return pickHighestSatisfyingDetailed(semverTags, constraintRaws, options).choice;
}

/**
 * Best-effort single semver range describing the intersection (for caret/tilde/>= .. < only).
 * Returns null if not expressible in this form or if it would not include `chosenSemver`.
 * @param {string[]} constraintRaws
 * @param {string | null} [chosenSemver] resolved version (no v prefix)
 */
export function normalizedConstraintIntersection(constraintRaws, chosenSemver = null) {
  const u = [
    ...new Set((constraintRaws || []).map((r) => String(r).trim()).filter((r) => r && r !== "*"))
  ].sort((a, b) => a.localeCompare(b));
  if (u.length === 0) return "*";
  if (u.length === 1) return u[0];

  if (u.some((r) => /^\s*=/.test(r))) return null;

  const lows = [];
  const highs = [];
  for (const r of u) {
    const b = rangeToHalfOpenInterval(r);
    if (!b) return null;
    lows.push(b.lo);
    highs.push(b.hiExclusive);
  }
  const maxLo = lows.reduce((a, b) => (semver.gt(a, b) ? a : b));
  const minHi = highs.reduce((a, b) => (semver.lt(a, b) ? a : b));
  if (!semver.lt(maxLo, minHi)) return null;

  const out = `>=${maxLo.version} <${minHi.version}`;
  if (!semver.validRange(out)) return null;
  if (chosenSemver && semver.valid(chosenSemver) && !semver.satisfies(chosenSemver, out)) {
    return null;
  }
  return out;
}

/**
 * @param {string} r
 * @returns {{ lo: import("semver").SemVer; hiExclusive: import("semver").SemVer } | null}
 */
/** Exported for conflict explanations and tooling. */
export function rangeToHalfOpenInterval(r) {
  const t = r.trim();
  const caret = /^\^\s*(\d+)\.(\d+)\.(\d+)(?:-[0-9A-Za-z.-]+)?$/i.exec(t);
  if (caret) {
    const lo = semver.parse(`${caret[1]}.${caret[2]}.${caret[3]}`);
    if (!lo) return null;
    const hi = new semver.SemVer(`${parseInt(caret[1], 10) + 1}.0.0`);
    return { lo, hiExclusive: hi };
  }
  const tilde = /^\~\s*(\d+)\.(\d+)\.(\d+)(?:-[0-9A-Za-z.-]+)?$/i.exec(t);
  if (tilde) {
    const major = parseInt(tilde[1], 10);
    const minor = parseInt(tilde[2], 10);
    const lo = semver.parse(`${tilde[1]}.${tilde[2]}.${tilde[3]}`);
    if (!lo) return null;
    const hi = new semver.SemVer(`${major}.${minor + 1}.0`);
    return { lo, hiExclusive: hi };
  }
  const lo = semver.minVersion(t);
  const ltPart = /\s<\s*([^\s]+)/.exec(t);
  if (lo && ltPart) {
    const hi = semver.parse(ltPart[1]);
    if (hi) return { lo, hiExclusive: hi };
  }
  return null;
}

/** Plain-language summary for errors (caret/tilde/>= .. < only get interval form). */
export function describeConstraintAcceptSet(raw) {
  const t = String(raw).trim();
  if (!t || t === "*") return "any version (*)";
  const b = rangeToHalfOpenInterval(t);
  if (b) {
    return `roughly >=${b.lo.version} and <${b.hiExclusive.version} (from ${t})`;
  }
  return `versions matching semver range: ${t}`;
}

/** Human-readable merged constraint for lockfiles (conjunction of inputs). */
export function formatMergedVersionRange(constraintRaws) {
  const u = [
    ...new Set(
      (constraintRaws.length ? constraintRaws : ["*"]).map((r) => String(r).trim())
    )
  ].sort((a, b) => a.localeCompare(b));
  if (u.length === 0 || (u.length === 1 && u[0] === "*")) return "*";
  return u.join(" AND ");
}

/**
 * @param {string} raw
 */
export function assertValidConstraintRaw(raw) {
  const r = String(raw).trim();
  if (r === "" || r === "*") return;
  if (!semver.validRange(r)) {
    throw new KargoCliError(`kargo: invalid semver range "${raw}"`, EXIT.USER);
  }
}
