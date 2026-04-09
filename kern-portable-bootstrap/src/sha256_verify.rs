//! Parse `kern-SHA256SUMS` (GNU format) and verify files.

use crate::error::{path_ctx, PortableError, Result};
use sha2::{Digest, Sha256};
use std::fs;
use std::io::Read;
use std::path::Path;

pub fn hash_file(path: &Path) -> Result<String> {
    let mut f = fs::File::open(path).map_err(|e| path_ctx(path, e))?;
    let mut h = Sha256::new();
    let mut buf = [0u8; 64 * 1024];
    loop {
        let n = f.read(&mut buf).map_err(|e| path_ctx(path, e))?;
        if n == 0 {
            break;
        }
        h.update(&buf[..n]);
    }
    Ok(format!("{:x}", h.finalize()))
}

pub fn parse_expected_hash(sums_content: &str, basename: &str) -> Option<String> {
    for line in sums_content.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let mut parts = line.split_whitespace();
        let hash = parts.next().unwrap_or("");
        let name = parts.next().unwrap_or("").trim_start_matches('*');
        if name == basename && hash.len() == 64 {
            return Some(hash.to_lowercase());
        }
    }
    None
}

/// Verify `path` matches the line for `basename` in `sums_content`, or error.
pub fn verify_file_against_sums(path: &Path, basename: &str, sums_content: &str) -> Result<()> {
    let expected = parse_expected_hash(sums_content, basename).ok_or_else(|| {
        PortableError::msg(format!(
            "no SHA256 line for `{}` in kern-SHA256SUMS (required for install verification)",
            basename
        ))
    })?;
    let got = hash_file(path)?;
    if !got.eq_ignore_ascii_case(&expected) {
        let _ = fs::remove_file(path);
        return Err(PortableError::msg(format!(
            "checksum mismatch for {}: expected {}, got {} — file removed",
            basename, expected, got
        )));
    }
    Ok(())
}

/// True if `path` exists and its SHA256 matches the line for `basename` in `sums_content`.
/// Does not delete files on mismatch (for cache probing).
pub fn file_matches_sums(path: &Path, basename: &str, sums_content: &str) -> bool {
    if !path.is_file() {
        return false;
    }
    let Some(exp) = parse_expected_hash(sums_content, basename) else {
        return false;
    };
    let Ok(got) = hash_file(path) else {
        return false;
    };
    got.eq_ignore_ascii_case(&exp)
}
