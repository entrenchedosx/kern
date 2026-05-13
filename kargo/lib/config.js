import fs from "node:fs/promises";
import { kargoConfigPath, kargoHome } from "./paths.js";
import { KargoCliError, EXIT, ERR, kargoIoError } from "./cli-error.js";

export async function readConfig() {
  const p = kargoConfigPath();
  let raw;
  try {
    raw = await fs.readFile(p, "utf8");
  } catch (e) {
    if (e && typeof e === "object" && e !== null && e.code === "ENOENT") return {};
    throw kargoIoError(`kargo: could not read ${p}`, e);
  }
  try {
    const j = JSON.parse(raw);
    if (j === null || typeof j !== "object" || Array.isArray(j)) {
      throw new KargoCliError(
        `${p} must contain a JSON object (got ${j === null ? "null" : Array.isArray(j) ? "array" : typeof j})`,
        EXIT.USER,
        ERR.INVALID_CONFIG_JSON
      );
    }
    return j;
  } catch (e) {
    if (e instanceof KargoCliError) throw e;
    const msg = e && typeof e.message === "string" ? e.message : String(e);
    throw new KargoCliError(`invalid JSON in ${p}: ${msg}`, EXIT.USER, ERR.INVALID_CONFIG_JSON);
  }
}

export async function writeConfig(cfg) {
  try {
    await fs.mkdir(kargoHome(), { recursive: true });
    await fs.writeFile(kargoConfigPath(), JSON.stringify(cfg, null, 2) + "\n", "utf8");
  } catch (e) {
    throw kargoIoError("kargo: could not write ~/.kargo/config.json", e);
  }
}
