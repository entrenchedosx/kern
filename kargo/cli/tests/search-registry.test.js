import { describe, it } from "node:test";
import assert from "node:assert";
import { scoreRegistryName } from "../../lib/search.js";

describe("scoreRegistryName", () => {
  it("ranks sec.* ahead of unrelated names for query sec", () => {
    const names = ["spring-security", "sec.auth", "web.rest", "sec.crypto", "ai.client"];
    const q = "sec";
    const sorted = names.sort((a, b) => scoreRegistryName(q, a) - scoreRegistryName(q, b));
    assert.deepStrictEqual(sorted.slice(0, 2).sort(), ["sec.auth", "sec.crypto"]);
  });

  it("exact match wins", () => {
    assert.ok(scoreRegistryName("sec", "sec") < scoreRegistryName("sec", "sec.auth"));
  });
});
