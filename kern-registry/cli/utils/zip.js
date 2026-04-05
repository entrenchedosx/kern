import path from "node:path";
import fs from "node:fs/promises";
import * as tar from "tar";
import { ensureDir, exists } from "./io.js";

function defaultIgnore(name) {
  const normalized = name.replace(/\\/g, "/");
  return (
    normalized.startsWith(".git/") ||
    normalized.startsWith(".kern/") ||
    normalized.startsWith("node_modules/") ||
    normalized === ".git" ||
    normalized === ".kern" ||
    normalized === "node_modules"
  );
}

export async function createTarball(sourceDir, outFile) {
  await ensureDir(path.dirname(outFile));
  await tar.create(
    {
      gzip: true,
      cwd: sourceDir,
      file: outFile,
      portable: true,
      filter: (name) => !defaultIgnore(name)
    },
    ["."]
  );
}

export async function extractTarball(tarballPath, outDir) {
  await ensureDir(outDir);
  await tar.extract({
    file: tarballPath,
    cwd: outDir,
    strip: 1
  });
}

export async function copyTarballTo(targetPath, tarballBuffer) {
  await ensureDir(path.dirname(targetPath));
  await fs.writeFile(targetPath, tarballBuffer);
}

export async function ensureTarballPresent(cachePath, loadFn) {
  if (await exists(cachePath)) return cachePath;
  const buf = await loadFn();
  await copyTarballTo(cachePath, buf);
  return cachePath;
}
