import { describe, it } from "node:test";
import assert from "node:assert";
import {
  minimalUnsatisfiableCore,
  isConstraintSetUnsatisfiable
} from "../../lib/conflict-core.js";

describe("conflict-core", () => {
  const tags = ["v1.0.0", "v1.5.0", "v2.0.0"];

  it("detects unsatisfiable pair ^1 vs ^2", () => {
    const raws = ["^1.0.0", "^2.0.0"];
    assert.strictEqual(isConstraintSetUnsatisfiable(raws, tags, {}), true);
    const core = minimalUnsatisfiableCore(raws, tags, {});
    assert.deepStrictEqual(core, ["^1.0.0", "^2.0.0"]);
  });

  it("drops constraints redundant for unsat (stricter caret subsumes wider)", () => {
    const raws = ["^1.0.0", "^1.2.0", "^2.0.0"];
    assert.strictEqual(isConstraintSetUnsatisfiable(raws, tags, {}), true);
    const core = minimalUnsatisfiableCore(raws, tags, {});
    assert.deepStrictEqual(core, ["^1.2.0", "^2.0.0"]);
  });

  it("returns null when satisfiable", () => {
    assert.strictEqual(minimalUnsatisfiableCore(["^1.0.0"], tags, {}), null);
  });
});
