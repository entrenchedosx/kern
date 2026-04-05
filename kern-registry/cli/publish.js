import path from "node:path";
import fs from "node:fs/promises";
import { pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { createTarball } from "./utils/zip.js";
import { ensureDir, exists, readJson, writeJsonAtomic } from "./utils/io.js";
import { incrementVersion } from "./utils/semver.js";
import { sha256Hex, toIntegrity } from "./utils/integrity.js";
import { getManifestPath } from "./utils/paths.js";

function parseArgs(argv) {
  const out = { dir: process.cwd(), bump: null, public: false, dryRun: false };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--dir" && i + 1 < argv.length) {
      out.dir = path.resolve(argv[++i]);
    } else if (a === "--bump" && i + 1 < argv.length) {
      out.bump = argv[++i];
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

function runGh(args) {
  const result = spawnSync("gh", args, { encoding: "utf8", stdio: "pipe" });
  if (result.status !== 0) {
    const detail = (result.stderr || result.stdout || "").trim();
    throw new Error(`gh ${args.join(" ")} failed${detail ? `: ${detail}` : ""}`);
  }
  return (result.stdout || "").trim();
}

function ensureReleaseTag(repo, tag) {
  const view = spawnSync("gh", ["release", "view", tag, "--repo", repo], { encoding: "utf8", stdio: "pipe" });
  if (view.status === 0) return;
  runGh([
    "release",
    "create",
    tag,
    "--repo",
    repo,
    "--title",
    tag,
    "--notes",
    `Automated release for ${tag}`
  ]);
}

export async function runPublish(argv) {
  const args = parseArgs(argv);
  const projectDir = args.dir;
  const manifestPath = getManifestPath(projectDir);
  if (!(await exists(manifestPath))) throw new Error("kern.json not found");
  const manifest = await readJson(manifestPath);
  await validatePackage(projectDir, manifest);

  const registryRoot = resolveRegistryRoot();
  const indexPath = path.join(registryRoot, "registry", "registry.json");
  if (!(await exists(indexPath))) {
    throw new Error(`registry.json not found under ${registryRoot}`);
  }
  const index = await readJson(indexPath);
  const packageName = manifest.name;
  const pkgDir = path.join(registryRoot, "registry", "packages", packageName);
  const metadataPath = path.join(pkgDir, "metadata.json");
  const versionsDir = path.join(pkgDir, "versions");
  const distDir = path.join(registryRoot, "registry", "dist", packageName);
  await ensureDir(versionsDir);
  await ensureDir(distDir);

  let metadata = {
    name: packageName,
    description: manifest.description || "",
    trusted: false,
    latest: manifest.version,
    versions: {}
  };
  if (await exists(metadataPath)) metadata = await readJson(metadataPath);

  let version = manifest.version;
  if (args.bump) {
    version = incrementVersion(version, args.bump);
    manifest.version = version;
    await writeJsonAtomic(manifestPath, manifest);
  }
  if (metadata.versions?.[version]) {
    throw new Error(`Version already exists: ${packageName}@${version}`);
  }

  const tarballName = `${packageName}-${version}.tgz`;
  const tarballPath = path.join(distDir, tarballName);
  await createTarball(projectDir, tarballPath);
  const bytes = await fs.readFile(tarballPath);
  const integrity = toIntegrity(sha256Hex(bytes));
  const tarballUrl = pathToFileURL(tarballPath).toString();

  const versionManifestRel = `versions/${version}.json`;
  const versionManifestPath = path.join(versionsDir, `${version}.json`);
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
  await writeJsonAtomic(versionManifestPath, versionManifest);

  let publicReleaseUrl = "";
  if (args.public) {
    const ghRepo = process.env.KERN_REGISTRY_GH_REPO || "";
    if (!ghRepo) {
      throw new Error("KERN_REGISTRY_GH_REPO is required for --public (format: owner/repo)");
    }
    const tag = `${packageName}-v${version}`;
    if (!args.dryRun) {
      ensureReleaseTag(ghRepo, tag);
      runGh(["release", "upload", tag, tarballPath, "--repo", ghRepo, "--clobber"]);
      publicReleaseUrl = `https://github.com/${ghRepo}/releases/download/${tag}/${tarballName}`;
      versionManifest.dist.tarball = publicReleaseUrl;
      await writeJsonAtomic(versionManifestPath, versionManifest);
    } else {
      publicReleaseUrl = `https://github.com/${ghRepo}/releases/download/${tag}/${tarballName}`;
    }
  }

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

  if (args.public) {
    const tag = `${packageName}-v${version}`;
    const releaseManifestPath = path.join(distDir, `${packageName}-${version}.github-release.json`);
    const releaseManifest = {
      tag,
      title: `${packageName}@${version}`,
      notes: `Automated publish metadata for ${packageName}@${version}`,
      assets: [
        {
          name: tarballName,
          path: tarballPath,
          integrity
        }
      ],
      uploaded: Boolean(publicReleaseUrl),
      releaseUrl: publicReleaseUrl || null,
      hint: publicReleaseUrl
        ? (args.dryRun
            ? "Dry run: no gh side effects executed; this is the expected release URL."
            : "Tarball uploaded with gh and version manifest updated to GitHub release URL.")
        : "Upload this tarball to a GitHub Release using gh, then replace dist.tarball with the release URL."
    };
    await writeJsonAtomic(releaseManifestPath, releaseManifest);
    console.log(`Prepared public release metadata: ${releaseManifestPath}`);
    if (publicReleaseUrl && !args.dryRun) {
      console.log(`Uploaded release asset: ${publicReleaseUrl}`);
    }
    if (args.dryRun) {
      console.log(`Dry run: skipped gh release create/upload for ${packageName}@${version}`);
      console.log(`Dry run target release URL: ${publicReleaseUrl}`);
    }
  }

  console.log(`Published ${packageName}@${version}`);
}
