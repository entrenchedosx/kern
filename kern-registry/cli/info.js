import { fetchPackageMetadata, fetchRegistryIndex } from "./utils/fetchRegistry.js";
import { maxSatisfyingVersion } from "./utils/semver.js";

export async function runInfo(argv) {
  const packageName = argv[0];
  const range = argv[1] || "*";
  if (!packageName) throw new Error("package name is required");

  const { registryUrl, index } = await fetchRegistryIndex();
  const pkg = await fetchPackageMetadata(index, registryUrl, packageName);
  if (!pkg) throw new Error(`Package not found: ${packageName}`);

  const versions = Object.keys(pkg.metadata?.versions || {}).sort();
  const selected = maxSatisfyingVersion(versions, range);

  const out = {
    name: pkg.metadata.name || packageName,
    description: pkg.metadata.description || "",
    trusted: Boolean(pkg.metadata.trusted),
    latest: pkg.metadata.latest || null,
    selected,
    versions
  };
  console.log(JSON.stringify(out, null, 2));
}
