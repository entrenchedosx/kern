import http from "node:http";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { readJson, writeJsonAtomic } from "../cli/utils/io.js";
import { handlePublish } from "./routes/publish.js";
import { handlePackage } from "./routes/install.js";
import { handleSearch } from "./routes/search.js";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const dbPath = path.join(__dirname, "db.json");
const port = Number(process.env.PORT || 4873);

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

async function main() {
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

    if (req.method === "POST" && url.pathname === "/publish") {
      await handlePublish(req, res, db, async (nextDb) => {
        db = nextDb;
        await saveDb(db);
      });
      return;
    }

    if (req.method === "GET" && url.pathname === "/search") {
      handleSearch(req, res, db, url.searchParams.get("q"));
      return;
    }

    if (req.method === "GET" && url.pathname.startsWith("/package/")) {
      const parts = url.pathname.split("/").filter(Boolean);
      const packageName = parts[1];
      const version = parts[2] || null;
      handlePackage(req, res, db, packageName, version);
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
