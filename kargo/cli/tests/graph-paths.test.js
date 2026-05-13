import test from "node:test";
import assert from "node:assert/strict";
import { pathsIntroducingPackage } from "../../lib/graph.js";

test("pathsIntroducingPackage finds direct and transitive paths", () => {
  const V = "__project__";
  const edges = [
    { from: V, to: "a/a", constraint: "*" },
    { from: V, to: "b/b", constraint: "^1" },
    { from: "a/a", to: "c/c", constraint: "*" },
    { from: "b/b", to: "c/c", constraint: "^2" }
  ];
  const paths = pathsIntroducingPackage(edges, V, "c/c");
  assert.equal(paths.length, 2);
});

test("pathsIntroducingPackage returns empty when unreachable", () => {
  const V = "__project__";
  const edges = [{ from: V, to: "a/a", constraint: "*" }];
  const paths = pathsIntroducingPackage(edges, V, "z/z");
  assert.equal(paths.length, 0);
});
