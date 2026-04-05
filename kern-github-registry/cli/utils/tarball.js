import path from "node:path";
import * as tar from "tar";
import { ensureDir } from "./io.js";

export async function createTarball(projectDir, outFile) {
  await ensureDir(path.dirname(outFile));
  await tar.create(
    {
      gzip: true,
      file: outFile,
      cwd: projectDir,
      portable: true
    },
    ["kern.json", "src", "README.md"]
  );
}

export async function extractTarball(file, outDir) {
  await ensureDir(outDir);
  // Guard against archive traversal and symlink tricks.
  await tar.extract({
    file,
    cwd: outDir,
    gzip: true,
    strict: true,
    onentry: (entry) => {
      const p = entry.path || "";
      if (path.isAbsolute(p) || p.includes("..") || p.includes("\\..")) {
        throw new Error(`Unsafe tar entry path: ${p}`);
      }
      if (entry.type === "SymbolicLink" || entry.type === "Link") {
        throw new Error(`Links are not allowed in packages: ${p}`);
      }
    }
  });
}
