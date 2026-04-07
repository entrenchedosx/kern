/**
 * Dependency declarations are string-based today. Future: table form for git rev pins, e.g.
 *   dep = { git = "owner/repo", rev = "abc123" }
 * (parser + resolver will treat rev as authoritative over tags.)
 */
import { parseGithubSpec, tagToSemver } from "./spec.js";
import { lockKey } from "./lockfile.js";
import { classifyVersionRef, constraintRawFromClassified } from "./ranges.js";
import { KargoCliError, EXIT } from "./cli-error.js";

const COMMIT_RE = /^[0-9a-f]{7,40}$/i;

export function isCommitHash(s) {
  return COMMIT_RE.test(String(s).trim());
}

/** owner/repo or https://github.com/... without @version */
export function parseGithubIdent(ident) {
  const id = String(ident).trim().replace(/\.git$/i, "");
  const x = parseGithubSpec(`${id}@v0.0.0`);
  return { owner: x.owner, repo: x.repo };
}

/**
 * kargo.toml [dependencies] line: key may be owner/repo or alias; value is range or owner/repo@...
 * @returns {{ owner: string; repo: string; depKey: string; constraintRaw: string }}
 */
export function parseDepEntry(depKey, depVal) {
  const k = depKey.trim();
  const v = String(depVal).trim();

  if (k.includes("/")) {
    const { owner, repo } = parseGithubIdent(k);
    const constraintRaw = v === "" ? "*" : normalizeConstraintValue(v);
    return { owner, repo, depKey: lockKey(owner, repo), constraintRaw };
  }

  if (!v.includes("/")) {
    throw new KargoCliError(
      `kargo: dependency "${k}" must use owner/repo as the key, or the value must contain owner/repo (got "${v}")`,
      EXIT.USER
    );
  }

  return parseDepValueSpec(v);
}

/**
 * Value-only form: "owner/repo", "owner/repo@v1.0.0", "owner/repo@^1.2.0"
 */
export function parseDepValueSpec(value) {
  const v = String(value).trim();
  const at = v.lastIndexOf("@");
  let ident;
  let refPart = null;
  if (at > 0) {
    refPart = v.slice(at + 1);
    if (isCommitHash(refPart)) {
      throw new KargoCliError(
        `kargo: git commit pins in dependencies are not supported (use a semver tag): "${v}"`,
        EXIT.USER
      );
    }
    ident = v.slice(0, at);
  } else {
    ident = v;
  }

  const { owner, repo } = parseGithubIdent(ident);
  const classified = refPart == null ? { kind: "latest" } : classifyVersionRef(refPart);
  const constraintRaw = constraintRawFromClassified(classified);
  return { owner, repo, depKey: lockKey(owner, repo), constraintRaw };
}

function normalizeConstraintValue(v) {
  assertValidConstraintString(v);
  return v.trim();
}

function assertValidConstraintString(v) {
  const s = v.trim();
  if (s === "*" || s === "") return;
  classifyVersionRef(s);
}

/** @param {Record<string, string>} deps */
export function parseDependencyDeclarations(deps) {
  const out = [];
  for (const [key, val] of Object.entries(deps || {})) {
    out.push(parseDepEntry(key, val));
  }
  return out;
}

/**
 * CLI install spec → semver constraint for the root package
 * @param {{ owner: string; repo: string; tag: string | null }} parsed
 */
export function constraintFromInstallSpec(parsed) {
  if (!parsed.tag) return "*";
  const classified = classifyVersionRef(parsed.tag);
  return constraintRawFromClassified(classified);
}
