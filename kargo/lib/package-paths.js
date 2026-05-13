import fs from "node:fs/promises";
import path from "node:path";
import { projectKernPackagePaths } from "./paths.js";
import { KargoCliError, EXIT, ERR, kargoIoError } from "./cli-error.js";

export async function readPackagePaths(projectDir) {
  const p = projectKernPackagePaths(projectDir);
  let raw;
  try {
    raw = await fs.readFile(p, "utf8");
  } catch (e) {
    if (e && typeof e === "object" && e !== null && e.code === "ENOENT") return {};
    throw kargoIoError(`kargo: could not read ${p}`, e);
  }
  try {
    const j = JSON.parse(raw);
    if (j === null || typeof j !== "object" || Array.isArray(j)) {
      throw new KargoCliError(
        `${p} must contain a JSON object (got ${j === null ? "null" : Array.isArray(j) ? "array" : typeof j})`,
        EXIT.USER,
        ERR.INVALID_PACKAGE_PATHS_JSON
      );
    }
    return j;
  } catch (e) {
    if (e instanceof KargoCliError) throw e;
    const msg = e && typeof e.message === "string" ? e.message : String(e);
    throw new KargoCliError(`invalid JSON in ${p}: ${msg}`, EXIT.USER, ERR.INVALID_PACKAGE_PATHS_JSON);
  }
}

export async function writePackagePaths(projectDir, map) {
  const p = projectKernPackagePaths(projectDir);
  try {
    await fs.mkdir(path.dirname(p), { recursive: true });
    await fs.writeFile(p, JSON.stringify(map, null, 2) + "\n", "utf8");
  } catch (e) {
    throw kargoIoError(`kargo: could not write ${p}`, e);
  }
}

/**
 * Register package for Kern imports: bare `name`, `owner/repo`, and `github.com/owner/repo`.
 */
export function makePathEntry({ root, semver, mainAbs }) {
  return {
    version: semver,
    root,
    main: mainAbs
  };
}

export async function mergeKernPaths(projectDir, entries) {
  const cur = await readPackagePaths(projectDir);
  for (const e of entries) {
    const { keys, root, semver, mainAbs } = e;
    const val = makePathEntry({ root, semver, main: mainAbs });
    for (const k of keys) {
      cur[k] = { ...val };
    }
  }
  await writePackagePaths(projectDir, cur);
}
