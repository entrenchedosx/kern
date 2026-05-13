import path from "node:path";
import fs from "node:fs/promises";
import { ensureDir, exists, readJson } from "./utils/io.js";
import {
  fetchRegistryIndex,
  readBufferFromUrl
} from "./utils/fetchRegistry.js";
import { verifyIntegrity } from "./utils/integrity.js";
import { extractTarball, ensureTarballPresent } from "./utils/zip.js";
import { resolveDependencyGraph } from "./utils/resolve.js";
import {
  buildLockfile,
  lockSatisfiesDirectDeps,
  readLockfile,
  readManifest,
  writeLockfile,
  writeManifest,
  writePackagePaths
} from "./utils/lockfile.js";
import {
  getCacheRoot,
  getLockfilePath,
  getManifestPath,
  getProjectPackagePathsFile,
  getProjectPackagesRoot
} from "./utils/paths.js";

function parseInstallSpec(spec) {
  if (!spec) return null;
  const at = spec.lastIndexOf("@");
  if (at <= 0) return { name: spec, range: "*" };
  return { name: spec.slice(0, at), range: spec.slice(at + 1) || "*" };
}

function parseArgs(argv) {
  const out = { project: process.cwd(), update: false, spec: null, api: null };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--project" && i + 1 < argv.length) {
      out.project = path.resolve(argv[++i]);
    } else if (a === "--api" && i + 1 < argv.length) {
      out.api = String(argv[++i]).replace(/\/+$/, "");
    } else if (a === "--update") {
      out.update = true;
    } else if (!out.spec) {
      out.spec = parseInstallSpec(a);
    } else {
      throw new Error(`Unknown install arg: ${a}`);
    }
  }
  return out;
}

async function installFromLock(projectDir, lock) {
  const cacheRoot = getCacheRoot();
  const packagesRoot = getProjectPackagesRoot(projectDir);
  await ensureDir(cacheRoot);
  await ensureDir(packagesRoot);
  const packagePaths = {};

  for (const [name, item] of Object.entries(lock.packages || {})) {
    const version = item.version;
    const installRoot = path.join(packagesRoot, name, version);
    const integrityPart = String(item.integrity || "").replace(/^sha256-/, "").slice(0, 16) || "legacy";
    const cacheTar = path.join(cacheRoot, `${name}-${version}-${integrityPart}.tgz`);
    const tarPath = await ensureTarballPresent(cacheTar, () => readBufferFromUrl(item.resolved));
    const bytes = await fs.readFile(tarPath);
    if (!verifyIntegrity(bytes, item.integrity)) {
      throw new Error(`Integrity verification failed for ${name}@${version}`);
    }
    await fs.rm(installRoot, { recursive: true, force: true });
    await ensureDir(installRoot);
    await extractTarball(tarPath, installRoot);

    const nestedManifestPath = path.join(installRoot, "kern.json");
    let main = "src/index.kn";
    if (await exists(nestedManifestPath)) {
      const nested = await readJson(nestedManifestPath);
      main = nested.main || "src/index.kn";
    }
    packagePaths[name] = {
      version,
      root: installRoot,
      main: path.join(installRoot, main)
    };
  }

  await writePackagePaths(getProjectPackagePathsFile(projectDir), packagePaths);
}

export async function runInstall(argv) {
  const args = parseArgs(argv);
  if (args.api) process.env.KERN_REGISTRY_API_URL = args.api;
  const manifestPath = getManifestPath(args.project);
  const lockPath = getLockfilePath(args.project);

  const manifest = await readManifest(manifestPath);
  manifest.dependencies = manifest.dependencies || {};

  if (args.spec) {
    manifest.dependencies[args.spec.name] = args.spec.range;
    await writeManifest(manifestPath, manifest);
  }

  let lock = await readLockfile(lockPath);
  const canUseLock =
    !args.update && lockSatisfiesDirectDeps(lock, manifest.dependencies);

  if (!canUseLock) {
    const { registryUrl, index } = await fetchRegistryIndex();
    const resolved = await resolveDependencyGraph({
      index,
      registryUrl,
      dependencies: manifest.dependencies
    });
    lock = buildLockfile(registryUrl, resolved);
    await writeLockfile(lockPath, lock);
  }

  await installFromLock(args.project, lock);
  console.log(`Installed ${Object.keys(lock.packages || {}).length} package(s).`);
}
