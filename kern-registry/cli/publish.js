import path from "node:path";
import fs from "node:fs/promises";
import { pathToFileURL } from "node:url";
import { createTarball } from "./utils/zip.js";
import { ensureDir, exists, readJson, writeJsonAtomic } from "./utils/io.js";
import { incrementVersion } from "./utils/semver.js";
import { sha256Hex, toIntegrity } from "./utils/integrity.js";
import { getManifestPath } from "./utils/paths.js";
import { getRegistryApiBase } from "./utils/fetchRegistry.js";

function parseArgs(argv) {
  const out = { dir: process.cwd(), bump: null, public: false, dryRun: false, api: null };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--dir" && i + 1 < argv.length) {
      out.dir = path.resolve(argv[++i]);
    } else if (a === "--bump" && i + 1 < argv.length) {
      out.bump = argv[++i];
    } else if (a === "--api" && i + 1 < argv.length) {
      out.api = String(argv[++i]).replace(/\/+$/, "");
    } else if (a === "--public") {
      out.public = true;
    } else if (a === "--dry-run") {
      out.dryRun = true;
    } else {
      throw new Error(`Unknown publish arg: ${a}`);
    }
  }
  return out;
}

function resolveRegistryRoot() {
  if (process.env.KERN_REGISTRY_ROOT) {
    return path.resolve(process.env.KERN_REGISTRY_ROOT);
  }
  return path.resolve(process.cwd(), "kern-registry");
}

async function validatePackage(projectDir, manifest) {
  if (!manifest?.name) throw new Error("kern.json missing package name");
  if (!manifest?.version) throw new Error("kern.json missing version");
  if (!manifest?.main) throw new Error("kern.json missing main");
  const srcDir = path.join(projectDir, "src");
  if (!(await exists(srcDir))) throw new Error("Package must contain src/");
  const mainFile = path.join(projectDir, manifest.main);
  if (!(await exists(mainFile))) throw new Error(`main entry not found: ${manifest.main}`);
}

function getPublishApiBase(args) {
  if (args.api) return args.api;
  const base = getRegistryApiBase();
  if (base) return base;
  return process.env.KERN_REGISTRY_API_URL
    ? String(process.env.KERN_REGISTRY_API_URL).replace(/\/+$/, "")
    : "http://127.0.0.1:4873";
}

async function publishToApi(apiBase, payload) {
  const publishUrl = `${apiBase}/api/v1/packages`;
  const headers = { "content-type": "application/json" };
  const key = process.env.KERN_REGISTRY_API_KEY || "";
  if (key) headers["x-api-key"] = key;
  const res = await fetch(publishUrl, {
    method: "POST",
    headers,
    body: JSON.stringify(payload)
  });
  const text = await res.text();
  let data = {};
  try {
    data = JSON.parse(text || "{}");
  } catch {
    data = { error: text || `HTTP ${res.status}` };
  }
  if (!res.ok) {
    throw new Error(data.error || `publish failed with HTTP ${res.status}`);
  }
  return data;
}

export async function runPublish(argv) {
  const args = parseArgs(argv);
  const projectDir = args.dir;
  const manifestPath = getManifestPath(projectDir);
  if (!(await exists(manifestPath))) throw new Error("kern.json not found");
  const manifest = await readJson(manifestPath);
  await validatePackage(projectDir, manifest);

  const mirrorRoot = process.env.KERN_REGISTRY_ROOT ? resolveRegistryRoot() : null;
  const indexPath = mirrorRoot ? path.join(mirrorRoot, "registry", "registry.json") : null;
  const mirrorEnabled = Boolean(indexPath && (await exists(indexPath)));
  const index = mirrorEnabled ? await readJson(indexPath) : null;
  const packageName = manifest.name;
  const pkgDir = mirrorEnabled ? path.join(mirrorRoot, "registry", "packages", packageName) : null;
  const metadataPath = mirrorEnabled ? path.join(pkgDir, "metadata.json") : null;
  const versionsDir = mirrorEnabled ? path.join(pkgDir, "versions") : null;
  const distDir = mirrorEnabled ? path.join(mirrorRoot, "registry", "dist", packageName) : path.join(projectDir, ".kern-pkg-dist", packageName);
  if (mirrorEnabled) {
    await ensureDir(versionsDir);
  }
  await ensureDir(distDir);

  let metadata = {
    name: packageName,
    description: manifest.description || "",
    trusted: false,
    latest: manifest.version,
    versions: {}
  };
  if (mirrorEnabled && (await exists(metadataPath))) metadata = await readJson(metadataPath);

  let version = manifest.version;
  if (args.bump) {
    version = incrementVersion(version, args.bump);
    manifest.version = version;
    await writeJsonAtomic(manifestPath, manifest);
  }
  if (mirrorEnabled && metadata.versions?.[version]) {
    throw new Error(`Version already exists: ${packageName}@${version}`);
  }

  const tarballName = `${packageName}-${version}.tgz`;
  const tarballPath = path.join(distDir, tarballName);
  await createTarball(projectDir, tarballPath);
  const bytes = await fs.readFile(tarballPath);
  const integrity = toIntegrity(sha256Hex(bytes));
  const tarballUrl = pathToFileURL(tarballPath).toString();

  const versionManifestRel = `versions/${version}.json`;
  const versionManifestPath = mirrorEnabled ? path.join(versionsDir, `${version}.json`) : null;
  const versionManifest = {
    name: packageName,
    version,
    main: manifest.main,
    dependencies: manifest.dependencies || {},
    trusted: Boolean(metadata.trusted),
    dist: {
      tarball: tarballUrl,
      shasum: integrity
    }
  };
  if (mirrorEnabled) {
    await writeJsonAtomic(versionManifestPath, versionManifest);
  }

  if (args.public) {
    console.warn("kern-pkg publish: --public is deprecated in API-first mode and is now ignored.");
  }

  const apiBase = getPublishApiBase(args);
  const payload = {
    name: packageName,
    version,
    manifest: {
      name: packageName,
      version,
      main: manifest.main,
      dependencies: manifest.dependencies || {},
      description: manifest.description || ""
    },
    integrity,
    tarballBase64: bytes.toString("base64")
  };
  if (!args.dryRun) {
    const publishResult = await publishToApi(apiBase, payload);
    if (mirrorEnabled && publishResult?.dist?.tarball) {
      versionManifest.dist.tarball = publishResult.dist.tarball;
      await writeJsonAtomic(versionManifestPath, versionManifest);
    }
  }

  if (mirrorEnabled) {
    metadata.latest = version;
    metadata.description = manifest.description || metadata.description || "";
    metadata.versions = metadata.versions || {};
    metadata.versions[version] = { manifest: versionManifestRel };
    await writeJsonAtomic(metadataPath, metadata);

    index.packages = index.packages || {};
    index.packages[packageName] = {
      latest: version,
      metadata: `packages/${packageName}/metadata.json`
    };
    index.generatedAt = new Date().toISOString();
    await writeJsonAtomic(indexPath, index);
  }

  if (args.dryRun) {
    console.log(`Dry run: would publish ${packageName}@${version} to ${apiBase}/api/v1/packages`);
  }

  console.log(`Published ${packageName}@${version}`);
}
