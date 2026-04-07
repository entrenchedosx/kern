import { describe, it } from "node:test";
import assert from "node:assert";
import { mkdtempSync, writeFileSync, rmSync } from "fs";
import { join } from "path";
import { tmpdir } from "os";
import { readLock } from "../../lib/lockfile.js";
import { KargoCliError, ERR } from "../../lib/cli-error.js";

describe("readLock", () => {
  it("missing kargo.lock → empty lock", async () => {
    const d = mkdtempSync(join(tmpdir(), "kargo-nolock-"));
    try {
      const L = await readLock(d);
      assert.deepStrictEqual(L.packages, {});
      assert.strictEqual(L.lockVersion, 1);
    } finally {
      rmSync(d, { recursive: true, force: true });
    }
  });

  it("invalid JSON → KARGO_INVALID_LOCK_JSON", async () => {
    const d = mkdtempSync(join(tmpdir(), "kargo-badlock-"));
    try {
      writeFileSync(join(d, "kargo.lock"), "{ not json\n", "utf8");
      await assert.rejects(
        readLock(d),
        (e) => e instanceof KargoCliError && e.errorCode === ERR.INVALID_LOCK_JSON
      );
    } finally {
      rmSync(d, { recursive: true, force: true });
    }
  });

  it("packages array → INVALID_LOCK_JSON", async () => {
    const d = mkdtempSync(join(tmpdir(), "kargo-badlock2-"));
    try {
      writeFileSync(join(d, "kargo.lock"), JSON.stringify({ lockVersion: 2, packages: [] }), "utf8");
      await assert.rejects(
        readLock(d),
        (e) => e instanceof KargoCliError && e.errorCode === ERR.INVALID_LOCK_JSON
      );
    } finally {
      rmSync(d, { recursive: true, force: true });
    }
  });
});
