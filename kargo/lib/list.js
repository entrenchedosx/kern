import fs from "node:fs/promises";
import path from "node:path";
import { kargoPackagesDir } from "./paths.js";
import { kargoIoError } from "./cli-error.js";

export async function runList() {
  const root = kargoPackagesDir();
  try {
    await fs.access(root);
  } catch {
    console.log("(no packages cached yet)");
    return;
  }
  try {
    const owners = await fs.readdir(root);
    for (const o of owners.sort()) {
      const op = path.join(root, o);
      const st = await fs.stat(op);
      if (!st.isDirectory()) continue;
      const repos = await fs.readdir(op);
      for (const r of repos.sort()) {
        const rp = path.join(op, r);
        const vst = await fs.stat(rp);
        if (!vst.isDirectory()) continue;
        const vers = await fs.readdir(rp);
        for (const v of vers.sort()) {
          console.log(`${o}/${r} @ ${v}`);
        }
      }
    }
  } catch (e) {
    throw kargoIoError(`kargo list: could not read ${root}`, e);
  }
}
