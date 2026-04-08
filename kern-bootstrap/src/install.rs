use crate::detect::{self, dedupe_paths, default_user_prefix, ensure_prefix_writable};
use crate::download::{
    download_to_file, downloads_dir, sha256_file, verify_sha256sum_file, DownloadContext,
    DownloadOptions, MirrorTracker,
};
use crate::error::{path_ctx, AppError, Result};
use crate::extract::{
    extract_tar_gz, extract_zip, find_single_subdirectory, verify_kern_bundle_layout,
};
use crate::tree_copy::copy_dir_all;
use crate::github::{self, find_asset};
use crate::layout::{self, version_home, versions_dir};
use crate::lock::InstallLock;
use crate::log::{self, LogFormat};
pub use crate::platform::kern_executable_name;
use crate::progress::Progress;
use crate::repair;
use crate::state::InstallState;
use crate::versions_cmd;
use serde_json::Value;
use std::fs;
use std::io::BufRead;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ExistingAction {
    Upgrade,
    Reinstall,
    Uninstall,
    Exit,
    SwitchVersion,
    CleanOldVersions,
    FixPath,
}

pub struct InstallParams<'a> {
    pub repo: &'a str,
    pub release_spec: &'a str,
    pub prefix: PathBuf,
    pub token: Option<&'a str>,
    pub mirrors: &'a [String],
    pub modify_path: bool,
    pub verbose: bool,
    pub color: bool,
    pub existing: Option<ExistingAction>,
    pub non_interactive: bool,
    pub log_format: LogFormat,
    /// When true and another process holds the lock, prompt to wait (TTY).
    pub lock_wait_prompt: bool,
    /// Windows: spawn cmd/PowerShell with PATH fixed (ignored without an interactive TTY).
    pub activate_here: bool,
}

fn semver_from_tag(tag: &str) -> String {
    tag.strip_prefix('v').unwrap_or(tag).to_string()
}

#[derive(Clone, Copy)]
enum OsKind {
    Linux,
    Macos,
    Windows,
}

fn detect_os() -> Result<OsKind> {
    match std::env::consts::OS {
        "linux" => Ok(OsKind::Linux),
        "macos" => Ok(OsKind::Macos),
        "windows" => Ok(OsKind::Windows),
        _ => Err(AppError::UnsupportedPlatform),
    }
}

fn kern_asset_name(os: OsKind, semver: &str) -> String {
    match os {
        OsKind::Linux => format!("kern-linux-x64-v{}.tar.gz", semver),
        OsKind::Macos => format!("kern-macos-v{}.tar.gz", semver),
        OsKind::Windows => format!("kern-windows-x64-v{}.zip", semver),
    }
}

fn smoke_kern(kern_exe: &Path, prog: &Progress) -> Result<String> {
    let out = Command::new(kern_exe)
        .arg("--version")
        .output()
        .map_err(|e| AppError::msg(format!("kern smoke failed: {}", e)))?;
    if !out.status.success() {
        return Err(AppError::msg(format!(
            "kern --version failed: {}",
            String::from_utf8_lossy(&out.stderr)
        )));
    }
    let line = String::from_utf8_lossy(&out.stdout).trim().to_string();
    prog.info(&format!("smoke: {}", line));
    Ok(line)
}

/// Refuse headless Kern builds: `import("g2d")` requires `KERN_BUILD_GAME` + Raylib in the shipped `kern` binary.
fn smoke_kern_raylib_graphics(kern_exe: &Path, prog: &Progress) -> Result<()> {
    let out = Command::new(kern_exe)
        .arg("--version")
        .output()
        .map_err(|e| AppError::msg(format!("kern --version (graphics check): {}", e)))?;
    if !out.status.success() {
        return Err(AppError::msg(format!(
            "kern --version failed (graphics check): {}",
            String::from_utf8_lossy(&out.stderr)
        )));
    }
    let text = String::from_utf8_lossy(&out.stdout);
    if text.contains("graphics: none") {
        return Err(AppError::msg(
            "This Kern executable was built without Raylib (KERN_BUILD_GAME=OFF). g2d/g3d/game imports will fail.\n\
             kern-bootstrap expects official GitHub release zips built with graphics enabled (see repo release workflow).\n\
             Rebuild Kern with Raylib (e.g. vcpkg raylib + -DKERN_BUILD_GAME=ON) or install from a full release asset."
                .to_string(),
        ));
    }
    if text.contains("graphics: g2d+g3d+game (Raylib linked)") {
        prog.info("smoke: graphics bundle OK (g2d/g3d/game + Raylib).");
        return Ok(());
    }
    prog.warn(
        "Could not parse `graphics:` line from `kern --version` (older Kern?). \
         If g2d fails at runtime, use a release built with KERN_BUILD_GAME=ON or upgrade Kern.",
    );
    Ok(())
}

fn parse_kargo_package_version(kargo_root: &Path) -> Option<String> {
    let pj = kargo_root.join("package.json");
    let raw = fs::read_to_string(&pj).ok()?;
    let v: Value = serde_json::from_str(&raw).ok()?;
    v.get("version")?.as_str().map(|s| s.to_string())
}

fn clear_managed_payload(prefix: &Path) -> Result<()> {
    layout::remove_active_pointer(prefix)?;
    let vdir = versions_dir(prefix);
    if vdir.is_dir() {
        fs::remove_dir_all(&vdir).map_err(|e| path_ctx(&vdir, e))?;
    }
    let bdir = prefix.join("bin");
    if bdir.is_dir() {
        fs::remove_dir_all(&bdir).map_err(|e| path_ctx(&bdir, e))?;
    }
    Ok(())
}

pub fn run_install(p: InstallParams<'_>) -> Result<()> {
    let total = 7;
    let mut prog = Progress::new(p.color, p.verbose, total);
    let _ = log::write_line(
        p.log_format,
        "INFO",
        "install",
        "start",
        Some(serde_json::json!({
            "prefix": p.prefix,
            "release": p.release_spec,
            "repo": p.repo,
        })),
    );

    prog.step("Acquiring install lock...");
    let _lock = InstallLock::acquire(
        &p.prefix,
        &prog,
        p.lock_wait_prompt,
        p.non_interactive,
    )?;

    if layout::any_installing_sentinel(&p.prefix) {
        prog.warn(
            "Previous install was interrupted (`.installing` / `.installing.<pid>` sentinel found). Running repair, then continuing.",
        );
        match repair::run_repair(repair::RepairParams {
            prefix: p.prefix.clone(),
            fix_path: cfg!(windows),
            verbose: p.verbose,
            color: p.color,
            log_format: p.log_format,
        }) {
            Ok(()) => {
                layout::clear_all_installing_sentinels(&p.prefix);
            }
            Err(e) => {
                prog.warn(&format!(
                    "Repair after interrupted install could not finish cleanly: {} (sentinels left in place for a later retry)",
                    e
                ));
            }
        }
    }

    prog.step("Detecting environment...");
    let os = detect_os()?;
    ensure_prefix_writable(&p.prefix)?;
    fs::create_dir_all(versions_dir(&p.prefix)).map_err(|e| path_ctx(&p.prefix, e))?;
    fs::create_dir_all(downloads_dir(&p.prefix)).map_err(|e| path_ctx(&p.prefix, e))?;

    warn_legacy_kern_context(&p.prefix, &mut prog);

    if let Some(prev) = InstallState::load(&p.prefix)? {
        let action = resolve_existing_action(&p, &prev, &mut prog)?;
        match action {
            ExistingAction::Exit => {
                let _ = log::write_line(p.log_format, "INFO", "install", "cancelled", None);
                return Err(AppError::Cancelled);
            }
            ExistingAction::Uninstall => {
                crate::uninstall::run_uninstall(crate::uninstall::UninstallParams {
                    prefix: prev.prefix.clone(),
                    purge_home: false,
                    modify_path: p.modify_path,
                    non_interactive: p.non_interactive,
                    yes: p.non_interactive,
                    verbose: p.verbose,
                    color: p.color,
                    log_format: p.log_format,
                })?;
                return Ok(());
            }
            ExistingAction::SwitchVersion => {
                if let Some(tag) = versions_cmd::interactive_pick_version(&p.prefix)? {
                    versions_cmd::run_use(
                        versions_cmd::VersionsParams {
                            prefix: p.prefix.clone(),
                            verbose: p.verbose,
                            color: p.color,
                        },
                        &tag,
                    )?;
                }
                return Ok(());
            }
            ExistingAction::CleanOldVersions => {
                versions_cmd::run_clean_old(versions_cmd::VersionsParams {
                    prefix: p.prefix.clone(),
                    verbose: p.verbose,
                    color: p.color,
                })?;
                return Ok(());
            }
            ExistingAction::FixPath => {
                crate::repair::run_repair(crate::repair::RepairParams {
                    prefix: p.prefix.clone(),
                    fix_path: true,
                    verbose: p.verbose,
                    color: p.color,
                    log_format: p.log_format,
                })?;
                return Ok(());
            }
            ExistingAction::Reinstall => {
                let _ = log::write_line(
                    p.log_format,
                    "INFO",
                    "install",
                    "reinstall_clear",
                    None,
                );
                clear_managed_payload(&p.prefix)?;
                fs::create_dir_all(versions_dir(&p.prefix)).map_err(|e| path_ctx(&versions_dir(&p.prefix), e))?;
            }
            ExistingAction::Upgrade => {}
        }
    }

    prog.step("Resolving versions...");
    let release = github::fetch_release(p.repo, p.release_spec, p.token)?;
    let semver = semver_from_tag(&release.tag_name);
    prog.info(&format!("release {}", release.tag_name));

    let kern_name = kern_asset_name(os, &semver);
    let kargo_tar = format!("kargo-{}.tar.gz", release.tag_name);
    let kern_asset = find_asset(&release, &kern_name).ok_or_else(|| {
        AppError::AssetNotFound(format!(
            "{} (platform asset). Available: {:?}",
            kern_name,
            release.assets.iter().map(|a| &a.name).collect::<Vec<_>>()
        ))
    })?;
    let kargo_asset = find_asset(&release, &kargo_tar).ok_or_else(|| {
        AppError::AssetNotFound(format!(
            "kargo tarball {} — Kern and Kargo must come from the same GitHub release.",
            kargo_tar
        ))
    })?;
    let sums_asset = find_asset(&release, "kargo-SHA256SUMS").ok_or_else(|| {
        AppError::AssetNotFound(
            "kargo-SHA256SUMS (required to verify Kargo downloads)".into(),
        )
    })?;

    let dl = downloads_dir(&p.prefix);
    let kern_path = dl.join(&kern_name);
    let kargo_path = dl.join(&kargo_tar);
    let sums_path = dl.join("kargo-SHA256SUMS");

    let dopts = DownloadOptions {
        token: p.token,
        mirrors: p.mirrors,
    };
    let mut mirrors = MirrorTracker::default();
    let mut dctx = DownloadContext {
        opts: dopts,
        mirrors: &mut mirrors,
    };

    prog.step("Downloading Kargo checksums + bundle...");
    download_to_file(&sums_asset.browser_download_url, &sums_path, &prog, &mut dctx)?;
    download_to_file(&kargo_asset.browser_download_url, &kargo_path, &prog, &mut dctx)?;
    verify_sha256sum_file(&sums_path, &kargo_tar, &kargo_path)?;

    prog.step("Downloading Kern...");
    download_to_file(&kern_asset.browser_download_url, &kern_path, &prog, &mut dctx)?;

    if let Some(sums) = find_asset(&release, "kern-SHA256SUMS") {
        let ks = dl.join("kern-SHA256SUMS");
        download_to_file(&sums.browser_download_url, &ks, &prog, &mut dctx)?;
        verify_sha256sum_file(&ks, &kern_name, &kern_path)?;
        let _ = log::write_line(
            p.log_format,
            "INFO",
            "verify",
            "kern sha256 ok",
            Some(serde_json::json!({ "file": kern_name })),
        );
    } else if p.verbose {
        let h = sha256_file(&kern_path)?;
        prog.info(&format!("kern archive sha256 (no kern-SHA256SUMS on release): {}", h));
    }

    let staging_id = format!(
        ".staging-{}",
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map(|d| d.as_millis())
            .unwrap_or(0)
    );
    let staging = p.prefix.join(&staging_id);
    let _ = fs::remove_dir_all(&staging);

    let mut installing_marker: Option<PathBuf> = None;
    let result = {
        let mut run = || -> Result<()> {
            let marker = layout::create_installing_sentinel(&p.prefix)?;
            installing_marker = Some(marker);
            fs::create_dir_all(&staging).map_err(|e| path_ctx(&staging, e))?;
        let tag = release.tag_name.clone();
        let version_staging = staging.join(&tag);
        let kern_unpack = staging.join("kern_unpack");
        let kargo_unpack = staging.join("kargo_unpack");
        fs::create_dir_all(&kern_unpack).map_err(|e| path_ctx(&kern_unpack, e))?;
        fs::create_dir_all(&kargo_unpack).map_err(|e| path_ctx(&kargo_unpack, e))?;

        match os {
            OsKind::Linux | OsKind::Macos => extract_tar_gz(&kern_path, &kern_unpack)?,
            OsKind::Windows => extract_zip(&kern_path, &kern_unpack)?,
        }
        extract_tar_gz(&kargo_path, &kargo_unpack)?;

        let kern_inner = find_single_subdirectory(&kern_unpack)?;
        let kargo_inner = find_single_subdirectory(&kargo_unpack)?;
        fs::create_dir_all(&version_staging).map_err(|e| path_ctx(&version_staging, e))?;
        let kern_dest = version_staging.join(layout::KERN_SUBDIR);
        let kargo_dest = version_staging.join(layout::KARGO_SUBDIR);
        copy_dir_all(&kern_inner, &kern_dest)?;
        copy_dir_all(&kargo_inner, &kargo_dest)?;

        #[cfg(windows)]
        let embedded_node_version =
            crate::nodejs::ensure_windows_node_for_kargo(&p.prefix, &mut prog, &mut dctx)?;
        #[cfg(not(windows))]
        let embedded_node_version = None::<String>;

        verify_kern_bundle_layout(&kern_dest)?;
        let kern_exe = kern_dest.join(kern_executable_name());
        prog.step("Verifying Kern (smoke)...");
        let kern_ver_line = smoke_kern(&kern_exe, &prog)?;
        smoke_kern_raylib_graphics(&kern_exe, &prog)?;

        prog.step("Promoting version directory...");
        let final_v = version_home(&p.prefix, &tag);
        // Reinstall [6] removes `versions/` entirely; `rename` needs the parent directory to exist (Windows: ERROR_PATH_NOT_FOUND).
        fs::create_dir_all(versions_dir(&p.prefix)).map_err(|e| path_ctx(&versions_dir(&p.prefix), e))?;
        if final_v.exists() {
            fs::remove_dir_all(&final_v).map_err(|e| path_ctx(&final_v, e))?;
        }
        fs::rename(&version_staging, &final_v).map_err(|e| path_ctx(&final_v, e))?;

        let prev_active = layout::read_active_release_tag(&p.prefix)?;

        prog.step("Activating version (atomic pointer)...");
        layout::set_active_version(&p.prefix, &tag)?;

        let mut refresh_err = None;
        let mut prog2 = Progress::new(p.color, p.verbose, 1);
        if let Err(e) = repair::refresh_bin(&p.prefix, &mut prog2) {
            refresh_err = Some(e);
        }

        if let Some(e) = refresh_err {
            let _ = layout::rollback_active_version(&p.prefix, prev_active.as_deref());
            let _ = fs::remove_dir_all(&final_v);
            return Err(e);
        }

        let kargo_installed_root = final_v.join(layout::KARGO_SUBDIR);
        let k_pkg_ver = parse_kargo_package_version(&kargo_installed_root);

        #[cfg(windows)]
        let win_tc = match crate::win_toolchain::verify_toolchain_minimal_path(
            &p.prefix.join("bin"),
            &p.prefix,
            crate::win_toolchain::VerifyExpectations {
                kern_semver: &semver,
                kargo_package_version: k_pkg_ver.as_deref(),
            },
            &prog,
            false,
        ) {
            Ok(v) => {
                if matches!(p.log_format, LogFormat::Json) || p.verbose {
                    let _ = log::write_line(
                        p.log_format,
                        "INFO",
                        "verify",
                        "minimal_path_resolution",
                        Some(serde_json::json!({
                            "kern_where": v.kern_where.display().to_string(),
                            "kargo_where": v.kargo_where.display().to_string(),
                            "kern_line": v.kern_line,
                            "kargo_line": v.kargo_line,
                        })),
                    );
                }
                v
            }
            Err(e) => {
                let _ = layout::rollback_active_version(&p.prefix, prev_active.as_deref());
                let _ = fs::remove_dir_all(&final_v);
                return Err(e);
            }
        };

        let shell_config = if p.modify_path && !cfg!(windows) {
            crate::env_paths::pick_shell_config()
        } else {
            None
        };

        let mut state = InstallState {
            schema_version: 2,
            prefix: p.prefix.clone(),
            release_tag: tag.clone(),
            kern_semver: semver.clone(),
            kargo_tag: tag.clone(),
            kargo_package_version: k_pkg_ver,
            path_snippet_installed: false,
            shell_config_path: shell_config.clone(),
            embedded_node_version,
        };

        if p.modify_path {
            let dst_bin = p.prefix.join("bin");
            if cfg!(windows) {
                let log_paths = matches!(p.log_format, LogFormat::Json) || p.verbose;
                if log_paths {
                    let _ = log::write_line(
                        p.log_format,
                        "INFO",
                        "path",
                        "process PATH snapshot before HKCU update",
                        Some(serde_json::json!({
                            "path": std::env::var("PATH").unwrap_or_default(),
                        })),
                    );
                }
                let hkcu_changed =
                    crate::env_paths::ensure_windows_user_path_verified(&dst_bin, &p.prefix)?;
                if log_paths {
                    if let Ok(hkcu) = crate::env_paths::read_hkcu_path_raw() {
                        let _ = log::write_line(
                            p.log_format,
                            "INFO",
                            "path",
                            "HKCU Path after ensure+verify",
                            Some(serde_json::json!({
                                "registry_changed": hkcu_changed,
                                "hkcu_path": hkcu,
                            })),
                        );
                    }
                }
                state.path_snippet_installed = true;
                prog.ok("HKCU Path: managed bin is first; duplicate segments removed; directories with stray `kern.exe` outside the install prefix dropped from user PATH.");
            } else if let Some(ref cfg_path) = shell_config {
                if crate::env_paths::ensure_unix_path_snippet(cfg_path, &dst_bin)? {
                    state.path_snippet_installed = true;
                    prog.ok(&format!(
                        "Updated {} with PATH snippet. Run `source {}` or open a new shell.",
                        cfg_path.display(),
                        cfg_path.display()
                    ));
                }
            }
        }

        state.save()?;

        #[cfg(windows)]
        let result_paths = serde_json::json!({
            "kern": win_tc.kern_where.display().to_string(),
            "kargo": win_tc.kargo_where.display().to_string(),
        });
        #[cfg(not(windows))]
        let result_paths = serde_json::json!({
            "kern": p.prefix.join("bin").join(kern_executable_name()).display().to_string(),
            "kargo": p.prefix.join("bin").join("kargo").display().to_string(),
        });

        let _ = log::write_line(
            p.log_format,
            "INFO",
            "install",
            "complete",
            Some(serde_json::json!({
                "result": {
                    "event": "install_complete",
                    "status": "ok",
                    "verified": true,
                    "release_tag": tag,
                    "kern_semver": semver,
                    "paths": result_paths,
                },
                "release": tag,
                "kern_line": kern_ver_line,
            })),
        );

        prog.ok("✔ Toolchain is fully functional (independently verified).");

        #[cfg(windows)]
        {
            let bin = p.prefix.join("bin");
            crate::win_toolchain::print_install_verified_vs_shell(
                &win_tc,
                &bin,
                &prog,
                &semver,
            );
            prog.ok("Installed versions (from strict verify):");
            prog.ok(&format!("  Kern:   {}", win_tc.kern_line));
            prog.ok(&format!("  Kargo: {}", win_tc.kargo_line));
            crate::shell_hint::print_windows_session_path_hint(&bin, &prog);
            if p.activate_here {
                if p.non_interactive || !std::io::IsTerminal::is_terminal(&std::io::stdin()) {
                    prog.warn(
                        "--activate-here ignored (non-interactive mode or stdin is not a TTY)",
                    );
                } else {
                    match crate::shell_hint::spawn_activated_shell_here(&bin) {
                        Ok(_) => prog.ok(
                            "Opened a new shell with managed bin on PATH (you can close this window).",
                        ),
                        Err(e) => prog.warn(&format!(
                            "--activate-here: could not spawn shell: {}",
                            e
                        )),
                    }
                }
            }
        }
        prog.ok(&format!(
            "Kern + Kargo installed under {} (release {}).",
            p.prefix.display(),
            tag
        ));
        Ok(())
    };
    run()
};

    if result.is_ok() {
        if let Some(ref m) = installing_marker {
            layout::clear_installing_sentinel_file(m);
        }
    }

    let _ = fs::remove_dir_all(&staging);

    result
}

/// Old `kern` on PATH, broken `versions/<tag>`, or stale state — tell the user before they choose a menu action.
fn warn_legacy_kern_context(prefix: &Path, prog: &mut Progress) {
    let kerns = dedupe_paths(&detect::all_on_path("kern"));
    if kerns.len() > 1 {
        prog.warn(&format!(
            "Found {} different `kern` executables on PATH; only the first is used when you type `kern`.",
            kerns.len()
        ));
        for (i, path) in kerns.iter().enumerate() {
            let role = if i == 0 {
                "FIRST — wins (used)"
            } else {
                "shadowed — ignored unless PATH order changes"
            };
            let ver = detect::run_version_line(path)
                .filter(|s| !s.trim().is_empty())
                .unwrap_or_else(|| "(kern --version failed)".into());
            prog.warn(&format!("  [{}] {}", role, path.display()));
            prog.warn(&format!("           {}", ver.trim()));
        }
        prog.warn("Run `kern-bootstrap doctor` for a full report.");
    }
    if let Some(first) = kerns.first() {
        if !first.starts_with(prefix) {
            prog.warn(&format!(
                "The `kern` that runs first on your PATH is outside the managed prefix:\n         {}\n         Managed shims live under {}\\bin — put that directory first on PATH (or remove old Kern installs) so you use the bootstrapper's toolchain.",
                first.display(),
                prefix.display()
            ));
        }
        if detect::run_version_line(first).is_none() {
            prog.warn(&format!(
                "The first `kern` on PATH failed `kern --version` (may be corrupt, wrong OS build, or very old):\n         {}",
                first.display()
            ));
        }
    }

    if let Ok(Some(st)) = InstallState::load(prefix) {
        let vh = version_home(prefix, &st.release_tag);
        if !vh.is_dir() {
            prog.warn(&format!(
                "This prefix looks broken: state.json lists release {} but folder is missing:\n         {}\n         Use install menu [6] Reinstall, or `kern-bootstrap uninstall` then install again.",
                st.release_tag,
                vh.display()
            ));
        }
    }
}

fn resolve_existing_action(
    p: &InstallParams<'_>,
    prev: &InstallState,
    prog: &mut Progress,
) -> Result<ExistingAction> {
    if let Some(a) = p.existing {
        return Ok(a);
    }
    if p.non_interactive {
        return Ok(ExistingAction::Upgrade);
    }

    let kern_all = dedupe_paths(&detect::all_on_path("kern"));
    let installed = layout::list_installed_tags(&p.prefix).unwrap_or_default();
    let active = layout::read_active_release_tag(&p.prefix).ok().flatten();
    let state_vdir = version_home(&p.prefix, &prev.release_tag);

    let mut summary = String::new();
    if let Some(ref a) = active {
        summary.push_str(&format!("Active: {}\n", a));
        if !installed.contains(a) {
            summary.push_str("Issue: active tag is not under installed versions/ list (broken or partial).\n");
        }
    } else {
        summary.push_str("Active: (none / broken pointer)\n");
    }
    if !installed.is_empty() {
        summary.push_str(&format!("Installed: {}\n", installed.join(", ")));
    }
    if !state_vdir.is_dir() {
        summary.push_str(&format!(
            "Issue: BROKEN — state release {} has no directory:\n         {}\n",
            prev.release_tag,
            state_vdir.display()
        ));
    }
    if kern_all.is_empty() {
        summary.push_str("PATH: no `kern` found (add .kern\\bin after install).\n");
    } else {
        for (i, path) in kern_all.iter().enumerate() {
            let role = if i == 0 { "first (used)" } else { "shadowed" };
            let ver = detect::run_version_line(path)
                .filter(|s| !s.trim().is_empty())
                .map(|s| format!("  {}", s.trim()))
                .unwrap_or_else(|| "(kern --version failed)".to_string());
            summary.push_str(&format!(
                "PATH kern [{}]: {}\n         {}\n",
                role,
                path.display(),
                ver
            ));
        }
        if let Some(first) = kern_all.first() {
            if !first.starts_with(&p.prefix) {
                summary.push_str("Issue: first `kern` on PATH is outside managed prefix — old install may win.\n");
            }
        }
    }
    summary.push_str(&format!(
        "State file: release {} at {}\n",
        prev.release_tag,
        prev.prefix.display()
    ));

    prog.always(&summary);
    prog.always("Choose an action:");
    prog.always("  Tip: [6] Reinstall clears a broken/missing versions/ tree; [4] refreshes shims + PATH help.");
    prog.always("  [1] Upgrade / install this release");
    prog.always("  [2] Switch installed version");
    prog.always("  [3] Clean old versions (keep active)");
    prog.always("  [4] Fix PATH + repair shims (repair)");
    prog.always("  [5] Keep existing and exit");
    prog.always("  [6] Reinstall (clear versions + bin under prefix)");
    prog.always("  [7] Uninstall");
    prog.always("Enter choice [1-7] (default 1): ");
    let mut line = String::new();
    std::io::stdin()
        .lock()
        .read_line(&mut line)
        .map_err(|e| AppError::msg(e.to_string()))?;
    Ok(match line.trim() {
        "" | "1" => ExistingAction::Upgrade,
        "2" => ExistingAction::SwitchVersion,
        "3" => ExistingAction::CleanOldVersions,
        "4" => ExistingAction::FixPath,
        "5" => ExistingAction::Exit,
        "6" => ExistingAction::Reinstall,
        "7" => ExistingAction::Uninstall,
        _ => {
            prog.warn("Unrecognized choice; exiting.");
            ExistingAction::Exit
        }
    })
}

pub fn default_prefix(system: bool) -> PathBuf {
    if system {
        detect::default_system_prefix()
    } else {
        default_user_prefix()
    }
}
