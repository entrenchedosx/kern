import { describe, it } from "node:test";
import assert from "node:assert";
import {
  classifyVersionRef,
  constraintRawFromClassified,
  pickHighestSatisfying,
  pickHighestSatisfyingDetailed,
  normalizeTagList,
  normalizedConstraintIntersection,
  versionSatisfiesConstraint
} from "../../lib/ranges.js";
import { parseDepEntry, constraintFromInstallSpec } from "../../lib/dep-entry.js";
import { parseGithubSpec } from "../../lib/spec.js";

describe("ranges", () => {
  it("classify exact vs range", () => {
    assert.strictEqual(constraintRawFromClassified(classifyVersionRef("v1.2.3")), "=1.2.3");
    assert.strictEqual(constraintRawFromClassified(classifyVersionRef("^1.2.0")), "^1.2.0");
    assert.strictEqual(constraintRawFromClassified(classifyVersionRef(">=1.0.0 <2.0.0")), ">=1.0.0 <2.0.0");
    assert.strictEqual(constraintRawFromClassified(classifyVersionRef(null)), "*");
  });

  it("pickHighestSatisfying with intersection", () => {
    const tags = ["v1.0.0", "v1.5.0", "v1.9.0", "v2.0.0"];
    const best = pickHighestSatisfying(tags, ["^1.2.0", "^1.3.0"]);
    assert.strictEqual(best.sv, "1.9.0");
  });

  it("pickHighestSatisfying returns null on impossible intersection", () => {
    const tags = ["v1.0.0", "v2.0.0"];
    const best = pickHighestSatisfying(tags, ["^1.0.0", "^2.0.0"]);
    assert.strictEqual(best, null);
  });

  it("versionSatisfiesConstraint exact", () => {
    assert.strictEqual(versionSatisfiesConstraint("1.0.0", "=1.0.0"), true);
    assert.strictEqual(versionSatisfiesConstraint("1.0.1", "=1.0.0"), false);
  });

  it("normalizeTagList dedupes and sorts", () => {
    const n = normalizeTagList(["v1.0.0", "v2.0.0", "v1.0.0", "1.5.0"]);
    assert.deepStrictEqual(n, ["v1.0.0", "v1.5.0", "v2.0.0"]);
  });

  it("prerelease fallback when no stable matches range", () => {
    const tags = ["v1.0.0", "v1.2.0-beta.0"];
    const d = pickHighestSatisfyingDetailed(tags, ["^1.1.0"], { allowPrereleaseFallback: true });
    assert.strictEqual(d.choice?.sv, "1.2.0-beta.0");
    assert.strictEqual(d.usedPrereleaseFallback, true);
  });

  it("prefers stable when it satisfies", () => {
    const tags = ["v1.0.0", "v1.2.0", "v1.3.0-beta.0"];
    const d = pickHighestSatisfyingDetailed(tags, ["^1.1.0"], { allowPrereleaseFallback: true });
    assert.strictEqual(d.choice?.sv, "1.2.0");
    assert.strictEqual(d.usedPrereleaseFallback, false);
  });

  it("allowPrereleaseFallback false skips prereleases", () => {
    const tags = ["v1.0.0", "v1.2.0-beta.0"];
    const d = pickHighestSatisfyingDetailed(tags, ["^1.1.0"], { allowPrereleaseFallback: false });
    assert.strictEqual(d.choice, null);
  });

  it("normalizedConstraintIntersection merges carets", () => {
    assert.strictEqual(
      normalizedConstraintIntersection(["^1.2.0", "^1.3.0"], "1.9.0"),
      ">=1.3.0 <2.0.0"
    );
  });

  it("normalizedConstraintIntersection returns null for exact pins in a merge", () => {
    assert.strictEqual(normalizedConstraintIntersection(["^1.0.0", "=1.2.3"], "1.2.3"), null);
  });
});

describe("dep-entry", () => {
  it("owner/repo key with range value", () => {
    const d = parseDepEntry("Acme/Foo", "^1.2.0");
    assert.strictEqual(d.depKey, "acme/foo");
    assert.strictEqual(d.constraintRaw, "^1.2.0");
  });

  it("alias key with full spec", () => {
    const d = parseDepEntry("widgets", "o/r@^2.0.0");
    assert.strictEqual(d.depKey, "o/r");
    assert.strictEqual(d.constraintRaw, "^2.0.0");
  });

  it("constraintFromInstallSpec", () => {
    assert.strictEqual(constraintFromInstallSpec(parseGithubSpec("a/b")), "*");
    assert.strictEqual(constraintFromInstallSpec(parseGithubSpec("a/b@v1.0.0")), "=1.0.0");
    assert.strictEqual(constraintFromInstallSpec(parseGithubSpec("a/b@^1.0.0")), "^1.0.0");
  });
});
