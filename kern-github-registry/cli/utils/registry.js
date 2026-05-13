import { getRepoFile, putRepoFile } from "./github.js";

function encodeJson(obj) {
  return Buffer.from(`${JSON.stringify(obj, null, 2)}\n`, "utf8").toString("base64");
}

function decodeContent(content) {
  return Buffer.from(String(content || "").replace(/\n/g, ""), "base64").toString("utf8");
}

export async function readRegistryIndex(owner, repo, ref, token) {
  try {
    const res = await getRepoFile(owner, repo, ref, "registry/index.json", token);
    const parsed = JSON.parse(decodeContent(res.content));
    return { sha: res.sha, index: parsed };
  } catch {
    return {
      sha: null,
      index: { registryVersion: 1, generatedAt: new Date().toISOString(), packages: {} }
    };
  }
}

export async function readPackageMetadata(owner, repo, ref, token, pkgName) {
  const p = `registry/packages/${pkgName}.json`;
  try {
    const res = await getRepoFile(owner, repo, ref, p, token);
    return { sha: res.sha, metadata: JSON.parse(decodeContent(res.content)) };
  } catch {
    return {
      sha: null,
      metadata: {
        name: pkgName,
        description: "",
        latest: "",
        trusted: false,
        versions: {}
      }
    };
  }
}

export async function writeRegistryIndex(owner, repo, token, index, sha = null) {
  return putRepoFile(owner, repo, "registry/index.json", token, {
    message: "chore(registry): update index",
    content: encodeJson(index),
    ...(sha ? { sha } : {})
  });
}

export async function writePackageMetadata(owner, repo, token, pkgName, metadata, sha = null) {
  return putRepoFile(owner, repo, `registry/packages/${pkgName}.json`, token, {
    message: `chore(registry): update ${pkgName} metadata`,
    content: encodeJson(metadata),
    ...(sha ? { sha } : {})
  });
}

