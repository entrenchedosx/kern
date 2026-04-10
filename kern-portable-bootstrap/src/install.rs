//! Create `kern-<version>/` from GitHub release assets (Windows).

use crate::artifact_cache::{
    self, ArtifactTriple, ASSET_KARGO_EXE, ASSET_KERN_CORE, ASSET_RUNTIME_ZIP,
};
use crate::download::download_to_file;
use crate::error::{path_ctx, PortableError, Result};
use crate::extract::extract_zip;
use crate::github::{self, ReleaseInfo};
use crate::sha256_verify;
use crate::tree_copy;
use serde::Serialize;
use std::fs;
use std::io::{BufRead, IsTerminal, Write};
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

const ASSET_KERN_SUMS: &str = "kern-SHA256SUMS";
/// Published beside merged `kern-SHA256SUMS` on entrenchedosx/kern; lists the portable trio only.
const ASSET_KERN_SUMS_PARTIAL_PORTABLE: &str = "kern-SHA256.partial-portable";

#[derive(Serialize)]
struct ConfigToml {
    kern_version: String,
}

/// `kern-NN` (two digits, 00–99) unique under `project_root`.
fn pick_env_directory_name(project_root: &Path) -> Result<String> {
    use rand::Rng;
    let mut rng = rand::thread_rng();
    for _ in 0..512 {
        let n = rng.gen_range(0..100);
        let name = format!("kern-{:02}", n);
        if !project_root.join(&name).exists() {
            return Ok(name);
        }
    }
    Err(PortableError::msg(
        "could not pick a free kern-NN directory (remove unused kern-* folders).",
    ))
}

fn kern_env_dirs(project_root: &Path) -> Result<Vec<PathBuf>> {
    let rd = fs::read_dir(project_root).map_err(|e| path_ctx(project_root, e))?;
    let mut hits: Vec<PathBuf> = Vec::new();
    for e in rd.flatten() {
        let p = e.path();
        if !p.is_dir() {
            continue;
        }
        let name = e.file_name();
        let name = name.to_string_lossy();
        if name.starts_with("kern-") && p.join("kern.exe").is_file() {
            hits.push(p);
        }
    }
    Ok(hits)
}

pub fn find_installed_env_root(project_root: &Path) -> Result<PathBuf> {
    let mut hits = kern_env_dirs(project_root)?;
    match hits.len() {
        0 => Err(PortableError::msg(
            "no kern-*/ environment here — run `kern-portable init` first.",
        )),
        1 => Ok(hits.pop().unwrap()),
        _ => Err(PortableError::msg(
            "multiple kern-*/ folders in this directory; remove extras or set KERN_HOME.",
        )),
    }
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
    Ok(())
}

fn resolve_assets(release: &ReleaseInfo) -> Result<(String, String, String, String)> {
    let tag = release.tag_name.clone();
    let kern = github::find_asset(release, ASSET_KERN_CORE)
        .ok_or_else(|| list_assets_error(release, ASSET_KERN_CORE))?
        .browser_download_url
        .clone();
    let rt = github::find_asset(release, ASSET_RUNTIME_ZIP)
        .ok_or_else(|| list_assets_error(release, ASSET_RUNTIME_ZIP))?
        .browser_download_url
        .clone();
    let kg = github::find_asset(release, ASSET_KARGO_EXE)
        .ok_or_else(|| list_assets_error(release, ASSET_KARGO_EXE))?
        .browser_download_url
        .clone();
    Ok((kern, rt, kg, tag))
}

fn resolve_kern_sums_url(release: &ReleaseInfo) -> Result<String> {
    let kern_sums = github::find_asset(release, ASSET_KERN_SUMS)
        .ok_or_else(|| {
            PortableError::msg(format!(
                "release {} missing `{}` (required for install verification)",
                release.tag_name, ASSET_KERN_SUMS
            ))
        })?
        .browser_download_url
        .clone();
    Ok(kern_sums)
}

fn list_assets_error(release: &ReleaseInfo, missing: &str) -> PortableError {
    let names: Vec<_> = release.assets.iter().map(|a| a.name.as_str()).collect();
    PortableError::msg(format!(
        "release {} missing required asset `{}`. Available: {:?}",
        release.tag_name, missing, names
    ))
}

/// Some releases ship a merged `kern-SHA256SUMS` without portable lines. Fix by, in order:
/// 1) `kern-SHA256.partial-portable` sidecar, 2) GitHub REST `digest` on `kern-core.exe` / `kern-runtime.zip`.
fn augment_kern_sums_with_portable_partial(
    release: &ReleaseInfo,
    kern_sums: &str,
    dl: &Path,
    token: Option<&str>,
) -> Result<String> {
    if sha256_verify::parse_expected_hash(kern_sums, ASSET_KERN_CORE).is_some()
        && sha256_verify::parse_expected_hash(kern_sums, ASSET_KARGO_EXE).is_some()
    {
        return Ok(kern_sums.to_string());
    }
    if let Some(asset) = github::find_asset(release, ASSET_KERN_SUMS_PARTIAL_PORTABLE) {
        let f_partial = dl.join(ASSET_KERN_SUMS_PARTIAL_PORTABLE);
        download_to_file(&asset.browser_download_url, &f_partial, token)?;
        let partial = fs::read_to_string(&f_partial).map_err(|e| path_ctx(&f_partial, e))?;
        let merged = format!("{}\n{}", partial.trim_end(), kern_sums.trim_end());
        if sha256_verify::parse_expected_hash(&merged, ASSET_KERN_CORE).is_some()
            && sha256_verify::parse_expected_hash(&merged, ASSET_KARGO_EXE).is_some()
        {
            return Ok(merged);
        }
    }
    let mut extra = String::new();
    for basename in [ASSET_KERN_CORE, ASSET_RUNTIME_ZIP, ASSET_KARGO_EXE] {
        let Some(a) = github::find_asset(release, basename) else {
            continue;
        };
        let Some(hex) = a.digest_sha256.as_deref() else {
            continue;
        };
        extra.push_str(&format!("{} *{}\n", hex, basename));
    }
    let merged = format!("{}\n{}", extra.trim_end(), kern_sums.trim_end());
    if sha256_verify::parse_expected_hash(&merged, ASSET_KERN_CORE).is_none()
        || sha256_verify::parse_expected_hash(&merged, ASSET_RUNTIME_ZIP).is_none()
        || sha256_verify::parse_expected_hash(&merged, ASSET_KARGO_EXE).is_none()
    {
        return Err(PortableError::msg(format!(
            "release {}: need SHA256 lines for `{}`, `{}`, and `{}` in `kern-SHA256SUMS`, or `{}`, or GitHub `digest` on those assets.",
            release.tag_name,
            ASSET_KERN_CORE,
            ASSET_RUNTIME_ZIP,
            ASSET_KARGO_EXE,
            ASSET_KERN_SUMS_PARTIAL_PORTABLE
        )));
    }
    Ok(merged)
}

/// Optional activation scripts: set `KERN_HOME` and prepend env root to `PATH`.
fn write_activation_scripts(env_root: &Path) -> Result<()> {
    let scripts = env_root.join("Scripts");
    fs::create_dir_all(&scripts).map_err(|e| path_ctx(&scripts, e))?;

    let activate_ps1 = scripts.join("Activate.ps1");
    fs::write(
        &activate_ps1,
        r#"# Sets KERN_HOME and prepends this environment to PATH.
# Usage:  . .\kern-42\Scripts\Activate.ps1   (path to your kern-NN folder from init)
if (-not $PSScriptRoot) {
    Write-Error 'Dot-source this file from its location under kern-*/Scripts/'
    exit 1
}
$KernEnvRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ($env:KERN_ACTIVE -eq '1' -and $env:_KERN_INACTIVE_PATH) {
    $env:PATH = $env:_KERN_INACTIVE_PATH
}
if (-not $env:_KERN_INACTIVE_PATH) {
    $env:_KERN_INACTIVE_PATH = $env:PATH
}
$env:KERN_HOME = $KernEnvRoot
$env:PATH = "$KernEnvRoot$([IO.Path]::PathSeparator)$env:PATH"
$env:KERN_ACTIVE = '1'

function global:kern-deactivate {
    if ($env:_KERN_INACTIVE_PATH) { $env:PATH = $env:_KERN_INACTIVE_PATH }
    Remove-Item Env:\_KERN_INACTIVE_PATH -ErrorAction SilentlyContinue
    Remove-Item Env:KERN_ACTIVE -ErrorAction SilentlyContinue
    Remove-Item Env:KERN_HOME -ErrorAction SilentlyContinue
    Remove-Item Function:kern-deactivate -Force -ErrorAction SilentlyContinue
}

Write-Host "KERN_HOME = $KernEnvRoot — kern.exe and kargo.exe on PATH. kern-deactivate to undo." -ForegroundColor Cyan
"#,
    )
    .map_err(|e| path_ctx(&activate_ps1, e))?;

    let activate_bat = scripts.join("activate.bat");
    fs::write(
        &activate_bat,
        "@echo off\r\n\
         pushd \"%~dp0..\"\r\n\
         set \"KERN_HOME=%CD%\"\r\n\
         popd\r\n\
         if not defined _KERN_INACTIVE_PATH set \"_KERN_INACTIVE_PATH=%PATH%\"\r\n\
         set \"PATH=%KERN_HOME%;%PATH%\"\r\n\
         set \"KERN_ACTIVE=1\"\r\n\
         echo KERN_HOME set. Run Scripts\\deactivate-kern.cmd to restore.\r\n",
    )
    .map_err(|e| path_ctx(&activate_bat, e))?;

    let deactivate_bat = scripts.join("deactivate-kern.cmd");
    fs::write(
        &deactivate_bat,
        "@echo off\r\n\
         if defined _KERN_INACTIVE_PATH set \"PATH=%_KERN_INACTIVE_PATH%\" & set \"_KERN_INACTIVE_PATH=\"\r\n\
         set \"KERN_ACTIVE=\"\r\n\
         set \"KERN_HOME=\"\r\n\
         echo PATH restored.\r\n",
    )
    .map_err(|e| path_ctx(&deactivate_bat, e))?;

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
    for sub in ["packages", "cache", "config"] {
        let d = env_root.join(sub);
        fs::create_dir_all(&d).map_err(|e| path_ctx(&d, e))?;
    }
    Ok(())
}

/// Fallback for tools when `KERN_HOME` is unset: `kern_env` reads `config/env.json` (`root` must be strict).
fn write_env_json(env_root: &Path) -> Result<()> {
    let abs = fs::canonicalize(env_root).unwrap_or_else(|_| env_root.to_path_buf());
    let obj = serde_json::json!({ "root": abs.to_string_lossy() });
    let p = env_root.join("config").join("env.json");
    let s = serde_json::to_string_pretty(&obj).map_err(|e| PortableError::msg(e.to_string()))?;
    fs::write(&p, s).map_err(|e| path_ctx(&p, e))?;
    Ok(())
}

fn read_kern_version_from_config(env_root: &Path) -> Option<String> {
    let p = env_root.join("config.toml");
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

fn check_env_sane_for_upgrade(env_root: &Path) -> Result<()> {
    let exe = env_root.join("kern.exe");
    if !exe.is_file() {
        return Err(PortableError::msg(
            "Environment appears corrupted (kern.exe missing at environment root). Run `kern-portable init --force`.",
        ));
    }
    let cfg = env_root.join("config.toml");
    fs::read_to_string(&cfg).map_err(|_| {
        PortableError::msg(
            "Environment appears corrupted (config.toml missing or unreadable). Run `kern-portable init --force` to reinstall.",
        )
    })?;
    Ok(())
}

fn apply_preserve_from_old_stage(old_env: &Path, stage: &Path) -> Result<()> {
    let old_pkg = old_env.join("packages");
    let new_pkg = stage.join("packages");
    if old_pkg.is_dir() {
        if new_pkg.exists() {
            fs::remove_dir_all(&new_pkg).map_err(|e| path_ctx(&new_pkg, e))?;
        }
        tree_copy::copy_dir_all(&old_pkg, &new_pkg)?;
    }
    let old_lock = old_env.join("lock.toml");
    if old_lock.is_file() {
        let dest = stage.join("lock.toml");
        fs::copy(&old_lock, &dest).map_err(|e| path_ctx(&dest, e))?;
    }
    Ok(())
}

fn merge_cache_artifacts_from_backup(backup: &Path, env_root: &Path) -> Result<()> {
    let src = backup.join("cache").join("artifacts");
    if !src.is_dir() {
        return Ok(());
    }
    let dst = env_root.join("cache").join("artifacts");
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

/// Move staged tree into `project_root/env_dir/`, backing up any existing folder.
fn promote_with_backup(
    project_root: &Path,
    stage: &Path,
    env_dir: &str,
    had_prior_env: bool,
) -> Result<()> {
    let dest_env = project_root.join(env_dir);
    let backup = project_root.join(format!("{}.backup", env_dir));

    if backup.exists() {
        fs::remove_dir_all(&backup).map_err(|e| path_ctx(&backup, e))?;
    }

    if dest_env.exists() {
        fs::rename(&dest_env, &backup).map_err(|e| {
            PortableError::msg(format!(
                "could not move {} to {}: {}",
                dest_env.display(),
                backup.display(),
                e
            ))
        })?;
    }

    let promote = (|| -> Result<()> {
        match fs::rename(stage, &dest_env) {
            Ok(()) => Ok(()),
            Err(_) => {
                tree_copy::copy_dir_all(stage, &dest_env)?;
                let _ = fs::remove_dir_all(stage);
                Ok(())
            }
        }
    })();

    if let Err(e) = promote {
        if had_prior_env {
            eprintln!("Install failed. Previous environment restored successfully.");
            eprintln!("No changes were applied.");
        }
        let _ = fs::remove_dir_all(&dest_env);
        if backup.exists() {
            let _ = fs::rename(&backup, &dest_env);
        }
        return Err(e);
    }

    if backup.exists() {
        merge_cache_artifacts_from_backup(&backup, &dest_env)?;
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
    preserve: bool,
    old_env: Option<&Path>,
) -> Result<()> {
    let runtime = stage.join("runtime");
    fs::create_dir_all(&runtime).map_err(|e| path_ctx(&runtime, e))?;
    fs::create_dir_all(stage.join("lib")).map_err(|e| path_ctx(&stage.join("lib"), e))?;
    fs::create_dir_all(stage.join("config")).map_err(|e| path_ctx(&stage.join("config"), e))?;

    fs::copy(f_kern, stage.join("kern.exe")).map_err(|e| path_ctx(&stage.join("kern.exe"), e))?;
    fs::copy(f_kargo, stage.join("kargo.exe")).map_err(|e| path_ctx(&stage.join("kargo.exe"), e))?;

    let rt_unpack = stage.join("_rt_unzip");
    if rt_unpack.exists() {
        fs::remove_dir_all(&rt_unpack).map_err(|e| path_ctx(&rt_unpack, e))?;
    }
    fs::create_dir_all(&rt_unpack).map_err(|e| path_ctx(&rt_unpack, e))?;
    extract_zip(f_rt, &rt_unpack)?;

    let kr = rt_unpack.join("kern-registry");
    if kr.is_dir() {
        let dest_kr = runtime.join("kern-registry");
        if dest_kr.exists() {
            fs::remove_dir_all(&dest_kr).map_err(|e| path_ctx(&dest_kr, e))?;
        }
        fs::rename(&kr, &dest_kr).map_err(|e| path_ctx(&dest_kr, e))?;
    }

    let lib_src = rt_unpack.join("lib");
    if lib_src.is_dir() {
        tree_copy::copy_dir_all(&lib_src, &stage.join("lib"))?;
    }
    let _ = fs::remove_dir_all(&rt_unpack);

    write_activation_scripts(stage)?;
    let ver_label = resolved_kern_version_for_config(release);
    write_config_and_lock(stage, &ver_label)?;
    ensure_empty_dirs(stage)?;
    write_env_json(stage)?;
    if preserve {
        if let Some(old) = old_env {
            apply_preserve_from_old_stage(old, stage)?;
        }
    }
    Ok(())
}

fn scrub_stale_kern_dirs(project_root: &Path, keep: &Path) -> Result<()> {
    let rd = fs::read_dir(project_root).map_err(|e| path_ctx(project_root, e))?;
    for e in rd.flatten() {
        let p = e.path();
        if !p.is_dir() {
            continue;
        }
        let name = e.file_name();
        let name = name.to_string_lossy();
        if name.starts_with("kern-") && p != keep {
            let _ = fs::remove_dir_all(&p);
        }
    }
    Ok(())
}

/// Reinstall / init: create or replace `kern-NN/` (optional preserve for interactive upgrade).
pub fn run_init(
    project_root: &Path,
    release_spec: &str,
    repo: &str,
    token: Option<&str>,
    force: bool,
) -> Result<()> {
    run_init_impl(project_root, release_spec, repo, token, force, false, false)
}

/// Replace `kern-*/` with a new release while keeping `packages/` and `lock.toml`.
pub fn run_upgrade(
    project_root: &Path,
    release_spec: &str,
    repo: &str,
    token: Option<&str>,
) -> Result<()> {
    let env_root = find_installed_env_root(project_root)?;
    check_env_sane_for_upgrade(&env_root)?;
    eprintln!("Checking for updates...");
    let cur = read_kern_version_from_config(&env_root);
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

    let release = github::fetch_release(repo, release_spec, token)?;
    require_checksum_assets(&release)?;

    let mut preserve = preserve;
    let old_env_for_preserve: Option<PathBuf> = if preserve {
        Some(find_installed_env_root(project_root)?)
    } else {
        None
    };

    let pre_scan = kern_env_dirs(project_root)?;
    if !preserve && pre_scan.len() > 1 {
        return Err(PortableError::msg(
            "multiple kern-*/ folders in this directory; remove extras or set KERN_HOME.",
        ));
    }

    let env_dir: String = if preserve {
        old_env_for_preserve
            .as_ref()
            .and_then(|p| p.file_name())
            .and_then(|n| n.to_str())
            .map(|s| s.to_string())
            .ok_or_else(|| PortableError::msg("upgrade: invalid existing environment path"))?
    } else if pre_scan.len() == 1 {
        pre_scan[0]
            .file_name()
            .and_then(|n| n.to_str())
            .map(|s| s.to_string())
            .ok_or_else(|| PortableError::msg("invalid kern-* folder name"))?
    } else {
        pick_env_directory_name(project_root)?
    };

    let dest_env = project_root.join(&env_dir);

    if dest_env.is_dir() && !force && !preserve {
        if std::env::var("CI")
            .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
            .unwrap_or(false)
            || !std::io::stdin().is_terminal()
        {
            return Err(PortableError::msg(format!(
                "{} already exists. Run with --force to reinstall, or remove it manually.",
                dest_env.display()
            )));
        }
        match prompt_existing_kern()? {
            1 => {}
            2 => preserve = true,
            _ => return Err(PortableError::msg("cancelled.")),
        }
    }

    let from_config = if preserve {
        old_env_for_preserve
            .as_ref()
            .and_then(|p| read_kern_version_from_config(p))
    } else {
        None
    };

    let had_prior_env = dest_env.is_dir();

    let (url_kern, url_rt, url_kargo, _tag_name) = resolve_assets(&release)?;
    let url_kern_sums = resolve_kern_sums_url(&release)?;

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
    let f_kargo = dl.join(ASSET_KARGO_EXE);
    let f_kern_sums = dl.join(ASSET_KERN_SUMS);

    let tag_slug = sanitize_tag_for_path(&release.tag_name);
    let cache_tag_dir = project_root
        .join(".kern-portable-cache")
        .join("artifacts")
        .join(&tag_slug);
    fs::create_dir_all(&cache_tag_dir).map_err(|e| path_ctx(&cache_tag_dir, e))?;

    if let Err(e) = download_to_file(&url_kern_sums, &f_kern_sums, token) {
        let _ = fs::remove_dir_all(&work);
        return Err(e);
    }

    let mut kern_sums = fs::read_to_string(&f_kern_sums).map_err(|e| path_ctx(&f_kern_sums, e))?;
    kern_sums = augment_kern_sums_with_portable_partial(&release, &kern_sums, &dl, token)?;
    fs::write(&f_kern_sums, &kern_sums).map_err(|e| path_ctx(&f_kern_sums, e))?;

    let kargo_sums_dummy = "";

    if artifact_cache::verify_tag_dir_consistent(
        &cache_tag_dir,
        &kern_sums,
        kargo_sums_dummy,
        ASSET_KARGO_EXE,
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
            ASSET_KARGO_EXE,
            &f_kargo,
            &cache_tag_dir.join(ASSET_KARGO_EXE),
            &kern_sums,
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
    fs::copy(&f_kern_sums, &c_kern_sums).map_err(|e| path_ctx(&c_kern_sums, e))?;

    let triple = ArtifactTriple {
        kern_core: &cache_tag_dir.join(ASSET_KERN_CORE),
        runtime_zip: &cache_tag_dir.join(ASSET_RUNTIME_ZIP),
        kargo_exe: &cache_tag_dir.join(ASSET_KARGO_EXE),
    };
    artifact_cache::write_integrity_manifest_v2_atomic(&cache_tag_dir, &release, triple)?;

    let stage = work.join("stage");
    if stage.exists() {
        fs::remove_dir_all(&stage).map_err(|e| path_ctx(&stage, e))?;
    }
    fs::create_dir_all(&stage).map_err(|e| path_ctx(&stage, e))?;

    let old_ref = old_env_for_preserve.as_deref();
    let build_result = build_stage_from_downloads(
        &stage,
        &f_kern,
        &f_rt,
        &f_kargo,
        &release,
        preserve,
        old_ref,
    );

    let _ = fs::remove_dir_all(&dl);

    if let Err(e) = build_result {
        let _ = fs::remove_dir_all(&work);
        return Err(e);
    }

    if let Err(e) = promote_with_backup(project_root, &stage, &env_dir, had_prior_env) {
        let _ = fs::remove_dir_all(&work);
        return Err(e);
    }

    let _ = fs::remove_dir_all(&work);

    let final_env = project_root.join(&env_dir);
    scrub_stale_kern_dirs(project_root, &final_env)?;

    eprintln!("Kern environment ready at {}", final_env.display());
    eprintln!(
        "  Set KERN_HOME or activate:\r\n\
              PowerShell:  . .\\{}\\Scripts\\Activate.ps1\r\n\
              cmd.exe:      {}\\Scripts\\activate.bat\r\n\
              Then:        kern --version    kargo.exe ...",
        env_dir, env_dir
    );
    Ok(())
}
