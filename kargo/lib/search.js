import { readConfig } from "./config.js";
import { KargoCliError, EXIT } from "./cli-error.js";

export async function runSearch(argv) {
  const q = argv.join(" ").trim();
  if (!q) throw new KargoCliError("Usage: kargo search <query>", EXIT.USAGE);

  const cfg = await readConfig();
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
