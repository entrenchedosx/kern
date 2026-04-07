import { spawnSync } from "node:child_process";
import { normalizeTagList } from "./ranges.js";
import { KargoCliError, EXIT } from "./cli-error.js";

export function runGit(cwd, args, env = process.env) {
  const r = spawnSync("git", args, {
    cwd,
    encoding: "utf8",
    env,
    shell: false
  });
  if (r.error) throw new KargoCliError(`[git] ${r.error.message} (is git on PATH?)`, EXIT.USER);
  if (r.status !== 0) {
    const detail = (r.stderr || r.stdout || "").trim() || `exited ${r.status}`;
    throw new KargoCliError(`[git] ${detail}`, EXIT.USER);
  }
  return (r.stdout || "").trim();
}

export function runGitAllowFail(cwd, args, env = process.env) {
  const r = spawnSync("git", args, {
    cwd,
    encoding: "utf8",
    env,
    shell: false
  });
  return { ok: r.status === 0, out: (r.stdout || "").trim(), err: (r.stderr || "").trim() };
}

/**
 * List remote tags (vX.Y.Z and optional vX.Y.Z-prerelease), deduped and semver-sorted ascending.
 * Order is deterministic for the same ls-remote output.
 */
export function listRemoteSemverTags(cloneUrl) {
  const out = runGit(process.cwd(), ["ls-remote", "--tags", cloneUrl]);
  const tags = [];
  for (const line of out.split("\n")) {
    const t = line.trim();
    if (!t || t.includes("^{}")) continue;
    const m =
      /^[0-9a-f]+\s+refs\/tags\/(v\d+\.\d+\.\d+(?:-[0-9A-Za-z.]+)?)\s*$/i.exec(t);
    if (m) tags.push(m[1]);
  }
  return normalizeTagList(tags);
}
