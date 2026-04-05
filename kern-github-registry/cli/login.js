import { getConfigPath, readConfig, writeConfig } from "./utils/config.js";
import { splitRepo, validateTokenAndScopes } from "./utils/github.js";

function parseArgs(argv) {
  const out = { token: "", repo: "", show: false, logout: false, skipValidate: false };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if ((a === "--token" || a === "--github-token") && i + 1 < argv.length) {
      out.token = String(argv[++i]);
    } else if (a === "--repo" && i + 1 < argv.length) {
      out.repo = String(argv[++i]);
    } else if (a === "--show") {
      out.show = true;
    } else if (a === "--logout") {
      out.logout = true;
    } else if (a === "--no-validate") {
      out.skipValidate = true;
    } else {
      throw new Error(`Unknown login arg: ${a}`);
    }
  }
  return out;
}

function maskToken(token) {
  const t = String(token || "");
  if (!t) return "(not set)";
  if (t.length <= 8) return `${t.slice(0, 2)}***${t.slice(-1)}`;
  return `${t.slice(0, 4)}...${t.slice(-4)}`;
}

export async function runLogin(argv) {
  const args = parseArgs(argv);
  const cur = await readConfig();
  if (args.logout) {
    const next = await writeConfig({ githubToken: "", registryRepo: "", registryRef: "main" });
    console.log(`Cleared credentials in ${getConfigPath()}`);
    console.log(`Repo: ${next.registryRepo || "(not set)"}`);
    return;
  }
  if (args.show) {
    console.log(JSON.stringify({
      registryRepo: cur.registryRepo || "(not set)",
      registryRef: cur.registryRef || "main",
      githubToken: maskToken(cur.githubToken),
      path: getConfigPath()
    }, null, 2));
    return;
  }

  const token = args.token || process.env.KERN_GITHUB_TOKEN || cur.githubToken;
  const repo = args.repo || process.env.KERN_REGISTRY_REPO || cur.registryRepo;
  if (!token) throw new Error("missing token (pass --token or set KERN_GITHUB_TOKEN)");
  if (!repo) throw new Error("missing repo (pass --repo owner/repo or set KERN_REGISTRY_REPO)");
  if (!/^gh[pousr]_|^github_pat_/i.test(token)) {
    throw new Error("token format looks invalid (expected GitHub PAT format)");
  }
  splitRepo(repo); // validate
  let who = null;
  if (!args.skipValidate) {
    who = await validateTokenAndScopes(token);
  }
  const next = await writeConfig({
    githubToken: token,
    registryRepo: repo,
    registryRef: cur.registryRef || "main"
  });
  console.log(`Saved credentials to ${getConfigPath()}`);
  console.log(`Repo: ${next.registryRepo}`);
  console.log(`Token: ${maskToken(next.githubToken)}`);
  if (who) {
    console.log(`Validated as GitHub user: ${who.login}`);
  }
}
