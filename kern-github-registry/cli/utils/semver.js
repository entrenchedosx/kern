import semver from "semver";

export function maxSatisfyingVersion(versions, range) {
  return semver.maxSatisfying(versions, range || "*", { includePrerelease: false });
}

export function incrementVersion(version, bump) {
  const next = semver.inc(version, bump || "patch");
  if (!next) throw new Error(`Invalid bump '${bump}' for version '${version}'`);
  return next;
}
