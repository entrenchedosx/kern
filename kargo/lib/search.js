import { readConfig } from "./config.js";
import { kargoVerbose } from "./cli-env.js";
import { fetchRegistryIndex, fetchPackageMetadata } from "./registry-index.js";
import { KargoCliError, EXIT } from "./cli-error.js";

/** Rank names for Kern-style dotted packages; avoids matching "sec" inside unrelated substrings where possible. */
export function scoreRegistryName(query, name) {
  const q = query.toLowerCase().trim();
  if (!q) return 9999;
  const n = name.toLowerCase();
  if (n === q) return 0;
  if (n.startsWith(`${q}.`) || n.startsWith(`${q}/`)) return 1;
  const parts = n.split(/[./]/);
  for (const p of parts) {
    if (p === q) return 2;
    if (p.startsWith(q)) return 3;
  }
  if (n.includes(q)) return 4;
  return 1000 + Math.abs(n.length - q.length);
}

function parseSearchArgv(argv) {
  const out = { github: false, api: null, queryParts: [] };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--github") out.github = true;
    else if ((a === "--api" || a === "--registry-api") && i + 1 < argv.length) {
      out.api = String(argv[++i]).replace(/\/+$/, "");
    } else {
      out.queryParts.push(a);
    }
  }
  return out;
}

async function runGithubSearch(q, cfg) {
  const token = cfg.github?.token || process.env.GITHUB_TOKEN || "";
  const url = new URL("https://api.github.com/search/repositories");
  url.searchParams.set("q", `${q} in:name,description`);
  url.searchParams.set("per_page", "20");

  const headers = {
    Accept: "application/vnd.github+json",
    "X-GitHub-Api-Version": "2022-11-28"
  };
  if (token) headers.Authorization = `Bearer ${token}`;

  const res = await fetch(url, { headers });
  if (!res.ok) {
    const t = await res.text();
    throw new KargoCliError(`GitHub search failed (${res.status}): ${t}`, EXIT.USER);
  }
  const data = await res.json();
  for (const it of data.items || []) {
    console.log(`${it.full_name}\t★${it.stargazers_count}\t${it.description || ""}`);
  }
}

function truncateDesc(s, maxLen) {
  const t = String(s || "").replace(/\s+/g, " ").trim();
  if (t.length <= maxLen) return t;
  return `${t.slice(0, maxLen - 1)}…`;
}

export async function runSearch(argv) {
  const cfg = await readConfig();
  const parsed = parseSearchArgv(argv);
  const q = parsed.queryParts.join(" ").trim();
  if (!q) {
    throw new KargoCliError(
      "Usage: kargo search <query>\n" +
        "       kargo search --github <query>     search GitHub repositories (legacy)\n" +
        "       kargo search --api <url> <query>  Kern registry HTTP API base (overrides env)",
      EXIT.USAGE
    );
  }

  if (parsed.api) process.env.KERN_REGISTRY_API_URL = parsed.api;

  if (parsed.github) {
    await runGithubSearch(q, cfg);
    return;
  }

  let registryUrl;
  let index;
  try {
    const r = await fetchRegistryIndex();
    registryUrl = r.registryUrl;
    index = r.index;
    if (kargoVerbose()) {
      console.error(`kargo search: registry (${r.mode}) ${registryUrl}`);
    }
  } catch (e) {
    const msg = e && typeof e.message === "string" ? e.message : String(e);
    throw new KargoCliError(
      `Kern registry index failed: ${msg}\n` +
        "Set KERN_REGISTRY_URL to a registry.json URL or path, or KERN_REGISTRY_API_URL to your registry server.\n" +
        "Use kargo search --github <query> to search GitHub instead.",
      EXIT.USER
    );
  }

  const names = Object.keys(index.packages || {});
  const ranked = names
    .map((name) => ({ name, score: scoreRegistryName(q, name), latest: index.packages[name]?.latest || "" }))
    .filter((row) => row.score < 1000)
    .sort((a, b) => a.score - b.score || a.name.localeCompare(b.name))
    .slice(0, 20);

  if (ranked.length === 0) {
    console.log("(no matching Kern packages — try a different query or kargo search --github)");
    return;
  }

  for (const row of ranked) {
    let desc = "";
    try {
      const packed = await fetchPackageMetadata(index, registryUrl, row.name);
      if (packed?.metadata) desc = truncateDesc(packed.metadata.description, 120);
    } catch {
      /* optional description */
    }
    if (desc) console.log(`${row.name}\t${row.latest}\t${desc}`);
    else console.log(`${row.name}\t${row.latest}`);
  }
}
