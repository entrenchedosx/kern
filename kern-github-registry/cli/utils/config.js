import path from "node:path";
import { exists, homeDir, readJson, writeJsonAtomicSecure } from "./io.js";

const CONFIG_PATH = path.join(homeDir(), ".kern", "config.json");

export function getConfigPath() {
  return CONFIG_PATH;
}

export async function readConfig() {
  if (!(await exists(CONFIG_PATH))) {
    return {
      githubToken: "",
      registryRepo: "",
      registryRef: "main"
    };
  }
  const cfg = await readJson(CONFIG_PATH);
  return {
    githubToken: String(cfg?.githubToken || ""),
    registryRepo: String(cfg?.registryRepo || ""),
    registryRef: String(cfg?.registryRef || "main")
  };
}

export async function writeConfig(next) {
  const cfg = {
    githubToken: String(next?.githubToken || ""),
    registryRepo: String(next?.registryRepo || ""),
    registryRef: String(next?.registryRef || "main")
  };
  await writeJsonAtomicSecure(CONFIG_PATH, cfg);
  return cfg;
}
