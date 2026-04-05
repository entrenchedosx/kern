export function handlePackage(req, res, db, packageName, version) {
  const pkg = db?.packages?.[packageName];
  if (!pkg) {
    res.writeHead(404, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "package not found" }));
    return;
  }

  if (!version) {
    res.writeHead(200, { "content-type": "application/json" });
    res.end(JSON.stringify(pkg));
    return;
  }

  const manifest = pkg.versions?.[version];
  if (!manifest) {
    res.writeHead(404, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "version not found" }));
    return;
  }
  res.writeHead(200, { "content-type": "application/json" });
  res.end(JSON.stringify(manifest));
}
