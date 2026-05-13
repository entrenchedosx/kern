/**
 * Parse install specs: owner/repo, owner/repo@v1.0.0, https://github.com/owner/repo.git@v1.0.0
 */
import { KargoCliError, EXIT } from "./cli-error.js";

export function parseGithubSpec(spec) {
  const s = spec.trim();
  const at = s.lastIndexOf("@");
  let ref = null;
  let ident = s;
  if (at > 0 && !s.slice(at).match(/^@[0-9a-f]{7,40}$/i)) {
    // @version or @v1.0.0 (not commit-only for URL form ambiguity — keep simple)
    ident = s.slice(0, at);
    ref = s.slice(at + 1) || null;
  }
  let owner;
  let repo;
  const gh = /^https?:\/\/github\.com\/([^/]+)\/([^/.]+)(\.git)?\/?$/i.exec(ident.replace(/#.*$/, ""));
  if (gh) {
    owner = gh[1];
    repo = gh[2];
  } else {
    const slash = ident.indexOf("/");
    if (slash <= 0 || slash === ident.length - 1) {
      throw new KargoCliError(
        `Invalid package spec "${spec}" (want owner/repo or https://github.com/owner/repo)`,
        EXIT.USER
      );
    }
    owner = ident.slice(0, slash);
    repo = ident.slice(slash + 1).replace(/\.git$/i, "");
  }
  if (!owner || !repo || owner.includes("/") || repo.includes("/")) {
    throw new KargoCliError(`Invalid owner/repo in "${spec}"`, EXIT.USER);
  }
  return { owner, repo, tag: ref };
}

export function cloneUrl(owner, repo, token) {
  if (token) {
    return `https://${token}@github.com/${owner}/${repo}.git`;
  }
  return `https://github.com/${owner}/${repo}.git`;
}

/** Tag v1.0.0 -> semver 1.0.0 */
export function tagToSemver(tag) {
  const t = String(tag).trim();
  if (t.startsWith("v") || t.startsWith("V")) return t.slice(1);
  return t;
}

/** Dependency string in kargo.toml: owner/repo or owner/repo@v1.0.0 */
export function parseDepString(val) {
  const s = String(val).trim();
  const at = s.lastIndexOf("@");
  if (at > 0) return parseGithubSpec(`${s.slice(0, at)}@${s.slice(at + 1)}`);
  return parseGithubSpec(s);
}
