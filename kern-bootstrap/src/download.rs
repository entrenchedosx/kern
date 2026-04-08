use crate::error::{path_ctx, AppError, Result};
use crate::progress::Progress;
use sha2::{Digest, Sha256};
use std::collections::HashSet;
use std::fs::File;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::time::{Duration, Instant};

const RETRIES: u32 = 4;
const CONNECT_TIMEOUT: Duration = Duration::from_secs(30);
const READ_TIMEOUT: Duration = Duration::from_secs(600);

pub struct DownloadOptions<'a> {
    pub token: Option<&'a str>,
    pub mirrors: &'a [String],
}

/// Session blacklist for failing mirror URLs (exact URL keys).
#[derive(Default)]
pub struct MirrorTracker {
    bad: HashSet<String>,
}

impl MirrorTracker {
    fn key(url: &str) -> String {
        url.split('?').next().unwrap_or(url).to_string()
    }

    pub fn blacklist(&mut self, url: &str) {
        self.bad.insert(Self::key(url));
    }

    pub fn is_bad(&self, url: &str) -> bool {
        self.bad.contains(&Self::key(url))
    }
}

pub struct DownloadContext<'a> {
    pub opts: DownloadOptions<'a>,
    pub mirrors: &'a mut MirrorTracker,
}

pub fn download_to_file(
    url: &str,
    dest: &Path,
    prog: &Progress,
    ctx: &mut DownloadContext<'_>,
) -> Result<()> {
    let fname = dest
        .file_name()
        .map(|s| s.to_os_string())
        .unwrap_or_else(|| "download".into());
    let mut part_name = fname;
    part_name.push(".part");
    let part = dest.with_file_name(part_name);

    for attempt in 0..RETRIES {
        if attempt > 0 {
            prog.warn(&format!(
                "download retry {}/{} for {}",
                attempt,
                RETRIES - 1,
                url
            ));
            std::thread::sleep(Duration::from_millis(500 * attempt as u64));
        }

        let urls = build_url_list_ranked(url, ctx.opts.mirrors, ctx.mirrors);
        let mut last_err: Option<String> = None;

        for try_url in urls {
            if ctx.mirrors.is_bad(&try_url) {
                continue;
            }
            let t0 = Instant::now();
            prog.info(&format!("GET {}", try_url));
            match download_once(&try_url, &part, ctx.opts.token) {
                Ok(()) => {
                    let _ = t0.elapsed();
                    std::fs::rename(&part, dest).map_err(|e| path_ctx(dest, e))?;
                    return Ok(());
                }
                Err(e) => {
                    last_err = Some(e.to_string());
                    ctx.mirrors.blacklist(&try_url);
                    let _ = std::fs::remove_file(&part);
                }
            }
        }

        if let Some(e) = last_err {
            prog.warn(&e);
        }
    }

    Err(AppError::Network(format!(
        "failed after {} attempts: {}",
        RETRIES, url
    )))
}

/// Priority: user mirrors (in order), then primary GitHub URL.
fn build_url_list_ranked(
    primary: &str,
    user_mirrors: &[String],
    tracker: &MirrorTracker,
) -> Vec<String> {
    let mut out = Vec::new();
    let mut seen = HashSet::new();
    let mut push = |u: String| {
        if seen.insert(u.clone()) && !tracker.is_bad(&u) {
            out.push(u);
        }
    };
    for m in user_mirrors {
        if m.is_empty() {
            continue;
        }
        if let Some(rest) = primary.strip_prefix("https://github.com/") {
            push(format!("{}{}", m.trim_end_matches('/'), rest));
        }
    }
    push(primary.to_string());
    out
}

fn download_once(url: &str, part: &Path, token: Option<&str>) -> Result<()> {
    let client = reqwest::blocking::Client::builder()
        .connect_timeout(CONNECT_TIMEOUT)
        .timeout(READ_TIMEOUT)
        .build()
        .map_err(|e| AppError::Network(e.to_string()))?;

    let mut req = client.get(url);
    if let Some(t) = token.filter(|s| !s.is_empty()) {
        req = req.header(
            reqwest::header::AUTHORIZATION,
            format!("Bearer {}", t),
        );
    }
    req = req.header(reqwest::header::USER_AGENT, "kern-bootstrap/0.1");

    let mut resp = req
        .send()
        .map_err(|e| AppError::Network(e.to_string()))?;

    if !resp.status().is_success() {
        return Err(AppError::Network(format!(
            "HTTP {} for {}",
            resp.status(),
            url
        )));
    }

    if let Some(parent) = part.parent() {
        std::fs::create_dir_all(parent).map_err(|e| path_ctx(&parent.to_path_buf(), e))?;
    }

    let mut file = File::create(part).map_err(|e| path_ctx(part, e))?;

    let mut buf = [0u8; 64 * 1024];
    loop {
        let n = resp
            .read(&mut buf)
            .map_err(|e| AppError::Network(e.to_string()))?;
        if n == 0 {
            break;
        }
        file.write_all(&buf[..n])
            .map_err(|e| path_ctx(part, e))?;
    }

    Ok(())
}

pub fn sha256_file(path: &Path) -> Result<String> {
    let mut f = File::open(path).map_err(|e| path_ctx(&path.to_path_buf(), e))?;
    let mut h = Sha256::new();
    let mut buf = [0u8; 64 * 1024];
    loop {
        let n = f.read(&mut buf).map_err(|e| path_ctx(&path.to_path_buf(), e))?;
        if n == 0 {
            break;
        }
        h.update(&buf[..n]);
    }
    Ok(hex::encode(h.finalize()))
}

pub fn verify_sha256sum_file(
    checksums_path: &Path,
    tarball_name: &str,
    tarball_path: &Path,
) -> Result<()> {
    let expected = parse_sha256sums_file(checksums_path, tarball_name)?;
    let got = sha256_file(tarball_path)?;
    if !got.eq_ignore_ascii_case(&expected) {
        return Err(AppError::Checksum {
            path: tarball_path.display().to_string(),
            expected,
            got,
        });
    }
    Ok(())
}

fn parse_sha256sums_file(path: &Path, tarball_name: &str) -> Result<String> {
    let raw = std::fs::read_to_string(path).map_err(|e| path_ctx(&path.to_path_buf(), e))?;
    for line in raw.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let mut parts = line.split_whitespace();
        let hash = parts.next().unwrap_or("");
        let name = parts.next().unwrap_or("");
        let name = name.trim_start_matches('*');
        if name == tarball_name && hash.len() == 64 {
            return Ok(hash.to_lowercase());
        }
    }
    Err(AppError::msg(format!(
        "no checksum line for {} in {}",
        tarball_name,
        path.display()
    )))
}

pub fn downloads_dir(prefix: &Path) -> PathBuf {
    prefix.join("downloads")
}
