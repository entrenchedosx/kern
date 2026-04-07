/**
 * Human-readable "why this version" for CLI / debug.
 */
export function summarizeWhySelected(sortedRaws, pick, resolutionMode = "latest") {
  if (!pick?.choice) return "";
  const constraints = sortedRaws.length ? sortedRaws.join(", ") : "*";
  if (resolutionMode === "locked") {
    return [
      `Selected: ${pick.choice.tag} (${pick.choice.sv})`,
      "Reason:",
      "  - version taken from kargo.lock (resolution_mode=locked)",
      `  - must still satisfy merged constraints: ${constraints}`
    ].join("\n");
  }
  const lines = [
    `Selected: ${pick.choice.tag} (${pick.choice.sv})`,
    "Reason:",
    "  - satisfies all merged constraints",
    `  - constraint set: ${constraints}`,
    pick.usedPrereleaseFallback
      ? "  - highest matching tag when including prereleases (no stable tag matched all constraints)"
      : "  - highest stable tag on the remote that matched all constraints (prereleases excluded in phase 1)"
  ];
  return lines.join("\n");
}

/** Structured form for --resolve-debug JSON. */
export function whySelectedRecord(sortedRaws, pick, resolutionMode = "latest") {
  if (!pick?.choice) return null;
  const bullets =
    resolutionMode === "locked"
      ? [
          "version and tag taken from kargo.lock (resolution_mode=locked)",
          "remote tag list was not used to choose the semver"
        ]
      : [
          "satisfies every entry in input_constraints",
          pick.usedPrereleaseFallback
            ? "no stable tag matched all constraints; highest match chosen with includePrerelease: true"
            : "among stable tags, highest semver that matched every constraint (prereleases excluded in phase 1)"
        ];
  return {
    selected_tag: pick.choice.tag,
    selected_version: pick.choice.sv,
    satisfies_all_merged_constraints: true,
    highest_matching_under_policy: resolutionMode !== "locked",
    prerelease_resolution_phase: resolutionMode !== "locked" && pick.usedPrereleaseFallback,
    resolution_mode: resolutionMode,
    merged_constraints: [...sortedRaws],
    explanation_bullets: bullets
  };
}
