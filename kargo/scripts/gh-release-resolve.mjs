/**
 * Print JSON with download URLs for Kargo release assets on GitHub.
 * Env: KARGO_RELEASE_REPO (owner/repo), KARGO_VERSION_REQUEST ("latest" or "v1.0.0" / "1.0.0")
 * Optional: GITHUB_TOKEN (higher rate limits / private repos)
 */
import https from "node:https";

function ghJson(url, headers) {
  return new Promise((resolve, reject) => {
    const u = new URL(url);
    const opts = {
      hostname: u.hostname,
      path: u.pathname + u.search,
      method: "GET",
      headers
    };
    const req = https.request(opts, (res) => {
      let data = "";
      res.setEncoding("utf8");
      res.on("data", (c) => {
        data += c;
      });
      res.on("end", () => {
        if (res.statusCode && res.statusCode >= 400) {
          reject(new Error(`GitHub API ${res.statusCode}: ${data.slice(0, 400)}`));
          return;
        }
        try {
          resolve(JSON.parse(data));
        } catch (err) {
          reject(err);
        }
      });
    });
    req.on("error", reject);
    req.end();
  });
}

async function main() {
  const repo = process.env.KARGO_RELEASE_REPO || "";
  const want = (process.env.KARGO_VERSION_REQUEST || "latest").trim();
  const token = (process.env.GITHUB_TOKEN || "").trim();

  if (!/^[a-zA-Z0-9_.-]+\/[a-zA-Z0-9_.-]+$/.test(repo)) {
    console.error("gh-release-resolve: set KARGO_RELEASE_REPO=owner/repo");
    process.exit(1);
  }

  const headers = {
    Accept: "application/vnd.github+json",
    "User-Agent": "kargo-install-from-release",
    "X-GitHub-Api-Version": "2022-11-28"
  };
  if (token) headers.Authorization = `Bearer ${token}`;

  let release;
  if (want === "latest") {
    release = await ghJson(`https://api.github.com/repos/${repo}/releases/latest`, headers);
  } else {
    const tag = want.startsWith("v") ? want : `v${want}`;
    release = await ghJson(
      `https://api.github.com/repos/${repo}/releases/tags/${encodeURIComponent(tag)}`,
      headers
    );
  }

  const assets = release.assets || [];
  const tarball = assets.find((a) => /^kargo-v.+\.tar\.gz$/.test(a.name));
  const sums = assets.find((a) => a.name === "kargo-SHA256SUMS");
  const manifest = assets.find((a) => a.name === "kargo-release.json");

  if (!tarball || !sums) {
    const names = assets.map((a) => a.name).join(", ") || "(none)";
    throw new Error(
      `Release ${release.tag_name} missing kargo bundle (need kargo-v*.tar.gz + kargo-SHA256SUMS). Assets: ${names}`
    );
  }

  function assertHttpsAssetUrl(u, label) {
    let parsed;
    try {
      parsed = new URL(u);
    } catch {
      throw new Error(`${label}: invalid download URL`);
    }
    if (parsed.protocol !== "https:") {
      throw new Error(`${label}: download URL must use https`);
    }
    const h = parsed.hostname.toLowerCase();
    const allowed =
      h === "github.com" ||
      h === "codeload.github.com" ||
      h.endsWith(".github.com") ||
      h.endsWith(".githubusercontent.com");
    if (!allowed) {
      throw new Error(`${label}: unexpected download host (${h})`);
    }
  }

  assertHttpsAssetUrl(tarball.browser_download_url, "tarball");
  assertHttpsAssetUrl(sums.browser_download_url, "checksums");
  if (manifest) {
    assertHttpsAssetUrl(manifest.browser_download_url, "manifest");
  }

  console.log(
    JSON.stringify({
      tag_name: release.tag_name,
      tarball_name: tarball.name,
      tarball_url: tarball.browser_download_url,
      sums_url: sums.browser_download_url,
      manifest_url: manifest ? manifest.browser_download_url : ""
    })
  );
}

main().catch((e) => {
  console.error(e instanceof Error ? e.message : e);
  process.exit(1);
});
