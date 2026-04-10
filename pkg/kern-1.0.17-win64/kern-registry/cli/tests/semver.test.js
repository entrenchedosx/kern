import test from "node:test";
import assert from "node:assert/strict";
import { maxSatisfyingVersion, satisfiesRange } from "../utils/semver.js";

test("semver exact range", () => {
  assert.equal(satisfiesRange("1.2.3", "1.2.3"), true);
  assert.equal(satisfiesRange("1.2.4", "1.2.3"), false);
});

test("semver caret range", () => {
  assert.equal(satisfiesRange("1.3.0", "^1.2.0"), true);
  assert.equal(satisfiesRange("2.0.0", "^1.2.0"), false);
});

test("semver tilde range", () => {
  assert.equal(satisfiesRange("1.2.9", "~1.2.0"), true);
  assert.equal(satisfiesRange("1.3.0", "~1.2.0"), false);
});

test("max satisfying picks highest valid", () => {
  const versions = ["1.0.0", "1.2.3", "1.3.1", "2.0.0"];
  assert.equal(maxSatisfyingVersion(versions, "^1.0.0"), "1.3.1");
});
