import path from "node:path";
import os from "node:os";

export function kargoHome() {
  return path.join(os.homedir(), ".kargo");
}

export function kargoCacheDir() {
  return path.join(kargoHome(), "cache");
}

/** ls-remote tag lists: ~/.kargo/cache/tags/<owner>/<repo>.json */
export function remoteTagsCacheFile(owner, repo) {
  const o = owner.toLowerCase();
  const r = repo.toLowerCase();
  return path.join(kargoCacheDir(), "tags", o, `${r}.json`);
}

export function kargoPackagesDir() {
  return path.join(kargoHome(), "packages");
}

export function kargoIndexDir() {
  return path.join(kargoHome(), "index");
}

export function kargoConfigPath() {
  return path.join(kargoHome(), "config.json");
}

/** Global install: owner/repo -> ~/.kargo/packages/owner/repo/<semver>/ */
export function globalPackageRoot(owner, repo, semverVersion) {
  const o = owner.toLowerCase();
  const r = repo.toLowerCase();
  return path.join(kargoPackagesDir(), o, r, semverVersion);
}

export function projectKargoLock(projectDir) {
  return path.join(projectDir, "kargo.lock");
}

export function projectKargoToml(projectDir) {
  return path.join(projectDir, "kargo.toml");
}

export function projectKernPackagePaths(projectDir) {
  return path.join(projectDir, ".kern", "package-paths.json");
}
