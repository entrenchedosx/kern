import path from "node:path";
import { ensureDir, exists, readJson, writeJsonAtomic } from "./io.js";

export function normalizeManifestDependencies(manifest) {
  const deps = manifest?.dependencies;
  if (!deps) return {};
  if (Array.isArray(deps)) {
    const out = {};
    for (const name of deps) out[String(name)] = "*";
    return out;
  }
  if (typeof deps === "object") {
    const out = {};
    for (const [k, v] of Object.entries(deps)) {
      out[k] = String(v || "*");
    }
    return out;
  }
  throw new Error("Invalid kern.json dependencies format");
}

export async function readManifest(manifestPath) {
  if (!(await exists(manifestPath))) {
    return { name: "kern-project", version: "1.0.0", entry: "src/main.kn", dependencies: {} };
  }
  const manifest = await readJson(manifestPath);
  return {
    ...manifest,
    dependencies: normalizeManifestDependencies(manifest)
  };
}

export async function writeManifest(manifestPath, manifest) {
  await writeJsonAtomic(manifestPath, manifest);
}

export async function readLockfile(lockPath) {
  if (!(await exists(lockPath))) return null;
  return readJson(lockPath);
}

export function lockSatisfiesDirectDeps(lock, dependencies) {
  if (!lock || lock.lockVersion !== 2 || typeof lock.packages !== "object") return false;
  for (const name of Object.keys(dependencies)) {
    if (!lock.packages[name]) return false;
  }
  return true;
}

export function buildLockfile(registryUrl, resolvedPackages) {
  const packages = {};
  for (const [name, pkg] of Object.entries(resolvedPackages)) {
    packages[name] = {
      version: pkg.version,
      resolved: pkg.dist.tarball,
      integrity: pkg.dist.shasum,
      dependencies: pkg.dependencies || {}
    };
  }
  return {
    lockVersion: 2,
    registry: registryUrl,
    packages
  };
}

export async function writeLockfile(lockPath, lockData) {
  await ensureDir(path.dirname(lockPath));
  await writeJsonAtomic(lockPath, lockData);
}

export async function writePackagePaths(pathsFile, packagePaths) {
  await writeJsonAtomic(pathsFile, { version: 1, packages: packagePaths });
}
