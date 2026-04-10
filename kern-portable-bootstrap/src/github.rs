//! Minimal GitHub Releases API (same shape as kern-bootstrap).

use crate::error::{PortableError, Result};
use serde_json::Value;

const GH_API: &str = "https://api.github.com";

/// Standalone installer repo (may have no releases until published).
pub const PRIMARY_INSTALLER_REPO: &str = "entrenchedosx/kern-installer-src";
/// Main Kern repo; compiler/runtime/kargo assets are published here.
pub const FALLBACK_RELEASE_REPO: &str = "entrenchedosx/kern";

fn should_fallback_to_kern(repo: &str, err: &PortableError) -> bool {
    if repo != PRIMARY_INSTALLER_REPO {
        return false;
    }
    match err {
        PortableError::Network(msg) => msg.contains("404") || msg.contains("Not Found"),
        PortableError::Msg(msg) => msg.contains("no prerelease found"),
        _ => false,
    }
}

#[derive(Debug, Clone)]
pub struct ReleaseInfo {
    pub tag_name: String,
    pub assets: Vec<ReleaseAsset>,
}

#[derive(Debug, Clone)]
pub struct ReleaseAsset {
    pub name: String,
    pub browser_download_url: String,
    /// GitHub REST `digest` on release assets (`sha256:...`), when returned by the API.
    pub digest_sha256: Option<String>,
}

fn release_asset_from_json(a: &Value) -> Option<ReleaseAsset> {
    let name = a.get("name")?.as_str()?;
    let browser_download_url = a.get("browser_download_url")?.as_str()?;
    if name.is_empty() || !browser_download_url.starts_with("https://") {
        return None;
    }
    let digest_sha256 = a
        .get("digest")
        .and_then(|d| d.as_str())
        .and_then(|s| {
            let hex = s.strip_prefix("sha256:")?.trim();
            if hex.len() == 64 && hex.chars().all(|c| c.is_ascii_hexdigit()) {
                Some(hex.to_lowercase())
            } else {
                None
            }
        });
    Some(ReleaseAsset {
        name: name.to_string(),
        browser_download_url: browser_download_url.to_string(),
        digest_sha256,
    })
}

fn client_headers(token: Option<&str>) -> reqwest::header::HeaderMap {
    use reqwest::header::{HeaderMap, HeaderValue, ACCEPT, USER_AGENT};
    let mut h = HeaderMap::new();
    h.insert(
        ACCEPT,
        HeaderValue::from_static("application/vnd.github+json"),
    );
    h.insert(
        USER_AGENT,
        HeaderValue::from_static(concat!(
            "kern-installer-src/",
            env!("KERN_PORTABLE_CRATE_VER")
        )),
    );
    h.insert("X-GitHub-Api-Version", HeaderValue::from_static("2022-11-28"));
    if let Some(t) = token.filter(|s| !s.is_empty()) {
        if let Ok(v) = HeaderValue::try_from(format!("Bearer {}", t)) {
            h.insert(reqwest::header::AUTHORIZATION, v);
        }
    }
    h
}

pub fn fetch_release(repo: &str, want: &str, token: Option<&str>) -> Result<ReleaseInfo> {
    match fetch_release_direct(repo, want, token) {
        Ok(info) => Ok(info),
        Err(e) if should_fallback_to_kern(repo, &e) => {
            eprintln!(
                "note: {} is unavailable or has no matching release; using {}.",
                PRIMARY_INSTALLER_REPO, FALLBACK_RELEASE_REPO
            );
            fetch_release_direct(FALLBACK_RELEASE_REPO, want, token)
        }
        Err(e) => Err(e),
    }
}

fn fetch_release_direct(repo: &str, want: &str, token: Option<&str>) -> Result<ReleaseInfo> {
    let want = want.trim();
    if want.eq_ignore_ascii_case("nightly") {
        return fetch_latest_prerelease_direct(repo, token);
    }
    let url = if want == "latest" || want.is_empty() {
        format!("{}/repos/{}/releases/latest", GH_API, repo)
    } else {
        let tag = if want.starts_with('v') {
            want.to_string()
        } else {
            format!("v{}", want)
        };
        format!(
            "{}/repos/{}/releases/tags/{}",
            GH_API,
            repo,
            encode_tag(&tag)
        )
    };

    let client = reqwest::blocking::Client::builder()
        .timeout(std::time::Duration::from_secs(120))
        .build()
        .map_err(|e| PortableError::Network(e.to_string()))?;

    let resp = client
        .get(&url)
        .headers(client_headers(token))
        .send()
        .map_err(|e| PortableError::Network(e.to_string()))?;

    if !resp.status().is_success() {
        let body = resp.text().unwrap_or_default();
        return Err(PortableError::Network(format!(
            "GitHub API {}: {}",
            url,
            body.chars().take(500).collect::<String>()
        )));
    }

    let v: Value = resp.json().map_err(|e| PortableError::Network(e.to_string()))?;
    let tag_name = v
        .get("tag_name")
        .and_then(|x| x.as_str())
        .ok_or_else(|| PortableError::msg("release missing tag_name"))?
        .to_string();

    let assets_arr = v
        .get("assets")
        .and_then(|a| a.as_array())
        .ok_or_else(|| PortableError::msg("release missing assets"))?;

    let mut assets = Vec::new();
    for a in assets_arr {
        if let Some(ra) = release_asset_from_json(a) {
            assets.push(ra);
        }
    }

    Ok(ReleaseInfo { tag_name, assets })
}

/// Newest prerelease (e.g. for `--nightly`).
pub fn fetch_latest_prerelease(repo: &str, token: Option<&str>) -> Result<ReleaseInfo> {
    match fetch_latest_prerelease_direct(repo, token) {
        Ok(info) => Ok(info),
        Err(e) if should_fallback_to_kern(repo, &e) => {
            eprintln!(
                "note: {} is unavailable or has no matching prerelease; using {}.",
                PRIMARY_INSTALLER_REPO, FALLBACK_RELEASE_REPO
            );
            fetch_latest_prerelease_direct(FALLBACK_RELEASE_REPO, token)
        }
        Err(e) => Err(e),
    }
}

fn fetch_latest_prerelease_direct(repo: &str, token: Option<&str>) -> Result<ReleaseInfo> {
    let url = format!("{}/repos/{}/releases?per_page=30", GH_API, repo);
    let client = reqwest::blocking::Client::builder()
        .timeout(std::time::Duration::from_secs(120))
        .build()
        .map_err(|e| PortableError::Network(e.to_string()))?;

    let resp = client
        .get(&url)
        .headers(client_headers(token))
        .send()
        .map_err(|e| PortableError::Network(e.to_string()))?;

    if !resp.status().is_success() {
        let body = resp.text().unwrap_or_default();
        return Err(PortableError::Network(format!(
            "GitHub API {}: {}",
            url,
            body.chars().take(500).collect::<String>()
        )));
    }

    let arr: Vec<Value> = resp.json().map_err(|e| PortableError::Network(e.to_string()))?;
    for v in arr {
        let prerelease = v.get("prerelease").and_then(|x| x.as_bool()).unwrap_or(false);
        if !prerelease {
            continue;
        }
        let tag_name = v
            .get("tag_name")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string();
        if tag_name.is_empty() {
            continue;
        }
        let assets_arr = v
            .get("assets")
            .and_then(|a| a.as_array())
            .ok_or_else(|| PortableError::msg("release missing assets"))?;

        let mut assets = Vec::new();
        for a in assets_arr {
            if let Some(ra) = release_asset_from_json(a) {
                assets.push(ra);
            }
        }
        return Ok(ReleaseInfo { tag_name, assets });
    }

    Err(PortableError::msg(
        "no prerelease found on this repo (try --release latest or an explicit tag)",
    ))
}

pub fn find_asset<'a>(release: &'a ReleaseInfo, pat: &str) -> Option<&'a ReleaseAsset> {
    release.assets.iter().find(|a| a.name == pat)
}

fn encode_tag(s: &str) -> String {
    let mut out = String::new();
    for c in s.chars() {
        match c {
            'a'..='z' | 'A'..='Z' | '0'..='9' | '-' | '_' | '.' | '~' => out.push(c),
            _ => out.push_str(&format!("%{:02X}", c as u32)),
        }
    }
    out
}
