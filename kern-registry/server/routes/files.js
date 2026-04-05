import path from "node:path";
import fs from "node:fs/promises";

function isSafeSegment(v) {
  return typeof v === "string" && v.length > 0 && !v.includes("..") && !v.includes("/") && !v.includes("\\");
}

export async function handleFileDownload(req, res, storageRoot, packageName, version, filename) {
  if (!isSafeSegment(packageName) || !isSafeSegment(version) || !isSafeSegment(filename)) {
    res.writeHead(400, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "invalid path segments" }));
    return;
  }
  const filePath = path.join(storageRoot, "packages", packageName, version, filename);
  try {
    const bytes = await fs.readFile(filePath);
    res.writeHead(200, {
      "content-type": "application/octet-stream",
      "content-length": String(bytes.length),
      "cache-control": "public, max-age=31536000, immutable"
    });
    res.end(bytes);
  } catch {
    res.writeHead(404, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "artifact not found" }));
  }
}
