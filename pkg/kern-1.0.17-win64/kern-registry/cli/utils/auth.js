import fs from "node:fs/promises";
import path from "node:path";
import { homeDir, ensureDir, exists, readJson, writeJsonAtomic } from "./io.js";

const AUTH_FILE = path.join(homeDir(), ".kern", "registry-auth.json");

function normalizeUrlMaybe(url) {
  if (!url) return "";
  return String(url).trim().replace(/\/+$/, "");
}

export async function readAuthConfig() {
  if (!(await exists(AUTH_FILE))) return { apiUrl: "", apiKey: "", updatedAt: "" };
  const cfg = await readJson(AUTH_FILE);
  return {
    apiUrl: normalizeUrlMaybe(cfg?.apiUrl || ""),
    apiKey: String(cfg?.apiKey || ""),
    updatedAt: String(cfg?.updatedAt || "")
  };
}

export async function writeAuthConfig(next) {
  await ensureDir(path.dirname(AUTH_FILE));
  const cfg = {
    apiUrl: normalizeUrlMaybe(next?.apiUrl || ""),
    apiKey: String(next?.apiKey || ""),
    updatedAt: new Date().toISOString()
  };
  await writeJsonAtomic(AUTH_FILE, cfg);
  return cfg;
}

export async function clearAuthConfig() {
  if (await exists(AUTH_FILE)) {
    await fs.rm(AUTH_FILE, { force: true });
  }
}

export function authConfigPath() {
  return AUTH_FILE;
}
