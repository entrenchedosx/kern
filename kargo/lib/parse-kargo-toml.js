/**
 * Minimal kargo.toml parser (string values, [dependencies] table).
 * Sufficient for name, version, description, author, and dep lines.
 */

function stripComment(line) {
  const q = line.indexOf("#");
  if (q < 0) return line.trim();
  return line.slice(0, q).trim();
}

export function parseKargoToml(text) {
  const out = {
    name: "",
    version: "",
    description: "",
    author: "",
    dependencies: {},
    /**
     * Resolver options:
     * - allow_prerelease: use prerelease tags only when no stable satisfies (default true).
     * - resolution_mode: "latest" (resolve from remote tags) or "locked" (use kargo.lock only).
     */
    kargo: { allow_prerelease: true, resolution_mode: "latest", strict: false }
  };
  let section = "";
  for (const rawLine of text.split(/\r?\n/)) {
    const line = stripComment(rawLine);
    if (!line) continue;
    const sec = /^\[([^\]]+)\]\s*$/.exec(line);
    if (sec) {
      section = sec[1].trim().toLowerCase();
      continue;
    }
    const kv = /^([A-Za-z0-9_-]+)\s*=\s*"([^"]*)"\s*$/.exec(line);
    if (kv) {
      const k = kv[1];
      const v = kv[2];
      if (section === "dependencies") {
        out.dependencies[k] = v;
      } else if (section === "kargo") {
        if (k === "allow_prerelease") {
          const low = v.toLowerCase();
          if (low === "false" || low === "0" || low === "no") out.kargo.allow_prerelease = false;
          else if (low === "true" || low === "1" || low === "yes") out.kargo.allow_prerelease = true;
        } else if (k === "resolution_mode") {
          const low = v.toLowerCase();
          if (low === "locked") out.kargo.resolution_mode = "locked";
          else if (low === "latest") out.kargo.resolution_mode = "latest";
        } else if (k === "strict") {
          const low = v.toLowerCase();
          if (low === "true" || low === "1" || low === "yes") out.kargo.strict = true;
          else if (low === "false" || low === "0" || low === "no") out.kargo.strict = false;
        }
      } else if (section === "" || section === "package" || section === "metadata") {
        if (k === "name") out.name = v;
        else if (k === "version") out.version = v;
        else if (k === "description") out.description = v;
        else if (k === "author") out.author = v;
      }
      continue;
    }
    const kvBare = /^([A-Za-z0-9_-]+)\s*=\s*'([^']*)'\s*$/.exec(line);
    if (kvBare) {
      const k = kvBare[1];
      const v = kvBare[2];
      if (section === "dependencies") out.dependencies[k] = v;
      else if (section === "kargo") {
        if (k === "allow_prerelease") {
          const low = v.toLowerCase();
          if (low === "false" || low === "0" || low === "no") out.kargo.allow_prerelease = false;
          else if (low === "true" || low === "1" || low === "yes") out.kargo.allow_prerelease = true;
        } else if (k === "resolution_mode") {
          const low = v.toLowerCase();
          if (low === "locked") out.kargo.resolution_mode = "locked";
          else if (low === "latest") out.kargo.resolution_mode = "latest";
        } else if (k === "strict") {
          const low = v.toLowerCase();
          if (low === "true" || low === "1" || low === "yes") out.kargo.strict = true;
          else if (low === "false" || low === "0" || low === "no") out.kargo.strict = false;
        }
      }
      continue;
    }
  }
  return out;
}
