import fs from "node:fs/promises";
import path from "node:path";
import os from "node:os";

export async function ensureDir(dirPath) {
  await fs.mkdir(dirPath, { recursive: true });
}

export async function readJson(filePath) {
  const raw = await fs.readFile(filePath, "utf8");
  return JSON.parse(raw);
}

export async function writeJsonAtomic(filePath, data) {
  const dir = path.dirname(filePath);
  await ensureDir(dir);
  const temp = `${filePath}.tmp-${process.pid}-${Date.now()}`;
  const body = `${JSON.stringify(data, null, 2)}\n`;
  await fs.writeFile(temp, body, "utf8");
  await fs.rename(temp, filePath);
}

export function homeDir() {
  return os.homedir();
}

export async function exists(filePath) {
  try {
    await fs.access(filePath);
    return true;
  } catch {
    return false;
  }
}
