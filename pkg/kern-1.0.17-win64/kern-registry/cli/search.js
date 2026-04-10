import { fetchRegistryIndex } from "./utils/fetchRegistry.js";

function score(query, candidate) {
  const q = query.toLowerCase();
  const c = candidate.toLowerCase();
  if (c === q) return 0;
  if (c.startsWith(q)) return 1;
  if (c.includes(q)) return 2;
  return 1000 + Math.abs(c.length - q.length);
}

function parseArgs(argv) {
  const out = { query: "", api: null };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--api" && i + 1 < argv.length) {
      out.api = String(argv[++i]).replace(/\/+$/, "");
    } else if (!out.query) {
      out.query = a;
    } else {
      throw new Error(`Unknown search arg: ${a}`);
    }
  }
  return out;
}

export async function runSearch(argv) {
  const args = parseArgs(argv);
  if (args.api) process.env.KERN_REGISTRY_API_URL = args.api;
  const query = args.query;
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
