import path from "node:path";
import fs from "node:fs/promises";
import fsSync from "node:fs";
import { pathToFileURL, fileURLToPath } from "node:url";

const DEFAULT_REGISTRY_URL =
  "https://raw.githubusercontent.com/kernlang/kern-registry/main/registry/registry.json";
const DEFAULT_API_BASE = "http://127.0.0.1:4873";

function withTimeout(ms) {
  const controller = new AbortController();
  const id = setTimeout(() => controller.abort(), ms);
  return { controller, done: () => clearTimeout(id) };
}

export function getRegistryUrl() {
  if (process.env.KERN_REGISTRY_URL) return process.env.KERN_REGISTRY_URL;
  const cwd = process.cwd();
  const localA = path.resolve(cwd, "registry", "registry.json");
  if (fsSync.existsSync(localA)) return localA;
  const localB = path.resolve(cwd, "kern-registry", "registry", "registry.json");
  if (fsSync.existsSync(localB)) return localB;
  return DEFAULT_REGISTRY_URL;
}

export function getRegistryApiBase() {
  if (process.env.KERN_REGISTRY_API_URL) {
    return String(process.env.KERN_REGISTRY_API_URL).replace(/\/+$/, "");
  }
  if (process.env.KERN_REGISTRY_URL) return null;
  return DEFAULT_API_BASE;
}

export function toUrlMaybe(input) {
  if (!input) return null;
  if (/^https?:\/\//i.test(input)) return new URL(input);
  if (/^file:\/\//i.test(input)) return new URL(input);
  return pathToFileURL(path.resolve(input));
}

export async function readJsonFromUrl(urlLike, timeoutMs = 15000) {
  const url = toUrlMaybe(urlLike);
  if (!url) throw new Error("Invalid URL");
  if (url.protocol === "file:") {
    const content = await fs.readFile(fileURLToPath(url), "utf8");
    return JSON.parse(content);
  }
  const t = withTimeout(timeoutMs);
  try {
    const res = await fetch(url, { signal: t.controller.signal });
    if (!res.ok) throw new Error(`HTTP ${res.status} fetching ${url}`);
    return await res.json();
  } finally {
    t.done();
  }
}

export async function readBufferFromUrl(urlLike, timeoutMs = 20000) {
  const url = toUrlMaybe(urlLike);
  if (!url) throw new Error("Invalid URL");
  if (url.protocol === "file:") {
    return fs.readFile(fileURLToPath(url));
  }
  const t = withTimeout(timeoutMs);
  try {
    const res = await fetch(url, { signal: t.controller.signal });
    if (!res.ok) throw new Error(`HTTP ${res.status} fetching ${url}`);
    return Buffer.from(await res.arrayBuffer());
  } finally {
    t.done();
  }
}

export function resolveRelativeUrl(baseUrlLike, relPath) {
  const base = toUrlMaybe(baseUrlLike);
  if (!base) throw new Error("Invalid base URL");
  return new URL(relPath, base).toString();
}

export async function fetchRegistryIndex() {
  const apiBase = getRegistryApiBase();
  if (apiBase) {
    const registryUrl = `${apiBase}/api/v1/simple`;
    try {
      const index = await readJsonFromUrl(registryUrl);
      return { registryUrl, index, apiBase, mode: "api" };
    } catch {
      // fall through to legacy/static registry path
    }
  }
  const registryUrl = getRegistryUrl();
  const index = await readJsonFromUrl(registryUrl);
  return { registryUrl, index, apiBase: null, mode: "legacy" };
}

export async function fetchPackageMetadata(index, registryUrl, packageName) {
  const entry = index?.packages?.[packageName];
  if (!entry) return null;
  const metadataUrl = resolveRelativeUrl(registryUrl, entry.metadata);
  const metadata = await readJsonFromUrl(metadataUrl);
  return { metadataUrl, metadata, entry };
}

export async function fetchVersionManifest(metadata, metadataUrl, version) {
  const info = metadata?.versions?.[version];
  if (!info) return null;
  const manifestPath = typeof info === "string" ? info : info.manifest;
  if (!manifestPath) throw new Error(`Missing manifest path for ${version}`);
  const manifestUrl = resolveRelativeUrl(metadataUrl, manifestPath);
  const manifest = await readJsonFromUrl(manifestUrl);
  return { manifest, manifestUrl };
}
