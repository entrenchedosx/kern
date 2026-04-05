export async function handlePublish(req, res, db, saveDb) {
  let body = "";
  req.on("data", (chunk) => {
    body += chunk.toString("utf8");
  });
  req.on("end", async () => {
    try {
      const payload = JSON.parse(body || "{}");
      const { name, version, manifest } = payload;
      if (!name || !version || !manifest) {
        res.writeHead(400, { "content-type": "application/json" });
        res.end(JSON.stringify({ error: "name, version, and manifest are required" }));
        return;
      }
      db.packages = db.packages || {};
      db.packages[name] = db.packages[name] || { latest: version, versions: {} };
      db.packages[name].versions[version] = manifest;
      db.packages[name].latest = version;
      await saveDb(db);
      res.writeHead(200, { "content-type": "application/json" });
      res.end(JSON.stringify({ ok: true, name, version }));
    } catch (err) {
      res.writeHead(500, { "content-type": "application/json" });
      res.end(JSON.stringify({ error: err.message }));
    }
  });
}
