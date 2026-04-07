import fs from "node:fs/promises";
import path from "node:path";
import { projectKargoLock } from "./paths.js";
import { KargoCliError, EXIT, ERR, kargoIoError } from "./cli-error.js";

/** Top-level lock keys (fixed order for byte-stable JSON). */
const LOCK_ROOT_KEYS = ["lockVersion", "packages"];
const LOCK_ROOT_SET = new Set(LOCK_ROOT_KEYS);

/** Per-package entry keys (fixed order). Omitted keys are skipped. */
const PACKAGE_ENTRY_KEY_ORDER = [
  "commit_sha",
  "resolved_constraints",
  "resolved_from",
  "resolved_tag",
  "resolved_version_range",
  "resolved_version_range_normalized",
  "root",
  "semver"
];
const PACKAGE_ENTRY_SET = new Set(PACKAGE_ENTRY_KEY_ORDER);

/**
 * Build a deep-sorted lock object for deterministic JSON output (CI diffs).
 * @param {Record<string, unknown>} lock
 */
export function sortLockForWrite(lock) {
  const pkgIn = lock.packages && typeof lock.packages === "object" ? lock.packages : {};
  const pkgKeys = Object.keys(pkgIn).sort((a, b) => a.localeCompare(b));
  const packages = {};
  for (const k of pkgKeys) {
    const ent = pkgIn[k];
    if (!ent || typeof ent !== "object") continue;
    const sortedEntry = {};
    for (const field of PACKAGE_ENTRY_KEY_ORDER) {
      if (!Object.prototype.hasOwnProperty.call(ent, field)) continue;
      sortedEntry[field] = ent[field];
    }
    const extra = Object.keys(ent)
      .filter((x) => !PACKAGE_ENTRY_SET.has(x))
      .sort((a, b) => a.localeCompare(b));
    for (const x of extra) {
      sortedEntry[x] = ent[x];
    }
    packages[k] = sortedEntry;
  }

  const out = {};
  for (const field of LOCK_ROOT_KEYS) {
    if (field === "packages") out.packages = packages;
    else if (Object.prototype.hasOwnProperty.call(lock, field)) out[field] = lock[field];
  }
  const extraRoot = Object.keys(lock)
    .filter((x) => !LOCK_ROOT_SET.has(x))
    .sort((a, b) => a.localeCompare(b));
  for (const x of extraRoot) {
    out[x] = lock[x];
  }
  return out;
}

export async function readLock(projectDir) {
  const p = projectKargoLock(projectDir);
  let raw;
  try {
    raw = await fs.readFile(p, "utf8");
  } catch (e) {
    if (e && typeof e === "object" && e !== null && e.code === "ENOENT") return { lockVersion: 1, packages: {} };
    throw kargoIoError(`kargo: could not read ${p}`, e);
  }
  try {
    const j = JSON.parse(raw);
    if (j === null || typeof j !== "object" || Array.isArray(j)) {
      throw new KargoCliError(
        `${p} must contain a JSON object (got ${j === null ? "null" : Array.isArray(j) ? "array" : typeof j})`,
        EXIT.USER,
        ERR.INVALID_LOCK_JSON
      );
    }
    const pk = j.packages;
    if (pk != null && (typeof pk !== "object" || Array.isArray(pk))) {
      throw new KargoCliError(`${p}: "packages" must be a JSON object`, EXIT.USER, ERR.INVALID_LOCK_JSON);
    }
    if (!j.packages) j.packages = {};
    if (j.lockVersion == null) j.lockVersion = 1;
    return j;
  } catch (e) {
    if (e instanceof KargoCliError) throw e;
    const msg = e && typeof e.message === "string" ? e.message : String(e);
    throw new KargoCliError(`invalid JSON in ${p}: ${msg}`, EXIT.USER, ERR.INVALID_LOCK_JSON);
  }
}

export async function writeLock(projectDir, lock) {
  const p = projectKargoLock(projectDir);
  try {
    await fs.mkdir(path.dirname(p), { recursive: true });
    const body = JSON.stringify(sortLockForWrite(lock), null, 2) + "\n";
    await fs.writeFile(p, body, "utf8");
  } catch (e) {
    throw kargoIoError(`kargo: could not write ${p}`, e);
  }
}

export function lockKey(owner, repo) {
  return `${owner.toLowerCase()}/${repo.toLowerCase()}`;
}
