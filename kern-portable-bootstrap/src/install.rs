//! Create `.kern/` from GitHub release assets (Windows).

use crate::artifact_cache::{
    self, ArtifactTriple, ASSET_KERN_CORE, ASSET_RUNTIME_ZIP,
};
use crate::download::download_to_file;
use crate::error::{path_ctx, PortableError, Result};
use crate::extract::{extract_tar_gz, extract_zip, find_single_subdirectory};
use crate::github::{self, ReleaseInfo};
use crate::node_embed;
use crate::sha256_verify;
use crate::tree_copy;
use serde::Serialize;
use std::fs;
use std::io::{BufRead, IsTerminal, Write};
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

const ASSET_KERN_SUMS: &str = "kern-SHA256SUMS";
const ASSET_KARGO_SUMS: &str = "kargo-SHA256SUMS";

#[derive(Serialize)]
struct ConfigToml {
    kern_version: String,
}

fn kargo_tarball_name(tag: &str) -> String {
    format!("kargo-{}.tar.gz", tag)
}

/// Canonical tag for `config.toml` (reproducible; never `"latest"` / aliases).
fn resolved_kern_version_for_config(release: &ReleaseInfo) -> String {
    let t = release.tag_name.trim();
    if t.starts_with('v') {
        t.to_string()
    } else {
        format!("v{}", t)
    }
}

fn sanitize_tag_for_path(tag: &str) -> String {
    tag.chars()
        .map(|c| match c {
            '/' | '\\' | ':' | '*' | '?' | '"' | '<' | '>' | '|' => '-',
            _ => c,
        })
        .collect()
}

fn temp_install_dir() -> Result<PathBuf> {
    let base = std::env::temp_dir();
    let ms = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis();
    let p = base.join(format!("kern-install-{}-{}", std::process::id(), ms));
    Ok(p)
}

fn prompt_existing_kern() -> Result<u8> {
    let stdin = std::io::stdin();
    let mut stdout = std::io::stdout();
    writeln!(
        stdout,
        "Kern environment already exists.\nOptions:\n  1. Reinstall\n  2. Upgrade (keep packages/ and lock.toml)\n  3. Cancel"
    )
    .ok();
    stdout.flush().ok();
    let mut line = String::new();
    stdin
        .lock()
        .read_line(&mut line)
        .map_err(|e| PortableError::msg(format!("read choice: {}", e)))?;
    let c = line.trim().chars().next().unwrap_or('3');
    Ok(match c {
        '1' => 1,
        '2' => 2,
        _ => 3,
    })
}

/// Fail before any download if checksum sidecars are absent on the release.
fn require_checksum_assets(release: &ReleaseInfo) -> Result<()> {
    if github::find_asset(release, ASSET_KERN_SUMS).is_none() {
        return Err(list_assets_error(release, ASSET_KERN_SUMS));
    }
    if github::find_asset(release, ASSET_KARGO_SUMS).is_none() {
        return Err(list_assets_error(release, ASSET_KARGO_SUMS));
    }
    Ok(())
}

fn resolve_assets(release: &ReleaseInfo) -> Result<(String, String, String, String)> {
    let tag = release.tag_name.clone();
    let kargo_name = kargo_tarball_name(&tag);
    let kern = github::find_asset(release, ASSET_KERN_CORE)
        .ok_or_else(|| list_assets_error(release, ASSET_KERN_CORE))?
        .browser_download_url
        .clone();
    let rt = github::find_asset(release, ASSET_RUNTIME_ZIP)
        .ok_or_else(|| list_assets_error(release, ASSET_RUNTIME_ZIP))?
        .browser_download_url
        .clone();
    let kg = github::find_asset(release, &kargo_name)
        .ok_or_else(|| list_assets_error(release, &kargo_name))?
        .browser_download_url
        .clone();
    Ok((kern, rt, kg, tag))
}

fn resolve_sums_urls(release: &ReleaseInfo) -> Result<(String, String)> {
    let kern_sums = github::find_asset(release, ASSET_KERN_SUMS)
        .ok_or_else(|| {
            PortableError::msg(format!(
                "release {} missing `{}` (required for install verification)",
                release.tag_name, ASSET_KERN_SUMS
            ))
        })?
        .browser_download_url
        .clone();
    let kargo_sums = github::find_asset(release, ASSET_KARGO_SUMS)
        .ok_or_else(|| {
            PortableError::msg(format!(
                "release {} missing `{}` (required for Kargo bundle verification)",
                release.tag_name, ASSET_KARGO_SUMS
            ))
        })?
        .browser_download_url
        .clone();
    Ok((kern_sums, kargo_sums))
}

fn list_assets_error(release: &ReleaseInfo, missing: &str) -> PortableError {
    let names: Vec<_> = release.assets.iter().map(|a| a.name.as_str()).collect();
    PortableError::msg(format!(
        "release {} missing required asset `{}`. Available: {:?}",
        release.tag_name, missing, names
    ))
}

/// `%~dp0` is `bin\`; project root is one level up (`.kern/`).
fn write_kargo_cmd(bin: &Path) -> Result<()> {
    let mut content = String::new();
    content.push_str("@echo off\r\n");
    content.push_str("set \"KERN_ENV_ROOT=%~dp0..\"\r\n");
    content.push_str("set \"NODE_EXE=%KERN_ENV_ROOT%\\tools\\nodejs\\node.exe\"\r\n");
    content.push_str("set \"KARGO_JS=%KERN_ENV_ROOT%\\kargo\\cli\\entry.js\"\r\n");
    content.push_str("if exist \"%NODE_EXE%\" (\r\n");
    content.push_str("  \"%NODE_EXE%\" \"%KARGO_JS%\" %*\r\n");
    content.push_str(") else (\r\n");
    content.push_str("  node \"%KARGO_JS%\" %*\r\n");
    content.push_str(")\r\n");
    content.push_str("exit /b %ERRORLEVEL%\r\n");
    let p = bin.join("kargo.cmd");
    fs::write(&p, content).map_err(|e| path_ctx(&p, e))?;
    Ok(())
}

fn write_config_and_lock(env_root: &Path, kern_version: &str) -> Result<()> {
    let cfg = ConfigToml {
        kern_version: kern_version.to_string(),
    };
    let s = toml::to_string_pretty(&cfg).map_err(|e| PortableError::msg(e.to_string()))?;
    let p = env_root.join("config.toml");
    fs::write(&p, s).map_err(|e| path_ctx(&p, e))?;
    let lp = env_root.join("lock.toml");
    if !lp.is_file() {
        fs::write(&lp, "# kern portable lock (reserved)\n").map_err(|e| path_ctx(&lp, e))?;
    }
    Ok(())
}

fn ensure_empty_dirs(env_root: &Path) -> Result<()> {
    for sub in ["packages", "cache"] {
        let d = env_root.join(sub);
        fs::create_dir_all(&d).map_err(|e| path_ctx(&d, e))?;
    }
    Ok(())
}

fn read_kern_version_from_config(dot_kern: &Path) -> Option<String> {
    let p = dot_kern.join("config.toml");
    let raw = fs::read_to_string(&p).ok()?;
    for line in raw.lines() {
        let line = line.trim();
        if line.starts_with("kern_version") {
            return line
                .split_once('=')
                .map(|(_, r)| r.trim().trim_matches('"').to_string())
                .filter(|s| !s.is_empty());
        }
    }
    None
}

fn check_env_sane_for_upgrade(dot_kern: &Path) -> Result<()> {
    let exe = dot_kern.join("bin").join("kern.exe");
    if !exe.is_file() {
        return Err(PortableError::msg(
            "Environment appears corrupted (.kern/bin/kern.exe missing). Run `kern-portable init --force` to reinstall.",
        ));
    }
    let cfg = dot_kern.join("config.toml");
    fs::read_to_string(&cfg).map_err(|_| {
        PortableError::msg(
            "Environment appears corrupted (config.toml missing or unreadable). Run `kern-portable init --force` to reinstall.",
        )
    })?;
    Ok(())
}

fn apply_preserve_from_old_stage(project_root: &Path, stage: &Path) -> Result<()> {
    let old = project_root.join(".kern");
    let old_pkg = old.join("packages");
    let new_pkg = stage.join("packages");
    if old_pkg.is_dir() {
        if new_pkg.exists() {
            fs::remove_dir_all(&new_pkg).map_err(|e| path_ctx(&new_pkg, e))?;
        }
        tree_copy::copy_dir_all(&old_pkg, &new_pkg)?;
    }
    let old_lock = old.join("lock.toml");
    if old_lock.is_file() {
        let dest = stage.join("lock.toml");
        fs::copy(&old_lock, &dest).map_err(|e| path_ctx(&dest, e))?;
    }
    Ok(())
}

fn merge_cache_artifacts_from_backup(backup: &Path, dot_kern: &Path) -> Result<()> {
    let src = backup.join("cache").join("artifacts");
    if !src.is_dir() {
        return Ok(());
    }
    let dst = dot_kern.join("cache").join("artifacts");
    fs::create_dir_all(&dst).map_err(|e| path_ctx(&dst, e))?;
    tree_copy::copy_dir_all(&src, &dst)?;
    Ok(())
}

fn fetch_binary_with_cache(
    url: &str,
    basename: &str,
    work_path: &Path,
    cache_path: &Path,
    sums: &str,
    token: Option<&str>,
    tag_dir: &Path,
    tag_label: &str,
) -> Result<()> {
    if cache_path.is_file() {
        if sha256_verify::file_matches_sums(cache_path, basename, sums) {
            fs::copy(cache_path, work_path).map_err(|e| path_ctx(work_path, e))?;
            return Ok(());
        }
        artifact_cache::scrub_corrupt_tag_cache(tag_dir, tag_label)?;
        fs::create_dir_all(tag_dir).map_err(|e| path_ctx(tag_dir, e))?;
    }
    download_to_file(url, work_path, token)?;
    sha256_verify::verify_file_against_sums(work_path, basename, sums)?;
    if let Some(parent) = cache_path.parent() {
        fs::create_dir_all(parent).map_err(|e| path_ctx(parent, e))?;
    }
    fs::copy(work_path, cache_path).map_err(|e| path_ctx(cache_path, e))?;
    Ok(())
}

/// Move staged `.kern` tree into place: existing `.kern` → `.kern.backup`, then
/// `stage` → `.kern`. On success remove backup (after merging artifact cache).
///
/// Staging may live on a different volume than the project (`%TEMP%` vs project disk);
/// if `rename` fails, we fall back to a recursive copy then delete `stage`.
fn promote_with_backup(project_root: &Path, stage: &Path, had_prior_dot_kern: bool) -> Result<()> {
    let dot_kern = project_root.join(".kern");
    let backup = project_root.join(".kern.backup");

    if backup.exists() {
        fs::remove_dir_all(&backup).map_err(|e| path_ctx(&backup, e))?;
    }

    if dot_kern.exists() {
        fs::rename(&dot_kern, &backup).map_err(|e| {
            PortableError::msg(format!(
                "could not move {} to {}: {}",
                dot_kern.display(),
                backup.display(),
                e
            ))
        })?;
    }

    let promote = (|| -> Result<()> {
        match fs::rename(stage, &dot_kern) {
            Ok(()) => Ok(()),
            Err(_) => {
                tree_copy::copy_dir_all(stage, &dot_kern)?;
                let _ = fs::remove_dir_all(stage);
                Ok(())
            }
        }
    })();

    if let Err(e) = promote {
        if had_prior_dot_kern {
            eprintln!("Install failed. Previous environment restored successfully.");
            eprintln!("No changes were applied.");
        }
        let _ = fs::remove_dir_all(&dot_kern);
        if backup.exists() {
            let _ = fs::rename(&backup, &dot_kern);
        }
        return Err(e);
    }

    if backup.exists() {
        merge_cache_artifacts_from_backup(&backup, &dot_kern)?;
        fs::remove_dir_all(&backup).map_err(|e| path_ctx(&backup, e))?;
    }
    Ok(())
}

fn build_stage_from_downloads(
    stage: &Path,
    f_kern: &Path,
    f_rt: &Path,
    f_kargo: &Path,
    release: &ReleaseInfo,
    token: Option<&str>,
    preserve: bool,
    project_root: &Path,
) -> Result<()> {
    let bin = stage.join("bin");
    let runtime = stage.join("runtime");
    let kargo_root = stage.join("kargo");
    fs::create_dir_all(&bin).map_err(|e| path_ctx(&bin, e))?;
    fs::create_dir_all(&runtime).map_err(|e| path_ctx(&runtime, e))?;

    fs::copy(f_kern, bin.join("kern.exe")).map_err(|e| path_ctx(&bin.join("kern.exe"), e))?;
    extract_zip(f_rt, &runtime)?;
    let k_unpack = stage.join("_kargo_unpack");
    fs::create_dir_all(&k_unpack).map_err(|e| path_ctx(&k_unpack, e))?;
    extract_tar_gz(f_kargo, &k_unpack)?;
    let inner = find_single_subdirectory(&k_unpack)?;
    if kargo_root.exists() {
        fs::remove_dir_all(&kargo_root).map_err(|e| path_ctx(&kargo_root, e))?;
    }
    fs::rename(&inner, &kargo_root).map_err(|e| path_ctx(&kargo_root, e))?;
    let _ = fs::remove_dir_all(&k_unpack);

    node_embed::ensure_windows_node(stage, token)?;
    write_kargo_cmd(&bin)?;
    let ver_label = resolved_kern_version_for_config(release);
    write_config_and_lock(stage, &ver_label)?;
    ensure_empty_dirs(stage)?;
    if preserve {
        apply_preserve_from_old_stage(project_root, stage)?;
    }
    Ok(())
}

/// Reinstall / init: create or replace `.kern/` (optional preserve for interactive upgrade).
pub fn run_init(
    project_root: &Path,
    release_spec: &str,
    repo: &str,
    token: Option<&str>,
    force: bool,
) -> Result<()> {
    run_init_impl(project_root, release_spec, repo, token, force, false, false)
}

/// Replace `.kern/` with a new release while keeping `packages/` and `lock.toml`.
pub fn run_upgrade(
    project_root: &Path,
    release_spec: &str,
    repo: &str,
    token: Option<&str>,
) -> Result<()> {
    let dot_kern = project_root.join(".kern");
    if !dot_kern.is_dir() {
        return Err(PortableError::msg(
            "no .kern/ here — run `kern-portable init` first, then `kern-portable upgrade`.",
        ));
    }
    check_env_sane_for_upgrade(&dot_kern)?;
    eprintln!("Checking for updates...");
    let cur = read_kern_version_from_config(&dot_kern);
    eprintln!(
        "Current: {}",
        cur.as_deref().unwrap_or("(unknown)")
    );
    run_init_impl(project_root, release_spec, repo, token, true, true, true)
}

fn run_init_impl(
    project_root: &Path,
    release_spec: &str,
    repo: &str,
    token: Option<&str>,
    force: bool,
    preserve: bool,
    upgrade_cli: bool,
) -> Result<()> {
    if !cfg!(windows) {
        return Err(PortableError::msg(
            "kern-portable init is only supported on Windows.",
        ));
    }

    let dot_kern = project_root.join(".kern");
    let mut preserve = preserve;

    if dot_kern.is_dir() && !force && !preserve {
        if std::env::var("CI")
            .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
            .unwrap_or(false)
            || !std::io::stdin().is_terminal()
        {
            return Err(PortableError::msg(
                ".kern/ already exists. Run with --force to reinstall, or remove .kern/ manually.",
            ));
        }
        match prompt_existing_kern()? {
            1 => {}
            2 => preserve = true,
            _ => return Err(PortableError::msg("cancelled.")),
        }
    }

    let from_config = if preserve && dot_kern.is_dir() {
        read_kern_version_from_config(&dot_kern)
    } else {
        None
    };

    let had_prior_dot_kern = dot_kern.is_dir();

    let release = github::fetch_release(repo, release_spec, token)?;
    require_checksum_assets(&release)?;
    let (url_kern, url_rt, url_kargo, tag_name) = resolve_assets(&release)?;
    let (url_kern_sums, url_kargo_sums) = resolve_sums_urls(&release)?;

    if upgrade_cli {
        eprintln!("Latest: {}", release.tag_name);
    }

    if preserve {
        eprintln!("Upgrading Kern environment:");
        if let Some(ref from) = from_config {
            eprintln!("  from {} → {}", from, release.tag_name);
        } else {
            eprintln!("  → {}", release.tag_name);
        }
        eprintln!("Preserving:");
        eprintln!("  - packages/");
        eprintln!("  - lock.toml (when present)");
    }

    let work = temp_install_dir()?;
    let dl = work.join("dl");
    fs::create_dir_all(&dl).map_err(|e| path_ctx(&dl, e))?;

    let f_kern = dl.join(ASSET_KERN_CORE);
    let f_rt = dl.join(ASSET_RUNTIME_ZIP);
    let kargo_file = kargo_tarball_name(&tag_name);
    let f_kargo = dl.join(&kargo_file);
    let f_kern_sums = dl.join(ASSET_KERN_SUMS);
    let f_kargo_sums = dl.join(ASSET_KARGO_SUMS);

    let tag_slug = sanitize_tag_for_path(&release.tag_name);
    let cache_tag_dir = project_root
        .join(".kern")
        .join("cache")
        .join("artifacts")
        .join(&tag_slug);
    fs::create_dir_all(&cache_tag_dir).map_err(|e| path_ctx(&cache_tag_dir, e))?;

    let dl_result = (|| -> Result<()> {
        download_to_file(&url_kern_sums, &f_kern_sums, token)?;
        download_to_file(&url_kargo_sums, &f_kargo_sums, token)?;
        Ok(())
    })();

    if let Err(e) = dl_result {
        let _ = fs::remove_dir_all(&work);
        return Err(e);
    }

    let kern_sums = fs::read_to_string(&f_kern_sums).map_err(|e| path_ctx(&f_kern_sums, e))?;
    let kargo_sums = fs::read_to_string(&f_kargo_sums).map_err(|e| path_ctx(&f_kargo_sums, e))?;

    if artifact_cache::verify_tag_dir_consistent(
        &cache_tag_dir,
        &kern_sums,
        &kargo_sums,
        kargo_file.as_str(),
    )
    .is_err()
    {
        artifact_cache::scrub_corrupt_tag_cache(&cache_tag_dir, &release.tag_name)?;
        fs::create_dir_all(&cache_tag_dir).map_err(|e| path_ctx(&cache_tag_dir, e))?;
    }

    let fetch_bins = (|| -> Result<()> {
        let tag_label = release.tag_name.as_str();
        fetch_binary_with_cache(
            &url_kern,
            ASSET_KERN_CORE,
            &f_kern,
            &cache_tag_dir.join(ASSET_KERN_CORE),
            &kern_sums,
            token,
            &cache_tag_dir,
            tag_label,
        )?;
        fetch_binary_with_cache(
            &url_rt,
            ASSET_RUNTIME_ZIP,
            &f_rt,
            &cache_tag_dir.join(ASSET_RUNTIME_ZIP),
            &kern_sums,
            token,
            &cache_tag_dir,
            tag_label,
        )?;
        fetch_binary_with_cache(
            &url_kargo,
            kargo_file.as_str(),
            &f_kargo,
            &cache_tag_dir.join(&kargo_file),
            &kargo_sums,
            token,
            &cache_tag_dir,
            tag_label,
        )?;
        Ok(())
    })();

    if let Err(e) = fetch_bins {
        let _ = fs::remove_dir_all(&work);
        return Err(e);
    }

    let c_kern_sums = cache_tag_dir.join(ASSET_KERN_SUMS);
    let c_kargo_sums = cache_tag_dir.join(ASSET_KARGO_SUMS);
    fs::copy(&f_kern_sums, &c_kern_sums).map_err(|e| path_ctx(&c_kern_sums, e))?;
    fs::copy(&f_kargo_sums, &c_kargo_sums).map_err(|e| path_ctx(&c_kargo_sums, e))?;

    let triple = ArtifactTriple {
        kern_core: &cache_tag_dir.join(ASSET_KERN_CORE),
        runtime_zip: &cache_tag_dir.join(ASSET_RUNTIME_ZIP),
        kargo_tgz: &cache_tag_dir.join(&kargo_file),
    };
    artifact_cache::write_integrity_manifest_v2_atomic(&cache_tag_dir, &release, triple)?;

    let stage = work.join("stage");
    if stage.exists() {
        fs::remove_dir_all(&stage).map_err(|e| path_ctx(&stage, e))?;
    }
    fs::create_dir_all(&stage).map_err(|e| path_ctx(&stage, e))?;

    let build_result = build_stage_from_downloads(
        &stage,
        &f_kern,
        &f_rt,
        &f_kargo,
        &release,
        token,
        preserve,
        project_root,
    );

    let _ = fs::remove_dir_all(&dl);

    if let Err(e) = build_result {
        let _ = fs::remove_dir_all(&work);
        return Err(e);
    }

    if let Err(e) = promote_with_backup(project_root, &stage, had_prior_dot_kern) {
        let _ = fs::remove_dir_all(&work);
        return Err(e);
    }

    let _ = fs::remove_dir_all(&work);

    eprintln!("Kern environment ready at {}", dot_kern.display());
    Ok(())
}
