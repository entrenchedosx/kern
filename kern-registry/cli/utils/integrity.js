import crypto from "node:crypto";

export function sha256Hex(buffer) {
  return crypto.createHash("sha256").update(buffer).digest("hex");
}

export function toIntegrity(shaHex) {
  return `sha256-${shaHex}`;
}

export function verifyIntegrity(buffer, integrity) {
  if (!integrity || typeof integrity !== "string") {
    throw new Error("Missing integrity string");
  }
  const expected = integrity.startsWith("sha256-") ? integrity.slice(7) : integrity;
  const actual = sha256Hex(buffer);
  return expected === actual;
}
