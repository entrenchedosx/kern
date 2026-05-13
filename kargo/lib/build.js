import { spawnSync } from "node:child_process";
import path from "node:path";
import fs from "node:fs/promises";
import { kargoVerbose } from "./cli-env.js";
import { KargoCliError, EXIT } from "./cli-error.js";

export async function runBuild(argv) {
  let project = process.cwd();
  const rest = [];
  for (let i = 0; i < argv.length; i += 1) {
    if (argv[i] === "--project" && i + 1 < argv.length) project = path.resolve(argv[++i]);
    else rest.push(argv[i]);
  }
  let entry = rest[0] || "src/main.kn";
  if (!rest[0]) {
    try {
      const kj = JSON.parse(await fs.readFile(path.join(project, "kern.json"), "utf8"));
      if (kj.main) entry = String(kj.main);
    } catch {
      /* default */
    }
  }
  const r = spawnSync("kern", ["--check", entry], {
    cwd: project,
    stdio: "inherit",
    shell: true
  });
  if (r.error) throw new KargoCliError(`[kern] ${r.error.message || r.error} (is kern on PATH?)`, EXIT.USER);
  const code = r.status === null ? 1 : r.status;
  if (code !== 0 && kargoVerbose()) {
    console.error(`kargo: kern --check exited ${code} (entry: ${entry})`);
  }
  process.exitCode = code;
}

export async function runRun(argv) {
  let project = process.cwd();
  const rest = [];
  for (let i = 0; i < argv.length; i += 1) {
    if (argv[i] === "--project" && i + 1 < argv.length) project = path.resolve(argv[++i]);
    else rest.push(argv[i]);
  }
  if (!rest.length) {
    try {
      const kj = JSON.parse(await fs.readFile(path.join(project, "kern.json"), "utf8"));
      if (kj.main) rest.push(String(kj.main));
    } catch {
      throw new KargoCliError('kargo run: pass a .kn file or add kern.json with "main"', EXIT.USAGE);
    }
  }
  const r = spawnSync("kern", rest, { cwd: project, stdio: "inherit", shell: true });
  if (r.error) throw new KargoCliError(`[kern] ${r.error.message || r.error} (is kern on PATH?)`, EXIT.USER);
  const code = r.status === null ? 1 : r.status;
  if (code !== 0 && kargoVerbose()) {
    console.error(`kargo: kern exited ${code} (args: ${rest.join(" ")})`);
  }
  process.exitCode = code;
}
