//! Minimal GitHub Releases API (same shape as kern-bootstrap).

use crate::error::{PortableError, Result};
use serde_json::Value;

const GH_API: &str = "https://api.github.com";

#[derive(Debug, Clone)]
pub struct ReleaseInfo {
    pub tag_name: String,
    pub assets: Vec<ReleaseAsset>,
}

#[derive(Debug, Clone)]
pub struct ReleaseAsset {
    pub name: String,
    pub browser_download_url: String,
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
            "kern-portable-bootstrap/",
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
    let want = want.trim();
    if want.eq_ignore_ascii_case("nightly") {
        return fetch_latest_prerelease(repo, token);
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
        let name = a
            .get("name")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string();
        let browser_download_url = a
            .get("browser_download_url")
            .and_then(|x| x.as_str())
            .unwrap_or("")
            .to_string();
        if !name.is_empty() && browser_download_url.starts_with("https://") {
            assets.push(ReleaseAsset {
                name,
                browser_download_url,
            });
        }
    }

    Ok(ReleaseInfo { tag_name, assets })
}

/// Newest prerelease (e.g. for `--nightly`).
pub fn fetch_latest_prerelease(repo: &str, token: Option<&str>) -> Result<ReleaseInfo> {
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
            let name = a
                .get("name")
                .and_then(|x| x.as_str())
                .unwrap_or("")
                .to_string();
            let browser_download_url = a
                .get("browser_download_url")
                .and_then(|x| x.as_str())
                .unwrap_or("")
                .to_string();
            if !name.is_empty() && browser_download_url.starts_with("https://") {
                assets.push(ReleaseAsset {
                    name,
                    browser_download_url,
                });
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
