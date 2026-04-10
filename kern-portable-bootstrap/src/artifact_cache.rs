//! Per-release artifact cache under `kern-*/cache/artifacts/<tag>/`.

use crate::error::{path_ctx, PortableError, Result};
use crate::github::ReleaseInfo;
use crate::sha256_verify;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::fs;
use std::path::Path;
use time::format_description::well_known::Rfc3339;
use time::OffsetDateTime;

pub const ASSET_KERN_CORE: &str = "kern-core.exe";
pub const ASSET_RUNTIME_ZIP: &str = "kern-runtime.zip";
pub const ASSET_KARGO_EXE: &str = "kargo.exe";

pub const SCHEMA_V2: u32 = 2;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IntegrityManifestV2 {
    pub schema: u32,
    pub tag: String,
    pub kern_core_sha256: String,
    pub runtime_sha256: String,
    pub kargo_sha256: String,
    pub created_at: String,
}

/// Paths to the three verified blobs (under the tag directory).
pub struct ArtifactTriple<'a> {
    pub kern_core: &'a Path,
    pub runtime_zip: &'a Path,
    pub kargo_exe: &'a Path,
}

pub fn utc_now_rfc3339() -> Result<String> {
    OffsetDateTime::now_utc()
        .format(&Rfc3339)
        .map_err(|e| PortableError::msg(format!("time format: {}", e)))
}

pub fn write_integrity_manifest_v2_atomic(
    tag_dir: &Path,
    release: &ReleaseInfo,
    triple: ArtifactTriple<'_>,
) -> Result<()> {
    let kern_core_sha256 = sha256_verify::hash_file(triple.kern_core)?;
    let runtime_sha256 = sha256_verify::hash_file(triple.runtime_zip)?;
    let kargo_sha256 = sha256_verify::hash_file(triple.kargo_exe)?;

    let manifest = IntegrityManifestV2 {
        schema: SCHEMA_V2,
        tag: release.tag_name.clone(),
        kern_core_sha256,
        runtime_sha256,
        kargo_sha256,
        created_at: utc_now_rfc3339()?,
    };

    let json =
        serde_json::to_string_pretty(&manifest).map_err(|e| PortableError::msg(e.to_string()))?;
    let final_path = tag_dir.join("integrity.json");
    let tmp_path = tag_dir.join("integrity.json.tmp");
    fs::write(&tmp_path, json).map_err(|e| path_ctx(&tmp_path, e))?;
    if final_path.exists() {
        fs::remove_file(&final_path).map_err(|e| path_ctx(&final_path, e))?;
    }
    fs::rename(&tmp_path, &final_path).map_err(|e| {
        let _ = fs::remove_file(&tmp_path);
        path_ctx(&final_path, e)
    })?;
    Ok(())
}

/// Returns `Ok(())` if the tag directory is absent, empty, or fully consistent.
pub fn verify_tag_dir_consistent(
    tag_dir: &Path,
    kern_sums: &str,
    _kargo_sums_unused: &str,
    kargo_basename: &str,
) -> Result<()> {
    if !tag_dir.is_dir() {
        return Ok(());
    }

    let manifest_path = tag_dir.join("integrity.json");
    if manifest_path.is_file() {
        let raw = fs::read_to_string(&manifest_path).map_err(|e| path_ctx(&manifest_path, e))?;
        let val: Value = serde_json::from_str(&raw).map_err(|e| PortableError::msg(e.to_string()))?;

        if val.get("kern_core_sha256").is_some() {
            let m: IntegrityManifestV2 = serde_json::from_value(val)
                .map_err(|e| PortableError::msg(format!("integrity.json v2: {}", e)))?;
            verify_hash_file(&tag_dir.join(ASSET_KERN_CORE), &m.kern_core_sha256)?;
            verify_hash_file(&tag_dir.join(ASSET_RUNTIME_ZIP), &m.runtime_sha256)?;
            verify_hash_file(&tag_dir.join(kargo_basename), &m.kargo_sha256)?; // field = native kargo.exe hash
            return Ok(());
        }

        if let Some(obj) = val.as_object() {
            if obj.is_empty() {
                return Err(PortableError::msg("integrity.json object is empty"));
            }
            if obj.len() == 1 && obj.contains_key("schema") {
                return Err(PortableError::msg("integrity.json is empty or invalid"));
            }
            for (name, hv) in obj {
                if name == "schema" {
                    continue;
                }
                let Some(expected) = hv.as_str() else {
                    continue;
                };
                verify_hash_file(&tag_dir.join(name), expected)?;
            }
            return Ok(());
        }
        return Err(PortableError::msg("integrity.json has unknown shape"));
    }

    verify_present_or_ok(tag_dir, ASSET_KERN_CORE, kern_sums)?;
    verify_present_or_ok(tag_dir, ASSET_RUNTIME_ZIP, kern_sums)?;
    verify_present_or_ok(tag_dir, kargo_basename, kern_sums)?;
    Ok(())
}

fn verify_present_or_ok(tag_dir: &Path, basename: &str, sums: &str) -> Result<()> {
    let p = tag_dir.join(basename);
    if !p.is_file() {
        return Ok(());
    }
    if sha256_verify::file_matches_sums(&p, basename, sums) {
        Ok(())
    } else {
        Err(PortableError::msg(format!(
            "cache file {} does not match release checksums",
            basename
        )))
    }
}

fn verify_hash_file(path: &Path, expected: &str) -> Result<()> {
    if !path.is_file() {
        return Err(PortableError::msg(format!(
            "missing cache file {}",
            path.display()
        )));
    }
    let got = sha256_verify::hash_file(path)?;
    if !got.eq_ignore_ascii_case(expected.trim()) {
        return Err(PortableError::msg(format!(
            "hash mismatch for {}",
            path.display()
        )));
    }
    Ok(())
}

/// Offline check: `integrity.json` vs files on disk (for `doctor`, no GitHub SUMS required).
pub fn verify_cached_artifacts_offline(tag_dir: &Path) -> Result<()> {
    let manifest_path = tag_dir.join("integrity.json");
    if !manifest_path.is_file() {
        return Ok(());
    }
    let raw = fs::read_to_string(&manifest_path).map_err(|e| path_ctx(&manifest_path, e))?;
    let val: Value = serde_json::from_str(&raw).map_err(|e| PortableError::msg(e.to_string()))?;
    if val.get("kern_core_sha256").is_some() {
        let m: IntegrityManifestV2 = serde_json::from_value(val)
            .map_err(|e| PortableError::msg(format!("integrity v2: {}", e)))?;
        verify_hash_file(&tag_dir.join(ASSET_KERN_CORE), &m.kern_core_sha256)?;
        verify_hash_file(&tag_dir.join(ASSET_RUNTIME_ZIP), &m.runtime_sha256)?;
        verify_hash_file(&tag_dir.join(ASSET_KARGO_EXE), &m.kargo_sha256)?;
        return Ok(());
    }
    if let Some(obj) = val.as_object() {
        for (name, hv) in obj {
            if name == "schema" {
                continue;
            }
            let Some(expected) = hv.as_str() else {
                continue;
            };
            verify_hash_file(&tag_dir.join(name), expected)?;
        }
        return Ok(());
    }
    Err(PortableError::msg("integrity.json has unknown shape"))
}

pub fn scrub_corrupt_tag_cache(tag_dir: &Path, tag_label: &str) -> Result<()> {
    if !tag_dir.exists() {
        return Ok(());
    }
    fs::remove_dir_all(tag_dir).map_err(|e| path_ctx(tag_dir, e))?;
    eprintln!(
        "Cache corrupted for {}. Rebuilding from release.",
        tag_label
    );
    Ok(())
}
