import test from "node:test";
import assert from "node:assert/strict";
import { resolveDistTarballUrl } from "../utils/resolve.js";

test("resolveDistTarballUrl passes through https tarball", () => {
  assert.equal(
    resolveDistTarballUrl("file:///x/y/1.0.0.json", "https://cdn.example/pkg.tgz"),
    "https://cdn.example/pkg.tgz"
  );
});

test("resolveDistTarballUrl resolves relative tarball against manifest URL", () => {
  const manifestUrl = "file:///C:/repo/kern-registry/registry/packages/foo/versions/1.0.0.json";
  const abs = resolveDistTarballUrl(manifestUrl, "../../../dist/foo/foo-1.0.0.tgz");
  assert.ok(abs.includes("dist/foo/foo-1.0.0.tgz"), abs);
  assert.ok(abs.startsWith("file:"), abs);
});
