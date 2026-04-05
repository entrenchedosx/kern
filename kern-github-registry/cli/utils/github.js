const GH_API = "https://api.github.com";
const DEFAULT_TIMEOUT_MS = 25000;

function authHeaders(token) {
  const headers = {
    "user-agent": "kern-github-registry-cli",
    "accept": "application/vnd.github+json"
  };
  if (token) headers.authorization = `Bearer ${token}`;
  return headers;
}

async function fetchWithTimeout(url, options = {}, timeoutMs = DEFAULT_TIMEOUT_MS) {
  const controller = new AbortController();
  const id = setTimeout(() => controller.abort(), timeoutMs);
  try {
    return await fetch(url, { ...options, signal: controller.signal });
  } finally {
    clearTimeout(id);
  }
}

export async function ghJson(url, token, options = {}) {
  const headers = {
    ...authHeaders(token),
    ...(options.headers || {})
  };
  let lastErr = null;
  for (let attempt = 1; attempt <= 3; attempt += 1) {
    try {
      const res = await fetchWithTimeout(url, { ...options, headers });
      const text = await res.text();
      let data = {};
      try { data = JSON.parse(text || "{}"); } catch { data = { raw: text }; }
      if (!res.ok) {
        throw new Error(data?.message || `GitHub API error ${res.status}`);
      }
      return data;
    } catch (err) {
      lastErr = err;
      if (attempt < 3) {
        await new Promise((r) => setTimeout(r, 300 * attempt));
        continue;
      }
    }
  }
  throw new Error(lastErr?.message || "GitHub API request failed");
}

export async function ghResponse(url, token, options = {}) {
  const headers = {
    ...authHeaders(token),
    ...(options.headers || {})
  };
  const res = await fetchWithTimeout(url, { ...options, headers });
  if (!res.ok) {
    const text = await res.text();
    let data = {};
    try { data = JSON.parse(text || "{}"); } catch { data = { raw: text }; }
    throw new Error(data?.message || `GitHub request failed (${res.status})`);
  }
  const text = await res.text();
  let data = {};
  try { data = JSON.parse(text || "{}"); } catch { data = { raw: text }; }
  return { res, data };
}

export async function getRepoFile(owner, repo, ref, filePath, token) {
  const encoded = filePath.split("/").map(encodeURIComponent).join("/");
  const url = `${GH_API}/repos/${owner}/${repo}/contents/${encoded}?ref=${encodeURIComponent(ref)}`;
  return ghJson(url, token);
}

export async function putRepoFile(owner, repo, filePath, token, payload) {
  const encoded = filePath.split("/").map(encodeURIComponent).join("/");
  const url = `${GH_API}/repos/${owner}/${repo}/contents/${encoded}`;
  return ghJson(url, token, {
    method: "PUT",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(payload)
  });
}

export async function getReleaseByTag(owner, repo, tag, token) {
  const url = `${GH_API}/repos/${owner}/${repo}/releases/tags/${encodeURIComponent(tag)}`;
  return ghJson(url, token);
}

export async function createRelease(owner, repo, tag, token) {
  const url = `${GH_API}/repos/${owner}/${repo}/releases`;
  return ghJson(url, token, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({
      tag_name: tag,
      name: tag,
      generate_release_notes: false,
      draft: false,
      prerelease: false
    })
  });
}

export async function uploadReleaseAsset(owner, repo, releaseId, assetName, bytes, token) {
  const url = `https://uploads.github.com/repos/${owner}/${repo}/releases/${releaseId}/assets?name=${encodeURIComponent(assetName)}`;
  const res = await fetchWithTimeout(url, {
    method: "POST",
    headers: {
      ...authHeaders(token),
      "content-type": "application/octet-stream",
      "content-length": String(bytes.length)
    },
    body: bytes
  });
  const text = await res.text();
  let data = {};
  try { data = JSON.parse(text || "{}"); } catch { data = { raw: text }; }
  if (!res.ok) {
    throw new Error(data?.message || `GitHub upload error ${res.status}`);
  }
  return data;
}

export function splitRepo(full) {
  const s = String(full || "").trim();
  const i = s.indexOf("/");
  if (i <= 0 || i >= s.length - 1) throw new Error("registryRepo must be owner/repo");
  return { owner: s.slice(0, i), repo: s.slice(i + 1) };
}

export async function getOrCreateRelease(owner, repo, tag, token) {
  try {
    return await getReleaseByTag(owner, repo, tag, token);
  } catch {
    return createRelease(owner, repo, tag, token);
  }
}

export async function validateTokenAndScopes(token) {
  if (!token) throw new Error("GitHub token is required");
  const { res, data } = await ghResponse(`${GH_API}/user`, token, {});
  const scopes = String(res.headers.get("x-oauth-scopes") || "");
  const hasRepoScope = scopes.split(",").map((s) => s.trim()).includes("repo");
  if (!hasRepoScope) {
    throw new Error("GitHub token is valid but missing `repo` scope");
  }
  return {
    login: data?.login || "",
    scopes
  };
}

export async function deleteReleaseAssetByName(owner, repo, release, assetName, token) {
  const target = (release?.assets || []).find((a) => a.name === assetName);
  if (!target) return false;
  const url = `${GH_API}/repos/${owner}/${repo}/releases/assets/${target.id}`;
  const headers = authHeaders(token);
  headers.accept = "application/vnd.github+json";
  const res = await fetchWithTimeout(url, { method: "DELETE", headers });
  if (res.status === 204) return true;
  const text = await res.text();
  throw new Error(`Failed deleting release asset '${assetName}': ${text || res.status}`);
}
