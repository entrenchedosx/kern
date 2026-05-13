import path from "node:path";
import fs from "node:fs/promises";
import { ensureDir, exists, readJson, writeJsonAtomic } from "./utils/io.js";
import { resolvePackageVersion } from "./utils/fetch.js";
import { verifyIntegrity } from "./utils/integrity.js";
import { extractTarball } from "./utils/tarball.js";

function parseInstallSpec(spec) {
  const s = String(spec || "").trim();
  if (!s) return null;
  const at = s.lastIndexOf("@");
  if (at <= 0) return { name: s, range: "*" };
  return { name: s.slice(0, at), range: s.slice(at + 1) || "*" };
}

function assertPackageName(name) {
  if (!/^[a-z][a-z0-9._-]*$/.test(String(name || ""))) {
    throw new Error(`invalid package name: ${name}`);
  }
}

function parseArgs(argv) {
  const out = { project: process.cwd(), update: false, spec: null };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--project" && i + 1 < argv.length) out.project = path.resolve(argv[++i]);
    else if (a === "--update") out.update = true;
    else if (!out.spec) out.spec = parseInstallSpec(a);
    else throw new Error(`Unknown install arg: ${a}`);
  }
  return out;
}

async function readManifest(manifestPath) {
  if (!(await exists(manifestPath))) {
    return { name: "kern-project", version: "1.0.0", main: "main.kn", dependencies: {} };
  }
  const manifest = await readJson(manifestPath);
  if (!manifest.dependencies || typeof manifest.dependencies !== "object" || Array.isArray(manifest.dependencies)) {
    manifest.dependencies = {};
  }
  return manifest;
}

async function resolveGraph(rootDeps) {
  const resolved = {};
  const visiting = new Set();
  async function visit(name, range, trail) {
    assertPackageName(name);
    if (visiting.has(name)) throw new Error(`Dependency cycle: ${[...trail, name].join(" -> ")}`);
    if (resolved[name]) return;
    visiting.add(name);
    const r = await resolvePackageVersion(name, range);
    const ver = r.selected;
    const node = r.metadata.versions[ver];
    resolved[name] = {
      version: ver,
      main: node.main || "src/index.kn",
      dependencies: node.dependencies || {},
      dist: node.dist
    };
    for (const [dn, dr] of Object.entries(node.dependencies || {})) {
      await visit(dn, dr || "*", [...trail, name]);
    }
    visiting.delete(name);
  }
  for (const [name, range] of Object.entries(rootDeps || {})) {
    await visit(name, range || "*", []);
  }
  return resolved;
}

async function fetchBuffer(url) {
  const parsed = new URL(url);
  if (parsed.protocol !== "https:") {
    throw new Error(`Refusing non-HTTPS package URL: ${url}`);
  }
  const host = parsed.hostname.toLowerCase();
  if (host !== "github.com" && host !== "objects.githubusercontent.com") {
    throw new Error(`Refusing untrusted package host: ${host}`);
  }
  const res = await fetch(url, { headers: { "user-agent": "kern-github-registry-cli" } });
  if (!res.ok) throw new Error(`Failed to download tarball (${res.status}) ${url}`);
  const len = Number(res.headers.get("content-length") || "0");
  const max = 200 * 1024 * 1024;
  if (len > max) throw new Error("tarball too large");
  const bytes = Buffer.from(await res.arrayBuffer());
  if (bytes.length > max) throw new Error("tarball too large");
  return bytes;
}

async function installResolved(projectDir, resolved) {
  const kernRoot = path.join(projectDir, ".kern");
  const packagesRoot = path.join(kernRoot, "packages");
  const cacheRoot = path.join(process.env.USERPROFILE || process.env.HOME || ".", ".kern", "cache");
  await ensureDir(packagesRoot);
  await ensureDir(cacheRoot);
  const paths = {};

  for (const [name, pkg] of Object.entries(resolved)) {
    const cacheName = `${name}-${pkg.version}-${String(pkg.dist.shasum || "").replace(/^sha256-/, "").slice(0, 16)}.tgz`;
    const cacheFile = path.join(cacheRoot, cacheName);
    let bytes = null;
    if (await exists(cacheFile)) {
      bytes = await fs.readFile(cacheFile);
    } else {
      bytes = await fetchBuffer(pkg.dist.tarball);
      await fs.writeFile(cacheFile, bytes);
    }
    if (!verifyIntegrity(bytes, pkg.dist.shasum)) {
      throw new Error(`Integrity verification failed for ${name}@${pkg.version}`);
    }

    const installRoot = path.join(packagesRoot, name, pkg.version);
    await fs.rm(installRoot, { recursive: true, force: true });
    await ensureDir(installRoot);
    await extractTarball(cacheFile, installRoot);
    paths[name] = {
      version: pkg.version,
      root: installRoot,
      main: path.join(installRoot, pkg.main || "src/index.kn")
    };
  }

  return paths;
}

export async function runInstall(argv) {
  const args = parseArgs(argv);
  const project = args.project;
  const manifestPath = path.join(project, "kern.json");
  const lockPath = path.join(project, "kern.lock");
  const packagePathsPath = path.join(project, ".kern", "package-paths.json");

  const manifest = await readManifest(manifestPath);
  if (args.spec) manifest.dependencies[args.spec.name] = args.spec.range;
  await writeJsonAtomic(manifestPath, manifest);

  const resolved = await resolveGraph(manifest.dependencies);
  const paths = await installResolved(project, resolved);

  const lock = {
    lockVersion: 2,
    registry: "github",
    packages: {}
  };
  for (const [name, pkg] of Object.entries(resolved)) {
    lock.packages[name] = {
      version: pkg.version,
      resolved: pkg.dist.tarball,
      integrity: pkg.dist.shasum,
      dependencies: pkg.dependencies || {}
    };
  }
  await writeJsonAtomic(lockPath, lock);
  await writeJsonAtomic(packagePathsPath, { version: 1, packages: paths });
  console.log(`Installed ${Object.keys(resolved).length} package(s).`);
}
