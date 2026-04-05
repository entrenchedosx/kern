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
  await ensureDir(path.dirname(filePath));
  const tmp = `${filePath}.tmp-${process.pid}-${Date.now()}`;
  await fs.writeFile(tmp, `${JSON.stringify(data, null, 2)}\n`, "utf8");
  await fs.rename(tmp, filePath);
}

export async function writeJsonAtomicSecure(filePath, data) {
  await writeJsonAtomic(filePath, data);
  // Best effort: restrict token-bearing config file permissions on POSIX.
  if (process.platform !== "win32") {
    try {
      await fs.chmod(filePath, 0o600);
    } catch {
      // ignore chmod errors on filesystems that do not support unix perms
    }
  }
}

export async function exists(filePath) {
  try {
    await fs.access(filePath);
    return true;
  } catch {
    return false;
  }
}

export function homeDir() {
  return os.homedir();
}
