import { fetchRegistryIndex } from "./utils/fetchRegistry.js";

function score(query, candidate) {
  const q = query.toLowerCase();
  const c = candidate.toLowerCase();
  if (c === q) return 0;
  if (c.startsWith(q)) return 1;
  if (c.includes(q)) return 2;
  return 1000 + Math.abs(c.length - q.length);
}

export async function runSearch(argv) {
  const query = argv[0];
  if (!query) throw new Error("search query is required");
  const { index } = await fetchRegistryIndex();
  const names = Object.keys(index.packages || {});
  const ranked = names
    .map((name) => ({ name, score: score(query, name), latest: index.packages[name].latest }))
    .sort((a, b) => a.score - b.score)
    .slice(0, 20);

  for (const row of ranked) {
    if (row.score >= 1000) continue;
    console.log(`${row.name}\t${row.latest}`);
  }
}
