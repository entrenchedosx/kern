import { describe, it } from "node:test";
import assert from "node:assert";
import { existsSync, mkdtempSync, readFileSync, rmSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";
import { runInit } from "../../lib/init.js";
import { parseKargoToml } from "../../lib/parse-kargo-toml.js";

describe("init", () => {
  it("writes kargo.toml and .kern", async () => {
    const dir = mkdtempSync(join(tmpdir(), "kargo-init-"));
    try {
      await runInit(["--project", dir]);
      const toml = join(dir, "kargo.toml");
      assert.ok(existsSync(toml));
      assert.ok(existsSync(join(dir, ".kern")));
      const pkg = parseKargoToml(readFileSync(toml, "utf8"));
      assert.strictEqual(pkg.version, "0.1.0");
      assert.ok(pkg.name.length > 0);
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});
