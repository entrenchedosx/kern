import { describe, it } from "node:test";
import assert from "node:assert";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "url";
import { dirname, join } from "path";
import { EXIT } from "../../lib/cli-error.js";

const entry = join(dirname(fileURLToPath(import.meta.url)), "..", "entry.js");

describe("exit codes", () => {
  it("unknown command → USAGE (2)", () => {
    const r = spawnSync(process.execPath, [entry, "not-a-command"], { encoding: "utf8" });
    assert.strictEqual(r.status, EXIT.USAGE);
  });
  it("install with no args → USAGE (2)", () => {
    const r = spawnSync(process.execPath, [entry, "install"], { encoding: "utf8" });
    assert.strictEqual(r.status, EXIT.USAGE);
  });

  it("--json-errors prints one JSON line with exit and suggestion", () => {
    const r = spawnSync(process.execPath, [entry, "--json-errors", "not-a-command"], {
      encoding: "utf8"
    });
    assert.strictEqual(r.status, EXIT.USAGE);
    const line = (r.stderr || "").trim().split(/\r?\n/)[0];
    const j = JSON.parse(line);
    assert.strictEqual(j.error.exit, EXIT.USAGE);
    assert.strictEqual(typeof j.error.message, "string");
    assert.ok(j.error.suggestion.includes("help"));
  });
});
