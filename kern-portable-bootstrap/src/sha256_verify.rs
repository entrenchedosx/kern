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

fn is_hex64(s: &str) -> bool {
    s.len() == 64 && s.chars().all(|c| c.is_ascii_hexdigit())
}

/// True if the path-ish field from a `sha256sum` line refers to `basename` (GNU `*path` or plain name).
fn name_field_matches_basename(name_field: &str, basename: &str) -> bool {
    let name = name_field.trim().trim_start_matches('*').trim();
    if name.is_empty() {
        return false;
    }
    if name == basename || name.eq_ignore_ascii_case(basename) {
        return true;
    }
    Path::new(name)
        .file_name()
        .and_then(|s| s.to_str())
        .map(|f| f == basename || f.eq_ignore_ascii_case(basename))
        .unwrap_or(false)
}

pub fn parse_expected_hash(sums_content: &str, basename: &str) -> Option<String> {
    let sums_content = sums_content.strip_prefix('\u{feff}').unwrap_or(sums_content);
    for raw in sums_content.lines() {
        let line = raw.trim().trim_end_matches('\r');
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let mut parts = line.split_whitespace();
        let hash = parts.next().unwrap_or("");
        if !is_hex64(hash) {
            continue;
        }
        let rest: Vec<&str> = parts.collect();
        if rest.is_empty() {
            continue;
        }
        // GNU format: HASH  *filename  or  HASH  filename (filename may contain spaces rarely)
        let name_field = rest.join(" ");
        if name_field_matches_basename(&name_field, basename) {
            return Some(hash.to_lowercase());
        }
    }
    None
}

/// Verify `path` matches the line for `basename` in `sums_content`, or error.
pub fn verify_file_against_sums(path: &Path, basename: &str, sums_content: &str) -> Result<()> {
    let expected = parse_expected_hash(sums_content, basename).ok_or_else(|| {
        PortableError::msg(format!(
            "no SHA256 line for `{}` in kern-SHA256SUMS (required for install verification). \
             The release file must list hashes for kern-core.exe, kern-runtime.zip, and kern-portable.exe \
             (CI `kern-SHA256.partial-portable`). Try `--release vX.Y.Z` matching an asset-complete tag, or fix the release.",
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_star_basename() {
        let s = "523a826c883d68b6e63b17fec1e43813fcad8c2cc4a68054bc4976ba91b8abdc *kern-core.exe\n";
        let h = parse_expected_hash(s, "kern-core.exe").expect("hash");
        assert_eq!(h, "523a826c883d68b6e63b17fec1e43813fcad8c2cc4a68054bc4976ba91b8abdc");
    }

    #[test]
    fn parses_path_prefix() {
        let s = "523a826c883d68b6e63b17fec1e43813fcad8c2cc4a68054bc4976ba91b8abdc *portable-out/kern-core.exe\n";
        assert!(parse_expected_hash(s, "kern-core.exe").is_some());
    }

    #[test]
    fn parses_crlf() {
        let s = "523a826c883d68b6e63b17fec1e43813fcad8c2cc4a68054bc4976ba91b8abdc *kern-core.exe\r\n";
        assert!(parse_expected_hash(s, "kern-core.exe").is_some());
    }
}
