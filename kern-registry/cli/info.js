import { fetchPackageMetadata, fetchRegistryIndex } from "./utils/fetchRegistry.js";
import { maxSatisfyingVersion } from "./utils/semver.js";

export async function runInfo(argv) {
  let api = null;
  const rest = [];
  for (let i = 0; i < argv.length; i += 1) {
    if (argv[i] === "--api" && i + 1 < argv.length) {
      api = String(argv[++i]).replace(/\/+$/, "");
    } else {
      rest.push(argv[i]);
    }
  }
  if (api) process.env.KERN_REGISTRY_API_URL = api;
  const packageName = rest[0];
  const range = rest[1] || "*";
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
