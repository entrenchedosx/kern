use crate::error::{AppError, Result};
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
    /// Present in GitHub API JSON; not used for selection logic.
    #[allow(dead_code)]
    pub size: u64,
}

fn client_headers(token: Option<&str>) -> reqwest::header::HeaderMap {
    use reqwest::header::{HeaderMap, HeaderValue, ACCEPT, USER_AGENT};
    let mut h = HeaderMap::new();
    h.insert(
        ACCEPT,
        HeaderValue::from_static("application/vnd.github+json"),
    );
    h.insert(USER_AGENT, HeaderValue::from_static("kern-bootstrap/0.1"));
    h.insert("X-GitHub-Api-Version", HeaderValue::from_static("2022-11-28"));
    if let Some(t) = token.filter(|s| !s.is_empty()) {
        if let Ok(v) = HeaderValue::try_from(format!("Bearer {}", t)) {
            h.insert(reqwest::header::AUTHORIZATION, v);
        }
    }
    h
}

pub fn fetch_release(repo: &str, want: &str, token: Option<&str>) -> Result<ReleaseInfo> {
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
            urlencoding::encode(&tag)
        )
    };

    let client = reqwest::blocking::Client::builder()
        .timeout(std::time::Duration::from_secs(120))
        .build()
        .map_err(|e| AppError::Network(e.to_string()))?;

    let resp = client
        .get(&url)
        .headers(client_headers(token))
        .send()
        .map_err(|e| AppError::Network(e.to_string()))?;

    if !resp.status().is_success() {
        let body = resp.text().unwrap_or_default();
        return Err(AppError::Network(format!(
            "GitHub API {}: {}",
            url,
            body.chars().take(500).collect::<String>()
        )));
    }

    let v: Value = resp.json().map_err(|e| AppError::Network(e.to_string()))?;
    let tag_name = v
        .get("tag_name")
        .and_then(|x| x.as_str())
        .ok_or_else(|| AppError::msg("release missing tag_name"))?
        .to_string();

    let assets_arr = v
        .get("assets")
        .and_then(|a| a.as_array())
        .ok_or_else(|| AppError::msg("release missing assets"))?;

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
        let size = a.get("size").and_then(|x| x.as_u64()).unwrap_or(0);
        if !name.is_empty() && browser_download_url.starts_with("https://") {
            assets.push(ReleaseAsset {
                name,
                browser_download_url,
                size,
            });
        }
    }

    Ok(ReleaseInfo { tag_name, assets })
}

pub fn find_asset<'a>(release: &'a ReleaseInfo, pat: &str) -> Option<&'a ReleaseAsset> {
    release.assets.iter().find(|a| a.name == pat)
}

// small encoder for tag in URL (avoid extra dep for only this)
mod urlencoding {
    pub fn encode(s: &str) -> String {
        let mut out = String::new();
        for c in s.chars() {
            match c {
                'a'..='z' | 'A'..='Z' | '0'..='9' | '-' | '_' | '.' | '~' => out.push(c),
                _ => out.push_str(&format!("%{:02X}", c as u32)),
            }
        }
        out
    }
}
