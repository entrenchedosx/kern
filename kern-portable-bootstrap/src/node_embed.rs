//! Bundle Node.js for Windows when missing from PATH (same sources as kern-bootstrap).

use crate::download::download_to_file;
use crate::error::{path_ctx, PortableError, Result};
use crate::extract::{extract_zip, find_single_subdirectory};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

pub const EMBEDDED_NODE_WIN_VERSION_DEFAULT: &str = "20.18.1";

pub fn embedded_node_root(prefix: &Path) -> PathBuf {
    prefix.join("tools").join("nodejs")
}

pub fn embedded_node_exe(prefix: &Path) -> PathBuf {
    embedded_node_root(prefix).join("node.exe")
}

fn system_node_works() -> bool {
    Command::new("node")
        .arg("-e")
        .arg("process.exit(0)")
        .stdin(std::process::Stdio::null())
        .stdout(std::process::Stdio::null())
        .stderr(std::process::Stdio::null())
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

/// Download Node win-x64 zip + SHASUMS256 verify into `<prefix>/tools/nodejs/`.
pub fn ensure_windows_node(prefix: &Path, token: Option<&str>) -> Result<()> {
    if system_node_works() {
        return Ok(());
    }

    let root = embedded_node_root(prefix);
    let exe = embedded_node_exe(prefix);
    if exe.is_file() {
        return Ok(());
    }

    let ver = std::env::var("KERN_PORTABLE_NODE_VERSION")
        .unwrap_or_else(|_| EMBEDDED_NODE_WIN_VERSION_DEFAULT.to_string());
    let zip_name = format!("node-v{ver}-win-x64.zip");
    let url = format!("https://nodejs.org/dist/v{ver}/{zip_name}");
    let sums_url = format!("https://nodejs.org/dist/v{ver}/SHASUMS256.txt");

    let dl = prefix.join(".downloads");
    fs::create_dir_all(&dl).map_err(|e| path_ctx(&dl, e))?;
    let zip_path = dl.join(&zip_name);
    let sums_path = dl.join(format!("SHASUMS256.nodejs-v{}.txt", ver));

    download_to_file(&url, &zip_path, token)?;
    download_to_file(&sums_url, &sums_path, token)?;
    verify_shasum_line(&sums_path, &zip_name, &zip_path)?;

    let tmp = prefix.join(format!(".node-unpack-{ver}"));
    let _ = fs::remove_dir_all(&tmp);
    fs::create_dir_all(&tmp).map_err(|e| path_ctx(&tmp, e))?;
    extract_zip(&zip_path, &tmp)?;
    let inner = find_single_subdirectory(&tmp)?;
    let _ = fs::remove_dir_all(&root);
    fs::create_dir_all(&root).map_err(|e| path_ctx(&root, e))?;
    crate::tree_copy::copy_dir_all(&inner, &root)?;
    let _ = fs::remove_dir_all(&tmp);

    if !embedded_node_exe(prefix).is_file() {
        return Err(PortableError::msg(
            "Node.js install failed: node.exe missing after extract.",
        ));
    }
    Ok(())
}

fn verify_shasum_line(sums: &Path, expect_name: &str, file: &Path) -> Result<()> {
    let text = fs::read_to_string(sums).map_err(|e| path_ctx(sums, e))?;
    use sha2::{Digest, Sha256};
    let mut f = fs::File::open(file).map_err(|e| path_ctx(file, e))?;
    let mut buf = [0u8; 64 * 1024];
    let mut h = Sha256::new();
    use std::io::Read;
    loop {
        let n = f.read(&mut buf).map_err(|e| path_ctx(file, e))?;
        if n == 0 {
            break;
        }
        h.update(&buf[..n]);
    }
    let got = format!("{:x}", h.finalize());

    for line in text.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let mut parts = line.split_whitespace();
        let hash = parts.next().unwrap_or("");
        let name = parts.next().unwrap_or("").trim_start_matches('*');
        if name == expect_name && hash.len() == 64 {
            if !got.eq_ignore_ascii_case(hash) {
                return Err(PortableError::msg(format!(
                    "SHA256 mismatch for {}: expected {}, got {}",
                    expect_name, hash, got
                )));
            }
            return Ok(());
        }
    }
    Err(PortableError::msg(format!(
        "no checksum line for {} in {}",
        expect_name,
        sums.display()
    )))
}
