import crypto from "node:crypto";

export function sha256Hex(buf) {
  return crypto.createHash("sha256").update(buf).digest("hex");
}

export function toIntegrity(hex) {
  return `sha256-${hex}`;
}

export function verifyIntegrity(buf, integrity) {
  const got = toIntegrity(sha256Hex(buf));
  return got === String(integrity || "");
}
