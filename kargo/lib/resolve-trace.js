import semver from "semver";
import { tagToSemver } from "./spec.js";
import { versionSatisfiesConstraint } from "./ranges.js";
import { whySelectedRecord } from "./why-version.js";

/**
 * Rich, self-describing resolver step for --resolve-debug (deterministic ordering).
 */
export function buildResolveDebugStep({
  iteration,
  depKey,
  sortedRaws,
  sortedTags,
  pick,
  lsRemoteCacheSize,
  resolutionMode
}) {
  const raws = sortedRaws.length ? sortedRaws : ["*"];
  const remoteSample = sortedTags
    .map((t) => tagToSemver(t))
    .slice(-24)
    .reverse();

  /** @type {string[]} */
  const satisfyingStable = [];
  /** @type {Array<{ version: string; phase: string; firstFailingConstraint: string; reason: string }>} */
  const rejectionSamples = [];

  for (const t of sortedTags) {
    const sv = tagToSemver(t);
    if (!semver.valid(sv)) continue;
    if (semver.prerelease(sv)) continue;
    let failRaw = null;
    for (const raw of raws) {
      if (!versionSatisfiesConstraint(sv, raw, { includePrerelease: false })) {
        failRaw = raw;
        break;
      }
    }
    if (failRaw != null) {
      if (rejectionSamples.length < 10) {
        rejectionSamples.push({
          version: sv,
          phase: "stable",
          firstFailingConstraint: failRaw,
          reason: `fails semver.satisfies(${JSON.stringify(sv)}, ${JSON.stringify(failRaw)}, { includePrerelease: false })`
        });
      }
    } else {
      satisfyingStable.push(sv);
    }
  }

  satisfyingStable.sort((a, b) => semver.rcompare(a, b));
  const topSatisfying = satisfyingStable.slice(0, 10);

  const selectionReason = pick.choice
    ? resolutionMode === "locked"
      ? "version taken from kargo.lock (resolution_mode=locked; remote tag list not used for version choice)"
      : pick.usedPrereleaseFallback
        ? "highest version matching all constraints in prerelease phase (includePrerelease: true; no stable match)"
        : "highest stable version matching all constraints (prereleases excluded in phase 1)"
    : "no version matched";

  return {
    iteration,
    depKey,
    resolution_mode: resolutionMode,
    input_constraints: [...sortedRaws],
    remote_tag_count: sortedTags.length,
    remote_versions_sample_newest_first: remoteSample,
    satisfying_stable_candidates_newest_first: topSatisfying,
    rejection_samples_stable_phase: rejectionSamples,
    selected_version: pick.choice?.sv ?? null,
    selected_tag: pick.choice?.tag ?? null,
    used_prerelease_fallback: pick.usedPrereleaseFallback,
    selection_reason: selectionReason,
    why_selected: whySelectedRecord(sortedRaws, pick, resolutionMode),
    ls_remote_cache_size: lsRemoteCacheSize
  };
}
