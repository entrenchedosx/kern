//! Fix broken active pointers, `bin/` launchers, and permissions.

use crate::env_paths;
use crate::error::{path_ctx, AppError, Result};
use crate::layout::{
    self, active_version_dir, kargo_bundle_dir, kern_bundle_dir, read_active_release_tag,
    version_home,
};
use crate::log::{self, LogFormat};
use crate::progress::Progress;
use crate::state::InstallState;
use std::fs;
use std::path::{Path, PathBuf};

pub struct RepairParams {
    pub prefix: PathBuf,
    pub fix_path: bool,
    pub verbose: bool,
    pub color: bool,
    pub log_format: LogFormat,
}

pub fn run_repair(p: RepairParams) -> Result<()> {
    let mut prog = Progress::new(p.color, p.verbose, 4);
    prog.step("Repair: analyzing prefix...");
    let _ = log::write_line(
        p.log_format,
        "INFO",
        "repair",
        "start",
        Some(serde_json::json!({ "prefix": p.prefix })),
    );

    let st = InstallState::load(&p.prefix)?;
    let active = active_version_dir(&p.prefix)?;
    if active.is_none() {
        prog.warn("No active version resolved (missing current symlink / active-release.txt).");
        if let Some(ref state) = st {
            let vdir = version_home(&p.prefix, &state.release_tag);
            if vdir.is_dir() {
                prog.info(&format!(
                    "Pointing active at state release {} ...",
                    state.release_tag
                ));
                layout::set_active_version(&p.prefix, &state.release_tag)?;
            }
        }
    }

    prog.step("Repair: refreshing bin/ launchers...");
    refresh_bin(&p.prefix, &mut prog)?;

    prog.step("Repair: permissions (Unix)...");
    #[cfg(unix)]
    fix_kern_permissions(&p.prefix)?;

    if p.fix_path {
        prog.step("Repair: PATH...");
        if let Some(st) = InstallState::load(&p.prefix)? {
            let bin = st.bin_dir();
            if cfg!(windows) {
                env_paths::ensure_windows_user_path_verified(&bin, &p.prefix)?;
            } else if let Some(cfg) = env_paths::pick_shell_config() {
                let _ = env_paths::ensure_unix_path_snippet(&cfg, &bin);
            }
        }
    }

    prog.ok("Repair pass complete. Run `kern-bootstrap doctor` to verify.");
    let _ = log::write_line(
        p.log_format,
        "INFO",
        "repair",
        "done",
        None,
    );
    Ok(())
}

pub fn refresh_bin(prefix: &Path, prog: &mut Progress) -> Result<()> {
    let Some(vdir) = active_version_dir(prefix)? else {
        return Err(AppError::msg(
            "cannot refresh bin/: no active version — install or `kern-bootstrap use <tag>` first",
        ));
    };
    let kern_root = kern_bundle_dir(&vdir);
    let kargo_root = kargo_bundle_dir(&vdir);
    if !kern_root.is_dir() || !kargo_root.is_dir() {
        return Err(AppError::msg(format!(
            "active version tree incomplete under {}",
            vdir.display()
        )));
    }

    let bin = prefix.join("bin");
    fs::create_dir_all(&bin).map_err(|e| path_ctx(&bin, e))?;

    #[cfg(unix)]
    {
        for n in ["kern", "kernc", "kern-scan", "kern_contract_humanize"] {
            let src = kern_root.join(n);
            if src.is_file() {
                let dst = bin.join(n);
                let _ = fs::remove_file(&dst);
                let rel = rel_path_from_bin_to(prefix, &src)?;
                std::os::unix::fs::symlink(&rel, &dst).map_err(|e| path_ctx(&dst, e))?;
            }
        }
        let shim = bin.join("kargo");
        let _ = fs::remove_file(&shim);
        let body = format!(
            "#!/usr/bin/env sh\nROOT=\"$(cd \"$(dirname \"$0\")/..\" && pwd)\"\nexec node \"$ROOT/current/kargo/cli/entry.js\" \"$@\"\n"
        );
        fs::write(&shim, body).map_err(|e| path_ctx(&shim, e))?;
        use std::os::unix::fs::PermissionsExt;
        let mut perms = fs::metadata(&shim)
            .map_err(|e| path_ctx(&shim, e))?
            .permissions();
        perms.set_mode(0o755);
        fs::set_permissions(&shim, perms).map_err(|e| path_ctx(&shim, e))?;
    }

    #[cfg(windows)]
    {
        write_windows_cmd(&bin, "kern", "kern.exe")?;
        write_windows_cmd(&bin, "kernc", "kernc.exe")?;
        write_windows_cmd(&bin, "kern-scan", "kern-scan.exe")?;
        write_windows_cmd(&bin, "kern_contract_humanize", "kern_contract_humanize.exe")?;
        let kcmd = bin.join("kargo.cmd");
        let body = windows_kargo_shim_body();
        fs::write(&kcmd, body).map_err(|e| path_ctx(&kcmd, e))?;
        let init = bin.join("kern-shell-init.cmd");
        let init_body = concat!(
            "@echo off\r\n",
            "REM Prepends this directory to PATH for the current cmd.exe session.\r\n",
            "REM Usage:  call \"%~f0\"\r\n",
            "set \"PATH=%~dp0;%PATH%\"\r\n",
        );
        fs::write(&init, init_body).map_err(|e| path_ctx(&init, e))?;
        let ps1 = bin.join("kern-shell-init.ps1");
        let ps1_body = concat!(
            "$here = Split-Path -Parent $MyInvocation.MyCommand.Path\r\n",
            "$env:Path = \"$here;$env:Path\"\r\n",
        );
        fs::write(&ps1, ps1_body).map_err(|e| path_ctx(&ps1, e))?;
    }

    prog.info(&format!("bin/ -> {}", vdir.display()));
    Ok(())
}

/// `prefix/bin/foo` -> `prefix/versions/.../kern/foo` as `../versions/.../kern/foo`.
#[cfg(unix)]
fn rel_path_from_bin_to(prefix: &Path, abs_target: &Path) -> Result<PathBuf> {
    let rel = abs_target.strip_prefix(prefix).map_err(|_| {
        AppError::msg(format!(
            "{} is not under install prefix {}",
            abs_target.display(),
            prefix.display()
        ))
    })?;
    Ok(Path::new("..").join(rel))
}

#[cfg(windows)]
fn write_windows_cmd(bin: &Path, stem: &str, exe: &str) -> Result<()> {
    let cmd = bin.join(format!("{}.cmd", stem));
    let body = windows_kern_shim_body(exe);
    fs::write(&cmd, body).map_err(|e| path_ctx(&cmd, e))?;
    Ok(())
}

/// Batch shims must use delayed expansion and a resolved `ROOT` path; bare `%%~dp0..` breaks `for /f`
/// and `%KERN_TAG%` is parsed before `set` without `EnableDelayedExpansion`, which flashes a window and exits.
#[cfg(windows)]
fn windows_kern_shim_body(kern_exe: &str) -> String {
    format!(
        "@echo off\r\n\
:: {}\r\n\
setlocal EnableExtensions EnableDelayedExpansion\r\n\
for %%I in (\"%~dp0..\") do set \"ROOT=%%~fI\"\r\n\
set \"TAGFILE=!ROOT!\\{arf}\"\r\n\
if not exist \"!TAGFILE!\" (\r\n\
  echo ERROR: missing \"!TAGFILE!\" ^(install prefix broken^).\r\n\
  echo Try: kern-bootstrap repair\r\n\
  pause\r\n\
  exit /b 1\r\n\
)\r\n\
set \"KERN_TAG=\"\r\n\
for /f \"usebackq tokens=* delims=\" %%a in (\"!TAGFILE!\") do set \"KERN_TAG=%%a\"\r\n\
if not defined KERN_TAG (\r\n\
  echo ERROR: could not read release tag from \"!TAGFILE!\".\r\n\
  pause\r\n\
  exit /b 1\r\n\
)\r\n\
set \"KERN_EXE=!ROOT!\\versions\\!KERN_TAG!\\kern\\{kern_exe}\"\r\n\
if not exist \"!KERN_EXE!\" (\r\n\
  echo ERROR: missing \"!KERN_EXE!\".\r\n\
  echo Tag: !KERN_TAG!  Try: kern-bootstrap doctor\r\n\
  pause\r\n\
  exit /b 1\r\n\
)\r\n\
\"!KERN_EXE!\" %*\r\n",
        layout::SHIM_TEMPLATE_MARKER,
        arf = layout::ACTIVE_RELEASE_FILE,
        kern_exe = kern_exe,
    )
}

#[cfg(windows)]
fn windows_kargo_shim_body() -> String {
    format!(
        "@echo off\r\n\
:: {}\r\n\
setlocal EnableExtensions EnableDelayedExpansion\r\n\
for %%I in (\"%~dp0..\") do set \"ROOT=%%~fI\"\r\n\
set \"TAGFILE=!ROOT!\\{arf}\"\r\n\
if not exist \"!TAGFILE!\" (\r\n\
  echo ERROR: missing \"!TAGFILE!\".\r\n\
  echo Try: kern-bootstrap repair\r\n\
  pause\r\n\
  exit /b 1\r\n\
)\r\n\
set \"KERN_TAG=\"\r\n\
for /f \"usebackq tokens=* delims=\" %%a in (\"!TAGFILE!\") do set \"KERN_TAG=%%a\"\r\n\
if not defined KERN_TAG (\r\n\
  echo ERROR: could not read release tag.\r\n\
  pause\r\n\
  exit /b 1\r\n\
)\r\n\
set \"KARGO_JS=!ROOT!\\versions\\!KERN_TAG!\\kargo\\cli\\entry.js\"\r\n\
if not exist \"!KARGO_JS!\" (\r\n\
  echo ERROR: missing \"!KARGO_JS!\".\r\n\
  pause\r\n\
  exit /b 1\r\n\
)\r\n\
if exist \"!ROOT!\\tools\\nodejs\\node.exe\" (\r\n\
  \"!ROOT!\\tools\\nodejs\\node.exe\" \"!KARGO_JS!\" %*\r\n\
  exit /b !ERRORLEVEL!\r\n\
)\r\n\
where node >nul 2>&1\r\n\
if errorlevel 1 (\r\n\
  echo ERROR: Node.js not found. Re-run: kern-bootstrap install   ^(bundles Node for Kargo^)\r\n\
  echo Or install Node 18+ from https://nodejs.org and reopen this terminal.\r\n\
  pause\r\n\
  exit /b 1\r\n\
)\r\n\
node \"!KARGO_JS!\" %*\r\n",
        layout::SHIM_TEMPLATE_MARKER,
        arf = layout::ACTIVE_RELEASE_FILE,
    )
}

#[cfg(unix)]
fn fix_kern_permissions(prefix: &Path) -> Result<()> {
    let Some(vdir) = active_version_dir(prefix)? else {
        return Ok(());
    };
    let kern_root = kern_bundle_dir(&vdir);
    if !kern_root.is_dir() {
        return Ok(());
    }
    use std::os::unix::fs::PermissionsExt;
    for e in fs::read_dir(&kern_root).map_err(|e| path_ctx(&kern_root, e))? {
        let e = e.map_err(|e| path_ctx(&kern_root, e))?;
        let p = e.path();
        if p.is_file() {
            let mut perms = fs::metadata(&p)
                .map_err(|e| path_ctx(&p, e))?
                .permissions();
            perms.set_mode(0o755);
            fs::set_permissions(&p, perms).map_err(|e| path_ctx(&p, e))?;
        }
    }
    Ok(())
}

/// Best-effort: ensure `current` and `bin/` match state / disk (for future repair flows).
#[allow(dead_code)]
pub fn ensure_consistent_with_state(prefix: &Path, prog: &mut Progress) -> Result<()> {
    let tag = match read_active_release_tag(prefix)? {
        Some(t) => t,
        None => {
            if let Some(st) = InstallState::load(prefix)? {
                st.release_tag.clone()
            } else {
                return Ok(());
            }
        }
    };
    let vdir = version_home(prefix, &tag);
    if !vdir.is_dir() {
        return Ok(());
    }
    let _ = layout::set_active_version(prefix, &tag);
    refresh_bin(prefix, prog)?;
    Ok(())
}
