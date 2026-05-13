import path from "node:path";
import fs from "node:fs/promises";
import { exists, readJson } from "./utils/io.js";
import { readConfig } from "./utils/config.js";
import { createTarball } from "./utils/tarball.js";
import { sha256Hex, toIntegrity } from "./utils/integrity.js";
import { splitRepo, getOrCreateRelease, uploadReleaseAsset, deleteReleaseAssetByName } from "./utils/github.js";
import {
  readRegistryIndex,
  readPackageMetadata,
  writeRegistryIndex,
  writePackageMetadata
} from "./utils/registry.js";
import { incrementVersion } from "./utils/semver.js";

function parseArgs(argv) {
  const out = { dir: process.cwd(), bump: null };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--dir" && i + 1 < argv.length) out.dir = path.resolve(argv[++i]);
    else if (a === "--bump" && i + 1 < argv.length) out.bump = argv[++i];
    else throw new Error(`Unknown publish arg: ${a}`);
  }
  return out;
}

function assertPackageManifest(manifest) {
  if (!manifest?.name) throw new Error("kern.json missing name");
  if (!manifest?.version) throw new Error("kern.json missing version");
  if (!manifest?.main) throw new Error("kern.json missing main");
  if (!/^[a-z][a-z0-9._-]*$/.test(String(manifest.name))) {
    throw new Error("invalid package name (expected /^[a-z][a-z0-9._-]*$/)");
  }
  if (!/^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$/.test(String(manifest.version))) {
    throw new Error("invalid package version (expected semver)");
  }
  if (String(manifest.main).includes("..") || path.isAbsolute(String(manifest.main))) {
    throw new Error("invalid main path");
  }
}

export async function runPublish(argv) {
  const args = parseArgs(argv);
  const cfg = await readConfig();
  if (!cfg.githubToken) throw new Error("Not logged in. Run: kern-gh-pkg login --token <PAT> --repo <owner/repo>");
  if (!cfg.registryRepo) throw new Error("No registry repo set. Run: kern-gh-pkg login --repo <owner/repo>");
  const { owner, repo } = splitRepo(cfg.registryRepo);

  const manifestPath = path.join(args.dir, "kern.json");
  if (!(await exists(manifestPath))) throw new Error("kern.json not found");
  const manifest = await readJson(manifestPath);
  assertPackageManifest(manifest);

  const packageName = manifest.name;
  let version = manifest.version;
  if (args.bump) version = incrementVersion(version, args.bump);

  const releaseTag = `${packageName}-v${version}`;
  const release = await getOrCreateRelease(owner, repo, releaseTag, cfg.githubToken);

  const tarName = `${packageName}-${version}.tgz`;
  const tarPath = path.join(args.dir, ".kern-publish", tarName);
  await createTarball(args.dir, tarPath);
  const bytes = await fs.readFile(tarPath);
  if (bytes.length > 200 * 1024 * 1024) {
    throw new Error("package tarball exceeds max allowed size (200MB)");
  }
  const integrity = toIntegrity(sha256Hex(bytes));
  await deleteReleaseAssetByName(owner, repo, release, tarName, cfg.githubToken);
  const uploaded = await uploadReleaseAsset(owner, repo, release.id, tarName, bytes, cfg.githubToken);
  const tarballUrl = uploaded.browser_download_url;

  const pkgMetaRes = await readPackageMetadata(owner, repo, cfg.registryRef, cfg.githubToken, packageName);
  const metadata = pkgMetaRes.metadata;
  if (metadata.versions?.[version]) throw new Error(`Version already exists: ${packageName}@${version}`);

  metadata.name = packageName;
  metadata.description = manifest.description || metadata.description || "";
  metadata.latest = version;
  metadata.trusted = Boolean(metadata.trusted);
  metadata.versions = metadata.versions || {};
  metadata.versions[version] = {
    version,
    main: manifest.main,
    dependencies: manifest.dependencies || {},
    dist: { tarball: tarballUrl, shasum: integrity }
  };
  await writePackageMetadata(owner, repo, cfg.githubToken, packageName, metadata, pkgMetaRes.sha);

  const idxRes = await readRegistryIndex(owner, repo, cfg.registryRef, cfg.githubToken);
  idxRes.index.generatedAt = new Date().toISOString();
  idxRes.index.packages = idxRes.index.packages || {};
  idxRes.index.packages[packageName] = {
    latest: version,
    metadata: `registry/packages/${packageName}.json`
  };
  await writeRegistryIndex(owner, repo, cfg.githubToken, idxRes.index, idxRes.sha);

  console.log(`Published ${packageName}@${version}`);
  console.log(`Tarball: ${tarballUrl}`);
}
