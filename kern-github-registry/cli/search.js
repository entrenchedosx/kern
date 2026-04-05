import { fetchRegistryIndex } from "./utils/fetch.js";

function score(query, name) {
  const q = query.toLowerCase();
  const n = name.toLowerCase();
  if (n === q) return 0;
  if (n.startsWith(q)) return 1;
  if (n.includes(q)) return 2;
  return 1000 + Math.abs(n.length - q.length);
}

export async function runSearch(argv) {
  const query = String(argv[0] || "").trim();
  if (!query) throw new Error("search query is required");
  const { index } = await fetchRegistryIndex();
  const names = Object.keys(index.packages || {});
  const ranked = names
    .map((name) => ({ name, latest: index.packages[name]?.latest || "", score: score(query, name) }))
    .sort((a, b) => a.score - b.score)
    .slice(0, 20);
  for (const row of ranked) {
    if (row.score < 1000) console.log(`${row.name}\t${row.latest}`);
  }
}
