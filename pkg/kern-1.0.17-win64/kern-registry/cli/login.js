import { readAuthConfig, writeAuthConfig, clearAuthConfig, authConfigPath } from "./utils/auth.js";
import { getRegistryApiBase } from "./utils/fetchRegistry.js";

function parseArgs(argv) {
  const out = { api: "", token: "", logout: false, show: false };
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--api" && i + 1 < argv.length) {
      out.api = String(argv[++i]);
    } else if ((a === "--token" || a === "--key") && i + 1 < argv.length) {
      out.token = String(argv[++i]);
    } else if (a === "--logout") {
      out.logout = true;
    } else if (a === "--show") {
      out.show = true;
    } else {
      throw new Error(`Unknown login arg: ${a}`);
    }
  }
  return out;
}

function maskToken(t) {
  const token = String(t || "");
  if (!token) return "(not set)";
  if (token.length <= 8) return `${token.slice(0, 2)}***${token.slice(-1)}`;
  return `${token.slice(0, 4)}...${token.slice(-4)}`;
}

export async function runLogin(argv) {
  const args = parseArgs(argv);
  if (args.logout) {
    await clearAuthConfig();
    console.log(`Logged out. Removed ${authConfigPath()}`);
    return;
  }

  const existing = await readAuthConfig();
  if (args.show) {
    console.log(JSON.stringify({
      apiUrl: existing.apiUrl || "(not set)",
      apiKey: maskToken(existing.apiKey),
      path: authConfigPath()
    }, null, 2));
    return;
  }

  const apiUrl = (args.api || existing.apiUrl || getRegistryApiBase() || "http://127.0.0.1:4873").trim();
  const token = (args.token || process.env.KERN_REGISTRY_API_KEY || existing.apiKey || "").trim();
  if (!token) {
    throw new Error("missing token. Use `kern login --token <token>` or set KERN_REGISTRY_API_KEY.");
  }

  const saved = await writeAuthConfig({ apiUrl, apiKey: token });
  console.log(`Saved registry credentials to ${authConfigPath()}`);
  console.log(`API: ${saved.apiUrl}`);
  console.log(`Token: ${maskToken(saved.apiKey)}`);
}
