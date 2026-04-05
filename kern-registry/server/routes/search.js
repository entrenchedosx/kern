function score(query, name) {
  const q = query.toLowerCase();
  const n = name.toLowerCase();
  if (n === q) return 0;
  if (n.startsWith(q)) return 1;
  if (n.includes(q)) return 2;
  return 50 + Math.abs(n.length - q.length);
}

export function handleSearch(req, res, db, query) {
  if (!query) {
    res.writeHead(400, { "content-type": "application/json" });
    res.end(JSON.stringify({ error: "q is required" }));
    return;
  }
  const names = Object.keys(db?.packages || {});
  const results = names
    .map((name) => ({ name, latest: db.packages[name].latest, score: score(query, name) }))
    .sort((a, b) => a.score - b.score)
    .slice(0, 20);
  res.writeHead(200, { "content-type": "application/json" });
  res.end(JSON.stringify({ query, results }));
}
