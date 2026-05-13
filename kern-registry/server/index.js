import http from "node:http";
import path from "node:path";
import fs from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { readJson, writeJsonAtomic } from "../cli/utils/io.js";
import { handlePublish } from "./routes/publish.js";
import { handlePackage } from "./routes/install.js";
import { handleSearch } from "./routes/search.js";
import { handleFileDownload } from "./routes/files.js";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const dbPath = path.join(__dirname, "db.json");
const storageRoot = process.env.KERN_REGISTRY_STORAGE_ROOT
  ? path.resolve(process.env.KERN_REGISTRY_STORAGE_ROOT)
  : path.join(__dirname, "storage");
const port = Number(process.env.PORT || 4873);
const maxBodyBytes = Number(process.env.KERN_REGISTRY_MAX_BODY_BYTES || 80 * 1024 * 1024);

async function loadDb() {
  try {
    return await readJson(dbPath);
  } catch {
    return { registryVersion: 1, packages: {} };
  }
}

async function saveDb(db) {
  await writeJsonAtomic(dbPath, db);
}

function withCors(res) {
  res.setHeader("access-control-allow-origin", "*");
  res.setHeader("access-control-allow-methods", "GET,POST,OPTIONS");
  res.setHeader("access-control-allow-headers", "content-type");
}

function getApiKeys() {
  const raw = process.env.KERN_REGISTRY_API_KEYS || "";
  return raw
    .split(",")
    .map((s) => s.trim())
    .filter(Boolean);
}

function requirePublishAuth(req, res) {
  const keys = getApiKeys();
  if (keys.length === 0) return true;
  const header = String(req.headers["authorization"] || "");
  const tokenHeader = String(req.headers["x-api-key"] || "");
  const bearer = header.toLowerCase().startsWith("bearer ") ? header.slice(7).trim() : "";
  const provided = tokenHeader || bearer;
  if (!provided || !keys.includes(provided)) {
    res.writeHead(401, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "unauthorized" }));
    return false;
  }
  return true;
}

async function readJsonBody(req, res) {
  const chunks = [];
  let size = 0;
  for await (const chunk of req) {
    const b = Buffer.from(chunk);
    size += b.length;
    if (size > maxBodyBytes) {
      res.writeHead(413, { "content-type": "application/json" });
      res.end(JSON.stringify({ error: "payload too large" }));
      return null;
    }
    chunks.push(b);
  }
  const raw = Buffer.concat(chunks).toString("utf8");
  try {
    return JSON.parse(raw || "{}");
  } catch {
    res.writeHead(400, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "invalid json body" }));
    return null;
  }
}

function buildSimpleIndex(db) {
  const out = { registryVersion: 2, generatedAt: new Date().toISOString(), packages: {} };
  for (const [name, pkg] of Object.entries(db?.packages || {})) {
    out.packages[name] = {
      latest: pkg.latest || null,
      metadata: `/api/v1/packages/${encodeURIComponent(name)}`
    };
  }
  return out;
}

async function main() {
  await fs.mkdir(storageRoot, { recursive: true });
  let db = await loadDb();
  const server = http.createServer(async (req, res) => {
    withCors(res);
    if (req.method === "OPTIONS") {
      res.writeHead(204);
      res.end();
      return;
    }

    const url = new URL(req.url, `http://${req.headers.host || "localhost"}`);
    if (req.method === "GET" && url.pathname === "/health") {
      res.writeHead(200, { "content-type": "application/json" });
      res.end(JSON.stringify({ ok: true }));
      return;
    }

    if (req.method === "GET" && (url.pathname === "/api/v1/simple" || url.pathname === "/registry.json")) {
      res.writeHead(200, { "content-type": "application/json" });
      res.end(JSON.stringify(buildSimpleIndex(db)));
      return;
    }
    if (req.method === "GET" && url.pathname.startsWith("/api/v1/simple/")) {
      const name = decodeURIComponent(url.pathname.split("/").filter(Boolean)[3] || "");
      const pkg = db?.packages?.[name];
      if (!pkg) {
        res.writeHead(404, { "content-type": "application/json" });
        res.end(JSON.stringify({ error: "package not found" }));
        return;
      }
      res.writeHead(200, { "content-type": "application/json" });
      res.end(JSON.stringify({
        name,
        latest: pkg.latest || null,
        versions: Object.keys(pkg.versions || {}).sort(),
        metadata: `/api/v1/packages/${encodeURIComponent(name)}`
      }));
      return;
    }

    if (req.method === "POST" && (url.pathname === "/publish" || url.pathname === "/api/v1/packages")) {
      if (!requirePublishAuth(req, res)) return;
      const body = await readJsonBody(req, res);
      if (body === null) return;
      req.body = body;
      await handlePublish(req, res, db, async (nextDb) => {
        db = nextDb;
        await saveDb(db);
      }, { storageRoot });
      return;
    }

    if (req.method === "GET" && (url.pathname === "/search" || url.pathname === "/api/v1/search")) {
      handleSearch(req, res, db, url.searchParams.get("q"));
      return;
    }

    if (req.method === "GET" && (url.pathname.startsWith("/package/") || url.pathname.startsWith("/api/v1/packages/"))) {
      const parts = url.pathname.split("/").filter(Boolean);
      const base = parts[0] === "api" ? 3 : 1;
      const packageName = decodeURIComponent(parts[base] || "");
      const version = parts[base + 1] ? decodeURIComponent(parts[base + 1]) : null;
      handlePackage(req, res, db, packageName, version);
      return;
    }

    if (req.method === "GET" && url.pathname.startsWith("/api/v1/files/")) {
      const parts = url.pathname.split("/").filter(Boolean);
      if (parts.length < 6) {
        res.writeHead(400, { "content-type": "application/json" });
        res.end(JSON.stringify({ error: "invalid file path" }));
        return;
      }
      const packageName = decodeURIComponent(parts[3]);
      const version = decodeURIComponent(parts[4]);
      const filename = decodeURIComponent(parts.slice(5).join("/"));
      await handleFileDownload(req, res, storageRoot, packageName, version, filename);
      return;
    }

    res.writeHead(404, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "not found" }));
  });

  server.listen(port, () => {
    console.log(`kern-registry server listening on :${port}`);
  });
}

main();
