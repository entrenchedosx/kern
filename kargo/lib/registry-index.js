/**
 * Kern registry index fetch (aligned with kern-registry/cli/utils/fetchRegistry.js).
 * Used by `kargo search` so queries hit package names (e.g. sec.*), not GitHub repos.
 */
import path from "node:path";
import fs from "node:fs/promises";
import fsSync from "node:fs";
import { homedir } from "node:os";
import { pathToFileURL, fileURLToPath } from "node:url";

const DEFAULT_REGISTRY_URL =
  "https://raw.githubusercontent.com/kernlang/kern-registry/main/registry/registry.json";
const DEFAULT_API_BASE = "http://127.0.0.1:4873";

function withTimeout(ms) {
  const controller = new AbortController();
  const id = setTimeout(() => controller.abort(), ms);
  return { controller, done: () => clearTimeout(id) };
}

async function readRegistryAuthConfig() {
  const p = path.join(homedir(), ".kern", "registry-auth.json");
  try {
    const raw = await fs.readFile(p, "utf8");
    const j = JSON.parse(raw);
    const apiUrl = String(j?.apiUrl || "").trim().replace(/\/+$/, "");
    return { apiUrl };
  } catch (e) {
    if (e && typeof e === "object" && e.code === "ENOENT") return { apiUrl: "" };
    return { apiUrl: "" };
  }
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

export async function getEffectiveApiBase() {
  if (process.env.KERN_REGISTRY_API_URL) {
    return String(process.env.KERN_REGISTRY_API_URL).replace(/\/+$/, "");
  }
  const cfg = await readRegistryAuthConfig();
  if (cfg.apiUrl) return cfg.apiUrl;
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

export function resolveRelativeUrl(baseUrlLike, relPath) {
  const base = toUrlMaybe(baseUrlLike);
  if (!base) throw new Error("Invalid base URL");
  return new URL(relPath, base).toString();
}

export async function fetchRegistryIndex() {
  const apiBase = await getEffectiveApiBase();
  if (apiBase) {
    const registryUrl = `${apiBase}/api/v1/simple`;
    try {
      const index = await readJsonFromUrl(registryUrl);
      return { registryUrl, index, apiBase, mode: "api" };
    } catch {
      /* fall through to static registry.json */
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
