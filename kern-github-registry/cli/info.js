import { resolvePackageVersion } from "./utils/fetch.js";

export async function runInfo(argv) {
  const name = String(argv[0] || "").trim();
  const range = String(argv[1] || "*").trim();
  if (!name) throw new Error("package name is required");
  const resolved = await resolvePackageVersion(name, range);
  const meta = resolved.metadata;
  console.log(JSON.stringify({
    name,
    latest: meta.latest || null,
    selected: resolved.selected,
    description: meta.description || "",
    versions: Object.keys(meta.versions || {}).sort()
  }, null, 2));
}
