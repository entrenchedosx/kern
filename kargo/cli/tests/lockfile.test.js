import { describe, it } from "node:test";
import assert from "node:assert";
import { sortLockForWrite } from "../../lib/lockfile.js";

describe("lockfile", () => {
  it("sortLockForWrite orders packages and entry keys deterministically", () => {
    const j = sortLockForWrite({
      lockVersion: 2,
      packages: {
        "z/z": {
          semver: "2.0.0",
          root: "/z",
          commit_sha: "bbb",
          resolved_tag: "v2.0.0",
          resolved_constraints: ["^2.0.0"],
          resolved_from: "x",
          resolved_version_range: "^2.0.0"
        },
        "a/a": {
          semver: "1.0.0",
          root: "/a",
          commit_sha: "aaa",
          resolved_tag: "v1.0.0",
          resolved_constraints: ["*"],
          resolved_from: "y",
          resolved_version_range: "*"
        }
      }
    });
    assert.deepStrictEqual(Object.keys(j.packages), ["a/a", "z/z"]);
    assert.deepStrictEqual(Object.keys(j.packages["a/a"]), [
      "commit_sha",
      "resolved_constraints",
      "resolved_from",
      "resolved_tag",
      "resolved_version_range",
      "root",
      "semver"
    ]);
  });

  it("sortLockForWrite preserves unknown package keys after known ones", () => {
    const j = sortLockForWrite({
      lockVersion: 2,
      packages: {
        "a/a": {
          semver: "1.0.0",
          root: "/a",
          commit_sha: "a",
          resolved_tag: "v1.0.0",
          resolved_constraints: [],
          resolved_from: "",
          resolved_version_range: "*",
          future_field: 1
        }
      }
    });
    const keys = Object.keys(j.packages["a/a"]);
    assert.strictEqual(keys[keys.length - 1], "future_field");
  });
});
