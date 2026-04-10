import test from "node:test";
import assert from "node:assert/strict";
import { buildLockfile, lockSatisfiesDirectDeps, normalizeManifestDependencies } from "../utils/lockfile.js";

test("normalize manifest dependencies from array", () => {
  const deps = normalizeManifestDependencies({ dependencies: ["a", "b"] });
  assert.deepEqual(deps, { a: "*", b: "*" });
});

test("normalize manifest dependencies from object", () => {
  const deps = normalizeManifestDependencies({ dependencies: { a: "^1.0.0" } });
  assert.deepEqual(deps, { a: "^1.0.0" });
});

test("lockfile direct dependency satisfaction", () => {
  const lock = buildLockfile("https://registry.example/registry.json", {
    alpha: {
      version: "1.0.0",
      dist: { tarball: "https://example.invalid/alpha.tgz", shasum: "sha256-abc" },
      dependencies: {}
    }
  });
  assert.equal(lockSatisfiesDirectDeps(lock, { alpha: "^1.0.0" }), true);
  assert.equal(lockSatisfiesDirectDeps(lock, { beta: "^1.0.0" }), false);
});
