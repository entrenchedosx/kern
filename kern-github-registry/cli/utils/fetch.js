import { maxSatisfyingVersion } from "./semver.js";
import { readConfig } from "./config.js";
import { splitRepo } from "./github.js";

export function registryRawBase(owner, repo, ref) {
  return `https://raw.githubusercontent.com/${owner}/${repo}/${ref}`;
}

export async function fetchRegistryIndex() {
  const cfg = await readConfig();
  if (!cfg.registryRepo) throw new Error("No registry repo configured. Run: kern-gh-pkg login --repo owner/repo");
  const { owner, repo } = splitRepo(cfg.registryRepo);
  const ref = cfg.registryRef || "main";
  const url = `${registryRawBase(owner, repo, ref)}/registry/index.json`;
  const res = await fetch(url, { headers: { "user-agent": "kern-github-registry-cli" } });
  if (!res.ok) throw new Error(`Failed to fetch registry index (${res.status})`);
  return { owner, repo, ref, index: await res.json() };
}

export async function fetchPackageMetadata(owner, repo, ref, packageName) {
  const url = `${registryRawBase(owner, repo, ref)}/registry/packages/${encodeURIComponent(packageName)}.json`;
  const res = await fetch(url, { headers: { "user-agent": "kern-github-registry-cli" } });
  if (!res.ok) throw new Error(`Package not found: ${packageName}`);
  return res.json();
}

export async function resolvePackageVersion(packageName, range = "*") {
  if (!/^[a-z][a-z0-9._-]*$/.test(String(packageName || ""))) {
    throw new Error(`invalid package name: ${packageName}`);
  }
  const { owner, repo, ref } = await fetchRegistryIndex();
  const meta = await fetchPackageMetadata(owner, repo, ref, packageName);
  const versions = Object.keys(meta.versions || {});
  if (!versions.length) throw new Error(`No versions available for ${packageName}`);
  const selected = maxSatisfyingVersion(versions, range);
  if (!selected) throw new Error(`No version of ${packageName} matches ${range}`);
  const node = meta.versions[selected] || {};
  if (!node?.dist?.tarball || !node?.dist?.shasum) {
    throw new Error(`invalid registry metadata for ${packageName}@${selected}`);
  }
  return { owner, repo, ref, metadata: meta, selected };
}
