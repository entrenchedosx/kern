function metadataForPackage(pkg, packageName) {
  const versions = {};
  for (const v of Object.keys(pkg?.versions || {})) {
    versions[v] = { manifest: `/api/v1/packages/${encodeURIComponent(packageName)}/${encodeURIComponent(v)}` };
  }
  return {
    name: pkg?.name || packageName,
    description: pkg?.description || "",
    trusted: Boolean(pkg?.trusted),
    latest: pkg?.latest || null,
    versions
  };
}

export function handlePackage(req, res, db, packageName, version) {
  const pkg = db?.packages?.[packageName] || null;
  if (!pkg) {
    res.writeHead(404, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "package not found" }));
    return;
  }

  if (!version) {
    res.writeHead(200, { "content-type": "application/json" });
    res.end(JSON.stringify(metadataForPackage(pkg, packageName)));
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
