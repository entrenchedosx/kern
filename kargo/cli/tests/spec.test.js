import { describe, it } from "node:test";
import assert from "node:assert";
import { parseGithubSpec, parseDepString, tagToSemver } from "../../lib/spec.js";
import { parseKargoToml } from "../../lib/parse-kargo-toml.js";

describe("spec", () => {
  it("parses owner/repo@tag", () => {
    const x = parseGithubSpec("Foo/Bar@v1.2.3");
    assert.strictEqual(x.owner, "Foo");
    assert.strictEqual(x.repo, "Bar");
    assert.strictEqual(x.tag, "v1.2.3");
  });
  it("parses github url", () => {
    const x = parseGithubSpec("https://github.com/a/b@v0.1.0");
    assert.strictEqual(x.owner, "a");
    assert.strictEqual(x.repo, "b");
    assert.strictEqual(x.tag, "v0.1.0");
  });
  it("tagToSemver strips v", () => {
    assert.strictEqual(tagToSemver("v1.0.0"), "1.0.0");
  });
  it("parseDepString", () => {
    const d = parseDepString("x/y@v2.0.0");
    assert.strictEqual(d.owner, "x");
    assert.strictEqual(d.repo, "y");
  });
});

describe("kargo.toml", () => {
  it("parses minimal", () => {
    const p = parseKargoToml(`name = "p"\nversion = "1.0.0"\n\n[dependencies]\na = "o/r@v1.0.0"\n`);
    assert.strictEqual(p.name, "p");
    assert.strictEqual(p.version, "1.0.0");
    assert.strictEqual(p.dependencies.a, "o/r@v1.0.0");
  });

  it("parses [kargo] allow_prerelease", () => {
    const p = parseKargoToml(`[kargo]\nallow_prerelease = "false"\n`);
    assert.strictEqual(p.kargo.allow_prerelease, false);
    const q = parseKargoToml(`[kargo]\nallow_prerelease = "true"\n`);
    assert.strictEqual(q.kargo.allow_prerelease, true);
  });

  it("parses [kargo] resolution_mode", () => {
    const p = parseKargoToml(`[kargo]\nresolution_mode = "locked"\n`);
    assert.strictEqual(p.kargo.resolution_mode, "locked");
    const q = parseKargoToml(`[kargo]\nresolution_mode = "latest"\n`);
    assert.strictEqual(q.kargo.resolution_mode, "latest");
  });

  it("parses [kargo] strict", () => {
    const p = parseKargoToml(`[kargo]\nstrict = "true"\n`);
    assert.strictEqual(p.kargo.strict, true);
    const q = parseKargoToml(`[kargo]\nstrict = "false"\n`);
    assert.strictEqual(q.kargo.strict, false);
  });
});
