//! Strict Windows toolchain verification: isolated PATH + `where` normalization + shim sanity.

use crate::detect;
use crate::error::{path_ctx, AppError, Result};
use crate::layout;
use crate::platform::kern_executable_name;
use crate::progress::Progress;
use std::ffi::OsString;
use std::fs;
use std::io::Read;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::thread;
use std::time::{Duration, Instant};

use std::os::windows::process::CommandExt;

/// Hide console windows from nested `cmd.exe` (install/doctor diagnostics).
const CREATE_NO_WINDOW: u32 = 0x0800_0000;

/// Avoid hangs if a broken `kern` or shell hook blocks forever.
const CMD_INNER_TIMEOUT: Duration = Duration::from_secs(3);

pub struct ToolchainVersions {
    pub kern_line: String,
    pub kargo_line: String,
    pub kern_where: PathBuf,
    pub kargo_where: PathBuf,
}

pub struct VerifyExpectations<'a> {
    pub kern_semver: &'a str,
    pub kargo_package_version: Option<&'a str>,
}

fn comspec() -> PathBuf {
    std::env::var_os("ComSpec")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(r"C:\Windows\System32\cmd.exe"))
}

/// PATH for subprocess verify: managed `bin`, Node (bundled and/or same as installer process), then Windows dirs.
///
/// When `ensure_windows_node_for_kargo` skips downloading because `node` is on the **parent** PATH, strict verify
/// must still expose that directory — otherwise `kargo.cmd` only sees `where node` under a too-tight PATH and fails.
fn minimal_verify_path(bin_dir: &Path, install_prefix: &Path) -> OsString {
    let windir = std::env::var_os("WINDIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(r"C:\Windows"));
    let sysroot = std::env::var_os("SystemRoot")
        .map(PathBuf::from)
        .unwrap_or_else(|| windir.clone());

    let mut parts: Vec<PathBuf> = vec![bin_dir.to_path_buf()];

    let bundled_node = install_prefix.join("tools").join("nodejs");
    if bundled_node.join("node.exe").is_file() {
        parts.push(bundled_node);
    }
    if let Some(path_os) = std::env::var_os("PATH") {
        if let Some(node_exe) = detect::first_on_path_in(&path_os, "node") {
            if let Some(dir) = node_exe.parent() {
                parts.push(dir.to_path_buf());
            }
        }
    }

    parts.push(windir.join("System32"));
    parts.push(windir.clone());
    if sysroot.join("System32") != windir.join("System32") {
        parts.push(sysroot.join("System32"));
    }
    let mut seen2 = std::collections::HashSet::<String>::new();
    let mut uniq: Vec<PathBuf> = Vec::new();
    for seg in parts {
        let key = seg.display().to_string().replace('/', "\\").to_lowercase();
        if seen2.insert(key) {
            uniq.push(seg);
        }
    }
    let mut o = OsString::new();
    for (i, seg) in uniq.iter().enumerate() {
        if i > 0 {
            o.push(";");
        }
        o.push(seg.as_os_str());
    }
    o
}

/// Run `cmd.exe /C <inner>` with optional PATH override, no console window, bounded wait.
fn run_cmd_inner(path_env: Option<&OsString>, inner: &str, timeout: Duration) -> Result<std::process::Output> {
    let mut cmd = Command::new(comspec());
    cmd.arg("/C").arg(inner);
    if let Some(p) = path_env {
        cmd.env("PATH", p);
    }
    cmd.stdin(Stdio::null());
    cmd.stdout(Stdio::piped());
    cmd.stderr(Stdio::piped());
    cmd.creation_flags(CREATE_NO_WINDOW);

    let mut child = cmd
        .spawn()
        .map_err(|e| AppError::msg(format!("subprocess spawn ({}): {}", inner, e)))?;

    let deadline = Instant::now() + timeout;
    let status = loop {
        if Instant::now() > deadline {
            let _ = child.kill();
            let _ = child.wait();
            return Err(AppError::msg(format!(
                "subprocess timed out after {:?}: {}",
                timeout, inner
            )));
        }
        match child.try_wait() {
            Ok(Some(st)) => break st,
            Ok(None) => thread::sleep(Duration::from_millis(25)),
            Err(e) => {
                let _ = child.kill();
                return Err(AppError::msg(format!("subprocess wait: {}", e)));
            }
        }
    };

    let mut stdout = Vec::new();
    let mut stderr = Vec::new();
    if let Some(mut o) = child.stdout.take() {
        let _ = o.read_to_end(&mut stdout);
    }
    if let Some(mut e) = child.stderr.take() {
        let _ = e.read_to_end(&mut stderr);
    }

    Ok(std::process::Output {
        status,
        stdout,
        stderr,
    })
}

fn run_cmd_captured(path_env: &OsString, inner: &str) -> Result<std::process::Output> {
    run_cmd_inner(Some(path_env), inner, CMD_INNER_TIMEOUT)
}

fn all_where_paths(stdout: &[u8], tool: &str) -> Result<Vec<PathBuf>> {
    let s = String::from_utf8_lossy(stdout);
    let paths: Vec<PathBuf> = s
        .lines()
        .map(|l| l.trim())
        .filter(|l| !l.is_empty())
        .map(PathBuf::from)
        .collect();
    if paths.is_empty() {
        return Err(AppError::msg(format!(
            "`where {}` produced no paths (stdout empty)",
            tool
        )));
    }
    Ok(paths)
}

/// `where` first; if it fails or returns nothing, scan `path_env` segment-by-segment (locked-down shells).
fn where_tool_paths(path_env: &OsString, tool: &str) -> Result<Vec<PathBuf>> {
    let where_hint = match run_cmd_captured(path_env, &format!("where {}", tool)) {
        Ok(w) => {
            if w.status.success() {
                if let Ok(paths) = all_where_paths(&w.stdout, tool) {
                    if !paths.is_empty() {
                        return Ok(paths);
                    }
                }
            }
            format!(
                "where exit={:?} stdout={} stderr={}",
                w.status.code(),
                String::from_utf8_lossy(&w.stdout).trim(),
                String::from_utf8_lossy(&w.stderr).trim()
            )
        }
        Err(e) => format!("where spawn/run error: {}", e),
    };
    if let Some(p) = detect::first_on_path_in(path_env, tool) {
        return Ok(vec![p]);
    }
    let path_human = path_env.to_string_lossy();
    let path_snip = if path_human.len() > 4000 {
        format!(
            "{}… (truncated, total {} chars)",
            &path_human[..4000],
            path_human.len()
        )
    } else {
        path_human.into_owned()
    };
    Err(AppError::msg(format!(
        "Failed to resolve `{}` from PATH.\nPATH used:\n{}\n\n{}",
        tool, path_snip, where_hint
    )))
}

fn path_compare_normalized(p: &Path) -> String {
    p.display()
        .to_string()
        .replace('/', "\\")
        .trim_end_matches('\\')
        .to_lowercase()
}

fn assert_path_under_managed_bin(resolved: &Path, bin_canon: &Path) -> Result<()> {
    if let (Ok(c), Ok(bc)) = (resolved.canonicalize(), bin_canon.canonicalize()) {
        if c.starts_with(&bc) {
            return Ok(());
        }
    }
    let r = path_compare_normalized(resolved);
    let b = path_compare_normalized(bin_canon);
    if r == b || r.starts_with(&(b.clone() + "\\")) {
        return Ok(());
    }
    Err(AppError::msg(format!(
        "resolved binary is not under managed bin:\n  {}\n  (expected under {})",
        resolved.display(),
        bin_canon.display()
    )))
}

fn assert_kern_output_contains_semver(line: &str, semver: &str) -> Result<()> {
    if !line.contains(semver) {
        return Err(AppError::msg(format!(
            "`kern --version` output does not contain expected semver {}:\n{}",
            semver, line
        )));
    }
    Ok(())
}

fn assert_kargo_output_matches(line: &str, expected: Option<&str>) -> Result<()> {
    if let Some(exp) = expected {
        let e = exp.trim();
        if !e.is_empty() && !line.contains(e) {
            return Err(AppError::msg(format!(
                "`kargo --version` output does not contain expected package version {}:\n{}",
                e, line
            )));
        }
    }
    Ok(())
}

/// Ensure `.cmd` shims reference the layout and **on-disk** targets exist.
/// When `require_state_matches_active`, `bootstrap-state.json` must agree with `active-release.txt` (skip during install before state is saved).
pub fn validate_windows_shim_files(
    bin_dir: &Path,
    install_prefix: &Path,
    require_state_matches_active: bool,
) -> Result<()> {
    let kern_p = bin_dir.join("kern.cmd");
    let kargo_p = bin_dir.join("kargo.cmd");
    let kern_raw = fs::read_to_string(&kern_p).map_err(|e| path_ctx(&kern_p, e))?;
    let kargo_raw = fs::read_to_string(&kargo_p).map_err(|e| path_ctx(&kargo_p, e))?;

    let has_ver = |s: &str| s.contains("versions\\") || s.contains("versions/");
    let kern_ok = kern_raw.contains(layout::SHIM_TEMPLATE_MARKER)
        || (has_ver(&kern_raw) && kern_raw.contains(layout::ACTIVE_RELEASE_FILE));
    if !kern_ok {
        return Err(AppError::msg(format!(
            "{} does not look like a valid Kern shim (expected `{}` or legacy active-release + versions tree). Run: kern-bootstrap repair",
            kern_p.display(),
            layout::SHIM_TEMPLATE_MARKER
        )));
    }
    let kargo_ok = kargo_raw.contains(layout::SHIM_TEMPLATE_MARKER)
        || (has_ver(&kargo_raw) && kargo_raw.contains("entry.js"));
    if !kargo_ok {
        return Err(AppError::msg(format!(
            "{} does not look like a valid Kargo shim (expected `{}` or legacy kargo layout). Run: kern-bootstrap repair",
            kargo_p.display(),
            layout::SHIM_TEMPLATE_MARKER
        )));
    }

    let tag = layout::read_active_release_file(install_prefix)?.ok_or_else(|| {
        AppError::msg(
            "active-release.txt missing — cannot validate shim targets".to_string(),
        )
    })?;
    if require_state_matches_active {
        if let Ok(Some(st)) = crate::state::InstallState::load(install_prefix) {
            if st.release_tag != tag {
                return Err(AppError::msg(format!(
                    "active-release.txt ({}) disagrees with bootstrap-state.json release_tag ({}); shims may point at the wrong versions/ tree. Run: kern-bootstrap repair",
                    tag, st.release_tag
                )));
            }
        }
    }
    let vhome = layout::version_home(install_prefix, &tag);
    let kern_impl = vhome.join("kern").join(kern_executable_name());
    if !kern_impl.is_file() {
        return Err(AppError::msg(format!(
            "shim target missing (versions tree damaged?): {}\nRun: kern-bootstrap repair",
            kern_impl.display()
        )));
    }
    let kargo_js = vhome.join("kargo").join("cli").join("entry.js");
    if !kargo_js.is_file() {
        return Err(AppError::msg(format!(
            "Kargo entry missing: {}\nRun: kern-bootstrap repair",
            kargo_js.display()
        )));
    }
    Ok(())
}

/// After install: contrast verified resolution vs this process’s inherited PATH.
pub fn print_install_verified_vs_shell(
    tv: &ToolchainVersions,
    bin_dir: &Path,
    prog: &Progress,
    expected_kern_semver: &str,
) {
    prog.always("");
    prog.always("━━ A — Verified toolchain (authoritative, independent of this shell)");
    prog.always(&format!("    kern  -> {}", tv.kern_where.display()));
    prog.always(&format!("    kargo -> {}", tv.kargo_where.display()));
    prog.always("");
    prog.always("━━ B — This shell right now (`where kern`)");

    let bin_canon = bin_dir.canonicalize().unwrap_or_else(|_| bin_dir.to_path_buf());

    let path_os = std::env::var_os("PATH").unwrap_or_default();
    match where_tool_paths(&path_os, "kern") {
        Ok(raw) => {
            let norm = detect::normalize_where_by_directory_prefer_cmd(raw);
            if let Some(first) = norm.first() {
                prog.always(&format!("    {}", first.display()));
                let under = assert_path_under_managed_bin(first, &bin_canon).is_ok();
                if under {
                    prog.ok("    This window already resolves `kern` from managed bin.");
                } else {
                    prog.err("    Current shell is using the path above — not managed bin.");
                    prog.err("    Reason: stale PATH (this window started before install/refresh).");
                    prog.always(&format!(
                        "    Fix (ONE command): call \"{}\"",
                        bin_dir.join("kern-shell-init.cmd").display()
                    ));
                }
            } else {
                prog.warn("    could not resolve `kern` on PATH (empty after normalization)");
            }
        }
        Err(e) => {
            prog.warn(&format!(
                "    could not resolve `kern` via `where` or PATH scan: {}",
                e
            ));
            prog.always(&format!(
                "    Fix (ONE command): call \"{}\"",
                bin_dir.join("kern-shell-init.cmd").display()
            ));
        }
    }

    prog.always("");
    prog.always("    What actually runs when you type `kern` here (`cmd /c kern --version`):");
    match run_cmd_inner(None, "kern --version", CMD_INNER_TIMEOUT) {
        Ok(o) if o.status.success() => {
            let line = String::from_utf8_lossy(&o.stdout).trim().to_string();
            if line.is_empty() {
                prog.warn("    (empty output)");
            } else {
                prog.always(&format!("    {}", line));
                if !line.contains(expected_kern_semver) {
                    prog.warn("    Semver does not match verified install — a different `kern` may be executing (cwd / PATHEXT / PATH order).");
                }
            }
        }
        Ok(o) => {
            prog.warn(&format!(
                "    kern --version failed in this shell (exit {}).",
                o.status.code().unwrap_or(-1)
            ));
        }
        Err(e) => {
            prog.warn(&format!("    could not run kern --version: {}", e));
        }
    }
}

/// Verify shims with hardened minimal PATH; check versions against installed state.
pub fn verify_toolchain_minimal_path(
    bin_dir: &Path,
    install_prefix: &Path,
    exp: VerifyExpectations<'_>,
    prog: &Progress,
    enforce_state_matches_active: bool,
) -> Result<ToolchainVersions> {
    let kern_cmd = bin_dir.join("kern.cmd");
    let kargo_cmd = bin_dir.join("kargo.cmd");
    if !kern_cmd.is_file() {
        return Err(AppError::msg(format!(
            "missing kern launcher: {}",
            kern_cmd.display()
        )));
    }
    if !kargo_cmd.is_file() {
        return Err(AppError::msg(format!(
            "missing kargo launcher: {}",
            kargo_cmd.display()
        )));
    }

    validate_windows_shim_files(bin_dir, install_prefix, enforce_state_matches_active)?;

    let bin_canon = bin_dir.canonicalize().map_err(|e| {
        AppError::msg(format!(
            "cannot canonicalize managed bin {}: {}",
            bin_dir.display(),
            e
        ))
    })?;

    let path_env = minimal_verify_path(bin_dir, install_prefix);

    let kern_raw = where_tool_paths(&path_env, "kern")?;
    let kern_paths = detect::normalize_where_by_directory_prefer_cmd(kern_raw);
    for p in &kern_paths {
        assert_path_under_managed_bin(p, &bin_canon)?;
    }
    let kern_where = kern_paths[0].clone();

    let kargo_raw = where_tool_paths(&path_env, "kargo")?;
    let kargo_paths = detect::normalize_where_by_directory_prefer_cmd(kargo_raw);
    for p in &kargo_paths {
        assert_path_under_managed_bin(p, &bin_canon)?;
    }
    let kargo_where = kargo_paths[0].clone();

    let kern_ver = run_cmd_captured(&path_env, "kern --version")?;
    if !kern_ver.status.success() {
        return Err(AppError::msg(format!(
            "`kern --version` failed under minimal PATH.\nstdout:\n{}\nstderr:\n{}",
            String::from_utf8_lossy(&kern_ver.stdout),
            String::from_utf8_lossy(&kern_ver.stderr)
        )));
    }
    let kern_line = String::from_utf8_lossy(&kern_ver.stdout).trim().to_string();
    if kern_line.is_empty() {
        return Err(AppError::msg(
            "`kern --version` returned empty stdout".to_string(),
        ));
    }
    assert_kern_output_contains_semver(&kern_line, exp.kern_semver)?;

    let kargo_ver = run_cmd_captured(&path_env, "kargo --version")?;
    if !kargo_ver.status.success() {
        return Err(AppError::msg(format!(
            "`kargo --version` failed under minimal PATH.\nstdout:\n{}\nstderr:\n{}",
            String::from_utf8_lossy(&kargo_ver.stdout),
            String::from_utf8_lossy(&kargo_ver.stderr)
        )));
    }
    let kargo_line = String::from_utf8_lossy(&kargo_ver.stdout).trim().to_string();
    if kargo_line.is_empty() {
        return Err(AppError::msg(
            "`kargo --version` returned empty stdout".to_string(),
        ));
    }
    assert_kargo_output_matches(&kargo_line, exp.kargo_package_version)?;

    prog.ok("Strict verify (minimal PATH: managed bin + Windows system dirs):");
    prog.info(&format!("    where kern  -> {}", kern_where.display()));
    prog.info(&format!("    where kargo -> {}", kargo_where.display()));
    prog.info(&format!("    kern --version -> {}", kern_line));
    prog.info(&format!("    kargo --version -> {}", kargo_line));

    Ok(ToolchainVersions {
        kern_line,
        kargo_line,
        kern_where,
        kargo_where,
    })
}
