import semver from "semver";

export function isExactVersion(input) {
  return Boolean(semver.valid(input));
}

export function normalizeRange(input) {
  if (!input || input === "*") return "*";
  const trimmed = String(input).trim();
  if (trimmed === "") return "*";
  if (isExactVersion(trimmed)) return trimmed;
  if (!semver.validRange(trimmed)) {
    throw new Error(`Invalid semver range: ${input}`);
  }
  return trimmed;
}

export function satisfiesRange(version, range) {
  if (!semver.valid(version)) return false;
  const r = normalizeRange(range);
  if (r === "*") return true;
  return semver.satisfies(version, r, { includePrerelease: false });
}

export function maxSatisfyingVersion(versions, range) {
  const r = normalizeRange(range);
  const selected = semver.maxSatisfying(versions, r, { includePrerelease: false });
  return selected || null;
}

export function compareVersions(a, b) {
  return semver.compare(a, b);
}

export function incrementVersion(version, bumpType) {
  if (!semver.valid(version)) {
    throw new Error(`Cannot bump invalid version: ${version}`);
  }
  const bumped = semver.inc(version, bumpType);
  if (!bumped) throw new Error(`Unsupported bump type: ${bumpType}`);
  return bumped;
}
