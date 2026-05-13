import fs from "node:fs/promises";
import path from "node:path";
import { runGit, runGitAllowFail } from "./git.js";
import { parseKargoToml } from "./parse-kargo-toml.js";
import { tagToSemver } from "./spec.js";
import { readConfig } from "./config.js";
import { KargoCliError, EXIT, kargoIoError } from "./cli-error.js";

async function readText(p) {
  try {
    return await fs.readFile(p, "utf8");
  } catch (e) {
    if (e && typeof e === "object" && e !== null && e.code === "ENOENT") {
      throw new KargoCliError("kargo publish: kargo.toml not found in current directory", EXIT.USER);
    }
    throw kargoIoError(`kargo publish: could not read ${p}`, e);
  }
}

export async function runPublish(argv) {
  let tagArg = null;
  let dry = false;
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--tag" && i + 1 < argv.length) tagArg = argv[++i];
    else if (a === "--dry-run") dry = true;
    else throw new KargoCliError(`Unknown publish arg: ${a}`, EXIT.USAGE);
  }
  if (!tagArg) throw new KargoCliError("Usage: kargo publish --tag v1.0.0 [--dry-run]", EXIT.USAGE);

  const cwd = process.cwd();
  const raw = await readText(path.join(cwd, "kargo.toml"));
  if (!String(raw).trim()) {
    throw new KargoCliError("kargo publish: kargo.toml is empty or unreadable", EXIT.USER);
  }
  const pkg = parseKargoToml(raw);
  if (!pkg.name) throw new KargoCliError("kargo.toml: missing name", EXIT.USER);
  if (!pkg.version) throw new KargoCliError("kargo.toml: missing version", EXIT.USER);

  const verFromTag = tagToSemver(tagArg);
  if (verFromTag !== pkg.version) {
    throw new KargoCliError(
      `kargo publish: kargo.toml version "${pkg.version}" must match tag "${tagArg}" (semver: ${verFromTag})`,
      EXIT.USER
    );
  }

  const st = runGitAllowFail(cwd, ["rev-parse", "--is-inside-work-tree"]);
  if (!st.ok) throw new KargoCliError("kargo publish: not a git repository", EXIT.USER);

  if (dry) {
    console.log(`kargo publish (dry-run): would tag ${tagArg} and push`);
    return;
  }

  const tagTest = runGitAllowFail(cwd, ["rev-parse", tagArg]);
  if (tagTest.ok) throw new KargoCliError(`kargo publish: tag ${tagArg} already exists locally`, EXIT.USER);
  runGit(cwd, ["tag", tagArg], process.env);
  runGit(cwd, ["push", "origin", tagArg], process.env);

  const cfg = await readConfig();
  const token = cfg.github?.token || process.env.GITHUB_TOKEN;
  if (token) {
    const remote = runGit(cwd, ["remote", "get-url", "origin"]);
    const m = /github\.com[:/]([^/]+)\/([^/.]+)/i.exec(remote);
    if (m) {
      const owner = m[1];
      const repo = m[2].replace(/\.git$/i, "");
      const url = `https://api.github.com/repos/${owner}/${repo}/releases`;
      const body = JSON.stringify({
        tag_name: tagArg,
        name: tagArg,
        body: `Published with kargo (${pkg.name} ${pkg.version})`
      });
      const res = await fetch(url, {
        method: "POST",
        headers: {
          Accept: "application/vnd.github+json",
          Authorization: `Bearer ${token}`,
          "X-GitHub-Api-Version": "2022-11-28"
        },
        body
      });
      if (!res.ok) {
        const t = await res.text();
        console.warn(`kargo publish: GitHub release API failed (${res.status}): ${t}`);
      } else {
        console.log("kargo publish: created GitHub release");
      }
    }
  }

  console.log(`kargo publish: pushed tag ${tagArg}`);
}
