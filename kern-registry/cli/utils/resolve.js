import { fetchPackageMetadata, fetchVersionManifest } from "./fetchRegistry.js";
import { maxSatisfyingVersion, satisfiesRange } from "./semver.js";

export async function resolveDependencyGraph({ index, registryUrl, dependencies }) {
  const resolved = {};
  const visiting = new Set();

  async function resolveOne(name, range, stack) {
    if (visiting.has(name)) {
      throw new Error(`Dependency cycle detected: ${[...stack, name].join(" -> ")}`);
    }

    if (resolved[name]) {
      if (!satisfiesRange(resolved[name].version, range)) {
        throw new Error(
          `Version conflict for ${name}: already resolved ${resolved[name].version}, required ${range}`
        );
      }
      return;
    }

    const pkgMeta = await fetchPackageMetadata(index, registryUrl, name);
    if (!pkgMeta) throw new Error(`Package not found: ${name}`);

    const versions = Object.keys(pkgMeta.metadata?.versions || {});
    if (!versions.length) throw new Error(`No versions available for package ${name}`);
    const selected = maxSatisfyingVersion(versions, range || "*");
    if (!selected) {
      throw new Error(`No version of ${name} satisfies ${range}`);
    }

    const manifestResult = await fetchVersionManifest(pkgMeta.metadata, pkgMeta.metadataUrl, selected);
    if (!manifestResult?.manifest) {
      throw new Error(`Missing version manifest for ${name}@${selected}`);
    }

    const manifest = manifestResult.manifest;
    if (!manifest?.dist?.tarball || !manifest?.dist?.shasum) {
      throw new Error(`Invalid dist metadata for ${name}@${selected}`);
    }

    visiting.add(name);
    const depMap = manifest.dependencies || {};
    resolved[name] = {
      name,
      version: selected,
      trusted: Boolean(manifest.trusted || pkgMeta.metadata?.trusted),
      main: manifest.main || "src/index.kn",
      dist: {
        tarball: manifest.dist.tarball,
        shasum: manifest.dist.shasum
      },
      dependencies: depMap
    };

    for (const [depName, depRange] of Object.entries(depMap)) {
      await resolveOne(depName, depRange || "*", [...stack, name]);
    }
    visiting.delete(name);
  }

  for (const [name, range] of Object.entries(dependencies || {})) {
    await resolveOne(name, range || "*", []);
  }

  return resolved;
}
