import path from "node:path";
import fs from "node:fs/promises";
import crypto from "node:crypto";

function getOrigin(req) {
  const host = req.headers.host || "127.0.0.1:4873";
  const proto = req.headers["x-forwarded-proto"] || "http";
  return `${proto}://${host}`;
}

function isValidName(name) {
  return typeof name === "string" && /^[a-z][a-z0-9._-]*$/.test(name);
}

function isValidVersion(version) {
  return typeof version === "string" && /^\d+\.\d+\.\d+(-[0-9A-Za-z.-]+)?$/.test(version);
}

async function writeArtifact(storageRoot, name, version, filename, bytes) {
  const versionDir = path.join(storageRoot, "packages", name, version);
  await fs.mkdir(versionDir, { recursive: true });
  const outPath = path.join(versionDir, filename);
  await fs.writeFile(outPath, bytes);
  return outPath;
}

export async function handlePublish(req, res, db, saveDb, options = {}) {
  const body = req.body || {};
  const storageRoot = options.storageRoot;
  if (!storageRoot) {
    res.writeHead(500, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "server misconfigured: storageRoot missing" }));
    return;
  }

  const name = body?.name;
  const version = body?.version;
  const manifest = body?.manifest || {};
  const tarballBase64 = body?.tarballBase64 || "";
  const integrity = body?.integrity || "";

  if (!isValidName(name) || !isValidVersion(version)) {
    res.writeHead(400, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "invalid package name or version" }));
    return;
  }
  if (!manifest?.main || !manifest?.dependencies || typeof manifest.dependencies !== "object") {
    res.writeHead(400, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "manifest.main and manifest.dependencies are required" }));
    return;
  }
  if (!tarballBase64 || !integrity) {
    res.writeHead(400, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "tarballBase64 and integrity are required" }));
    return;
  }

  db.registryVersion = 2;
  db.generatedAt = new Date().toISOString();
  db.packages = db.packages || {};
  db.packages[name] = db.packages[name] || {
    name,
    description: manifest.description || "",
    trusted: false,
    latest: version,
    versions: {}
  };
  if (db.packages[name].versions?.[version]) {
    res.writeHead(409, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: `version already exists: ${name}@${version}` }));
    return;
  }

  const tarballName = `${name}-${version}.tgz`;
  const tarballBytes = Buffer.from(tarballBase64, "base64");
  const digest = `sha256-${crypto.createHash("sha256").update(tarballBytes).digest("hex")}`;
  if (digest !== integrity) {
    res.writeHead(400, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "integrity mismatch" }));
    return;
  }

  await writeArtifact(storageRoot, name, version, tarballName, tarballBytes);
  const tarballUrl = `${getOrigin(req)}/api/v1/files/${encodeURIComponent(name)}/${encodeURIComponent(version)}/${encodeURIComponent(tarballName)}`;
  const versionManifest = {
    name,
    version,
    main: manifest.main,
    dependencies: manifest.dependencies || {},
    trusted: Boolean(db.packages[name].trusted),
    dist: {
      tarball: tarballUrl,
      shasum: integrity
    }
  };

  db.packages[name].description = manifest.description || db.packages[name].description || "";
  db.packages[name].latest = version;
  db.packages[name].versions[version] = versionManifest;
  await saveDb(db);

  res.writeHead(201, { "content-type": "application/json" });
  res.end(JSON.stringify({ ok: true, name, version, dist: versionManifest.dist }));
}
