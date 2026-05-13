import fs from "node:fs";
import path from "node:path";
import { remoteTagsCacheFile } from "./paths.js";
import { kargoIoError } from "./cli-error.js";

const DEFAULT_TTL_MS = 10 * 60 * 1000;

function ttlMs() {
  const n = Number(process.env.KARGO_TAG_CACHE_TTL_MS);
  return Number.isFinite(n) && n >= 0 ? n : DEFAULT_TTL_MS;
}

/**
 * @returns {{ tags: string[]; fetchedAt: number } | null}
 */
export function readPersistentTagCache(owner, repo) {
  const p = remoteTagsCacheFile(owner, repo);
  try {
    const raw = fs.readFileSync(p, "utf8");
    const j = JSON.parse(raw);
    if (!j || typeof j !== "object" || !Array.isArray(j.tags)) return null;
    const fetchedAt = Number(j.fetchedAt);
    if (!Number.isFinite(fetchedAt)) return null;
    const age = Date.now() - fetchedAt;
    if (ttlMs() > 0 && age > ttlMs()) return null;
    return { tags: j.tags.map(String), fetchedAt };
  } catch {
    return null;
  }
}

/**
 * @param {string[]} tags
 */
export function writePersistentTagCache(owner, repo, tags) {
  const p = remoteTagsCacheFile(owner, repo);
  try {
    fs.mkdirSync(path.dirname(p), { recursive: true });
    const body = JSON.stringify({ fetchedAt: Date.now(), tags }, null, 0) + "\n";
    fs.writeFileSync(p, body, "utf8");
  } catch (e) {
    throw kargoIoError(`kargo: could not write tag cache ${p}`, e);
  }
}
