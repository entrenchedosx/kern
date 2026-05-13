import path from "node:path";
import { parseGithubSpec } from "./spec.js";
import { KargoCliError, EXIT } from "./cli-error.js";
import { lockKey, readLock, writeLock } from "./lockfile.js";
import { writePackagePaths } from "./package-paths.js";
import { parseKargoToml } from "./parse-kargo-toml.js";
import fs from "node:fs/promises";

async function readText(p) {
  try {
    return await fs.readFile(p, "utf8");
  } catch {
    return "";
  }
}

async function resolveMainPath(packageRoot) {
  const kernJson = path.join(packageRoot, "kern.json");
  let main = "src/index.kn";
  try {
    const kj = JSON.parse(await readText(kernJson));
    if (kj.main) main = String(kj.main);
  } catch {
    /* default */
  }
  return path.resolve(packageRoot, main);
}

async function readKargoName(packageRoot) {
  const raw = await readText(path.join(packageRoot, "kargo.toml"));
  if (!raw) return "";
  return parseKargoToml(raw).name || "";
}

async function rebuildPackagePaths(projectDir, lock) {
  const map = {};
  for (const [key, ent] of Object.entries(lock.packages || {})) {
    if (!ent?.root) continue;
    let mainAbs;
    try {
      mainAbs = await resolveMainPath(ent.root);
    } catch {
      continue;
    }
    const name = await readKargoName(ent.root);
    const entry = { version: ent.semver, root: ent.root, main: mainAbs };
    map[key] = { ...entry };
    map[`github.com/${key}`] = { ...entry };
    if (name) map[name] = { ...entry };
  }
  await writePackagePaths(projectDir, map);
}

export async function runRemove(argv) {
  let project = process.cwd();
  let spec = null;
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--project" && i + 1 < argv.length) project = path.resolve(argv[++i]);
    else if (!spec) spec = a;
    else throw new KargoCliError(`Unknown argument: ${a}`, EXIT.USAGE);
  }
  if (!spec) throw new KargoCliError("Usage: kargo remove <owner/repo> [--project <dir>]", EXIT.USAGE);

  const { owner, repo } = parseGithubSpec(spec);
  const key = lockKey(owner, repo);
  const lock = await readLock(project);
  if (!lock.packages?.[key]) throw new KargoCliError(`Not installed in kargo.lock: ${key}`, EXIT.USER);

  delete lock.packages[key];
  await writeLock(project, lock);
  await rebuildPackagePaths(project, lock);

  console.log(`kargo: removed ${key} from kargo.lock and refreshed .kern/package-paths.json`);
  console.log(`kargo: package files remain under ~/.kargo/packages (optional: delete that folder to reclaim disk)`);
}
