use crate::error::{path_ctx, PortableError, Result};
use std::fs::File;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::time::Duration;

const CONNECT_TIMEOUT: Duration = Duration::from_secs(30);
const READ_TIMEOUT: Duration = Duration::from_secs(600);

pub fn download_to_file(url: &str, dest: &Path, token: Option<&str>) -> Result<()> {
    let client = reqwest::blocking::Client::builder()
        .connect_timeout(CONNECT_TIMEOUT)
        .timeout(READ_TIMEOUT)
        .build()
        .map_err(|e| PortableError::Network(e.to_string()))?;

    let mut req = client.get(url);
    if let Some(t) = token.filter(|s| !s.is_empty()) {
        req = req.header(
            reqwest::header::AUTHORIZATION,
            format!("Bearer {}", t),
        );
    }
    req = req.header(
        reqwest::header::USER_AGENT,
        concat!("kern-portable-bootstrap/", env!("KERN_PORTABLE_CRATE_VER")),
    );

    let resp = req
        .send()
        .map_err(|e| PortableError::Network(e.to_string()))?;

    if !resp.status().is_success() {
        return Err(PortableError::Network(format!(
            "HTTP {} for {}",
            resp.status(),
            url
        )));
    }

    if let Some(parent) = dest.parent() {
        std::fs::create_dir_all(parent).map_err(|e| path_ctx(parent, e))?;
    }

    let part: PathBuf = format!("{}.part", dest.display()).into();
    let mut file = File::create(&part).map_err(|e| path_ctx(&part, e))?;
    let bytes = resp.bytes().map_err(|e| PortableError::Network(e.to_string()))?;
    file.write_all(&bytes).map_err(|e| path_ctx(&part, e))?;
    drop(file);
    std::fs::rename(&part, dest).map_err(|e| path_ctx(dest, e))?;
    Ok(())
}
