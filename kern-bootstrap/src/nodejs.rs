//! On Windows, Kargo is launched with Node. Clean machines often have no Node — download official
//! Node.js x64 zip from nodejs.org under `<prefix>/tools/nodejs/` (includes npm).

use crate::error::Result;
use crate::progress::Progress;
use std::path::{Path, PathBuf};
use std::process::Command;

#[cfg(windows)]
use crate::download::{download_to_file, verify_sha256sum_file, DownloadContext};
#[cfg(windows)]
use crate::error::{path_ctx, AppError};
#[cfg(windows)]
use crate::extract::{extract_zip, find_single_subdirectory};
#[cfg(windows)]
use crate::tree_copy::copy_dir_all;
#[cfg(windows)]
use std::fs;

/// Default Node.js version to bundle when `node` is missing (LTS). Override with `KERN_BOOTSTRAP_NODE_VERSION`.
pub const EMBEDDED_NODE_WIN_VERSION_DEFAULT: &str = "20.18.1";

pub fn embedded_node_root(prefix: &Path) -> PathBuf {
    prefix.join("tools").join("nodejs")
}

pub fn embedded_node_exe(prefix: &Path) -> PathBuf {
    embedded_node_root(prefix).join("node.exe")
}

pub fn system_node_works() -> bool {
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

fn node_version_line(exe: &Path) -> Option<String> {
    let o = Command::new(exe)
        .arg("-p")
        .arg("process.version")
        .output()
        .ok()?;
    if !o.status.success() {
        return None;
    }
    let s = String::from_utf8_lossy(&o.stdout).trim().to_string();
    if s.is_empty() {
        return None;
    }
    Some(s.trim_matches('"').to_string())
}

/// If `node` is on PATH, returns `Ok(None)`. Otherwise downloads Node.js win-x64 zip into
/// `<prefix>/tools/nodejs/` and returns `Ok(Some(version))`.
#[cfg(windows)]
pub fn ensure_windows_node_for_kargo(
    prefix: &Path,
    prog: &mut Progress,
    dctx: &mut DownloadContext<'_>,
) -> Result<Option<String>> {
    if system_node_works() {
        prog.info("Using Node.js from PATH for Kargo.");
        return Ok(None);
    }

    let root = embedded_node_root(prefix);
    let exe = embedded_node_exe(prefix);
    if exe.is_file() {
        if let Some(v) = node_version_line(&exe) {
            prog.info(&format!(
                "Using bundled Node.js {} at {}",
                v,
                root.display()
            ));
            return Ok(Some(v));
        }
        let _ = fs::remove_dir_all(&root);
    }

    let ver = std::env::var("KERN_BOOTSTRAP_NODE_VERSION")
        .unwrap_or_else(|_| EMBEDDED_NODE_WIN_VERSION_DEFAULT.to_string());
    let zip_name = format!("node-v{ver}-win-x64.zip");
    let url = format!("https://nodejs.org/dist/v{ver}/{zip_name}");
    let sums_url = format!("https://nodejs.org/dist/v{ver}/SHASUMS256.txt");

    let dl = prefix.join("downloads");
    fs::create_dir_all(&dl).map_err(|e| path_ctx(&dl, e))?;
    let zip_path = dl.join(&zip_name);
    let sums_path = dl.join(format!("SHASUMS256.nodejs-v{}.txt", ver));

    prog.step(&format!(
        "Installing Node.js {} for Kargo (internet download; ~30 MiB)…",
        ver
    ));
    download_to_file(&url, &zip_path, prog, dctx)?;
    download_to_file(&sums_url, &sums_path, prog, dctx)?;
    verify_sha256sum_file(&sums_path, &zip_name, &zip_path)?;

    let tmp = prefix.join(format!(".node-unpack-{ver}"));
    let _ = fs::remove_dir_all(&tmp);
    fs::create_dir_all(&tmp).map_err(|e| path_ctx(&tmp, e))?;
    extract_zip(&zip_path, &tmp)?;
    let inner = find_single_subdirectory(&tmp)?;
    let _ = fs::remove_dir_all(&root);
    fs::create_dir_all(&root).map_err(|e| path_ctx(&root, e))?;
    copy_dir_all(&inner, &root)?;
    let _ = fs::remove_dir_all(&tmp);

    if !embedded_node_exe(prefix).is_file() {
        return Err(AppError::msg(
            "Node.js install failed: node.exe missing after extract — try KERN_BOOTSTRAP_NODE_VERSION or check network.",
        ));
    }

    let reported = node_version_line(&exe).unwrap_or_else(|| ver.clone());
    prog.ok(&format!(
        "Bundled Node.js {} at {} — Kargo will use this (no separate Node installer needed).",
        reported,
        root.display()
    ));
    Ok(Some(reported))
}

#[cfg(not(windows))]
pub fn ensure_windows_node_for_kargo(
    _prefix: &Path,
    _prog: &mut Progress,
    _dctx: &mut DownloadContext<'_>,
) -> Result<Option<String>> {
    Ok(None)
}
