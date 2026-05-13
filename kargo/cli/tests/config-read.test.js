import { describe, it } from "node:test";
import assert from "node:assert";
import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync, rmSync, mkdtempSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";
import { fileURLToPath } from "url";
import { dirname } from "path";
import { EXIT } from "../../lib/cli-error.js";

const entry = join(dirname(fileURLToPath(import.meta.url)), "..", "entry.js");

describe("readConfig", () => {
  it("invalid ~/.kargo/config.json → USER (1)", () => {
    const home = mkdtempSync(join(tmpdir(), "kargo-badcfg-"));
    try {
      const kd = join(home, ".kargo");
      mkdirSync(kd, { recursive: true });
      writeFileSync(join(kd, "config.json"), "{ not json\n", "utf8");
      const env = { ...process.env, HOME: home, USERPROFILE: home };
      const r = spawnSync(process.execPath, [entry, "search", "x"], { encoding: "utf8", env });
      assert.strictEqual(r.status, EXIT.USER, r.stderr || r.stdout);
      assert.match(r.stderr || "", /KARGO_INVALID_CONFIG_JSON/);
      assert.match(r.stderr || "", /invalid JSON|config/i);
      assert.match(r.stderr || "", /Suggestion:/);
      assert.match(r.stderr || "", /config\.json|Fix JSON/i);
    } finally {
      rmSync(home, { recursive: true, force: true });
    }
  });
});
