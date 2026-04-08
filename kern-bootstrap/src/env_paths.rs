use crate::error::{path_ctx, Result};
#[cfg(windows)]
use crate::error::AppError;
use std::io::Read;
use std::path::{Path, PathBuf};

pub const MARK_BEGIN: &str = "# >>> KERN_BOOTSTRAP_BEGIN >>>";
pub const MARK_END: &str = "# <<< KERN_BOOTSTRAP_END <<<";

pub fn snippet_unix(bin_dir: &Path) -> String {
    format!(
        "{}\nexport PATH=\"{}:$PATH\"\n{}\n",
        MARK_BEGIN,
        bin_dir.display(),
        MARK_END
    )
}

pub fn shell_config_candidates() -> Vec<PathBuf> {
    let home = dirs::home_dir().unwrap_or_else(|| PathBuf::from("."));
    let mut v = Vec::new();
    if let Ok(s) = std::env::var("SHELL") {
        if s.contains("zsh") {
            v.push(home.join(".zshrc"));
        }
        if s.contains("bash") {
            v.push(home.join(".bashrc"));
            v.push(home.join(".bash_profile"));
        }
    }
    v.push(home.join(".profile"));
    v.push(home.join(".zshrc"));
    v.push(home.join(".bashrc"));
    v.dedup();
    v
}

pub fn pick_shell_config() -> Option<PathBuf> {
    for p in shell_config_candidates() {
        if p.is_file() {
            return Some(p);
        }
    }
    shell_config_candidates().first().cloned()
}

pub fn has_marker_block(text: &str) -> bool {
    text.contains(MARK_BEGIN) && text.contains(MARK_END)
}

pub fn remove_marker_block(text: &str) -> String {
    let mut out = String::new();
    let mut skip = false;
    for line in text.lines() {
        if line.trim() == MARK_BEGIN {
            skip = true;
            continue;
        }
        if line.trim() == MARK_END {
            skip = false;
            continue;
        }
        if !skip {
            out.push_str(line);
            out.push('\n');
        }
    }
    out
}

pub fn ensure_unix_path_snippet(config_path: &Path, bin_dir: &Path) -> Result<bool> {
    let block = snippet_unix(bin_dir);
    let mut raw = String::new();
    if config_path.is_file() {
        std::fs::File::open(config_path)
            .and_then(|mut f| f.read_to_string(&mut raw))
            .map_err(|e| path_ctx(&config_path.to_path_buf(), e))?;
    }
    let stripped = remove_marker_block(&raw);
    let mut new_content = stripped.trim_end().to_string();
    if !new_content.is_empty() {
        new_content.push('\n');
    }
    new_content.push_str(&block);
    if new_content == raw {
        return Ok(false);
    }
    if let Some(parent) = config_path.parent() {
        std::fs::create_dir_all(parent).map_err(|e| path_ctx(&parent.to_path_buf(), e))?;
    }
    std::fs::write(config_path, new_content).map_err(|e| path_ctx(&config_path.to_path_buf(), e))?;
    Ok(true)
}

#[cfg(windows)]
fn normalize_win_path_entry(s: &str) -> String {
    s.trim()
        .trim_end_matches('\\')
        .replace('/', "\\")
        .to_lowercase()
}

/// Stable key for dedupe: canonicalize when possible, else normalized display.
#[cfg(windows)]
fn segment_dedupe_key(seg: &str) -> String {
    let t = seg.trim();
    if t.is_empty() {
        return String::new();
    }
    Path::new(t)
        .canonicalize()
        .map(|p| normalize_win_path_entry(&p.display().to_string()))
        .unwrap_or_else(|_| normalize_win_path_entry(t))
}

/// Compare paths when `canonicalize` fails (missing dir, permissions, UNC quirks).
#[cfg(windows)]
fn path_prefix_equal_normalized(seg: &str, base: &Path) -> bool {
    let sn = normalize_win_path_entry(seg);
    let bn = normalize_win_path_entry(&base.display().to_string());
    sn == bn || sn.starts_with(&(bn.clone() + "\\"))
}

/// HKCU-only: directory on PATH contains `kern.exe`, is not managed `bin`, and lies **outside** install prefix.
#[cfg(windows)]
fn path_segment_is_external_kern_exe_dir(seg: &str, bin_dir: &Path, install_prefix: &Path) -> bool {
    let p = Path::new(seg.trim());
    if !p.join("kern.exe").is_file() {
        return false;
    }
    if path_segment_is_managed_bin(seg, bin_dir) {
        return false;
    }
    match (p.canonicalize(), install_prefix.canonicalize()) {
        (Ok(dir_canon), Ok(pre_canon)) => !dir_canon.starts_with(&pre_canon),
        _ => !path_prefix_equal_normalized(seg.trim(), install_prefix),
    }
}

/// True if `dir` (or its canonical form) appears in HKCU `Environment` `Path`.
#[cfg(windows)]
pub fn windows_registry_path_contains_bin_dir(dir: &Path) -> bool {
    use winreg::enums::*;
    use winreg::RegKey;

    let hcu = match RegKey::predef(HKEY_CURRENT_USER).open_subkey("Environment") {
        Ok(k) => k,
        Err(_) => return false,
    };
    let path: String = hcu.get_value("Path").unwrap_or_default();

    let mut targets: Vec<String> = Vec::new();
    targets.push(normalize_win_path_entry(&dir.display().to_string()));
    if let Ok(c) = dir.canonicalize() {
        targets.push(normalize_win_path_entry(&c.display().to_string()));
    }

    for part in path.split(';') {
        let n = normalize_win_path_entry(part);
        if n.is_empty() {
            continue;
        }
        if targets.iter().any(|t| t == &n) {
            return true;
        }
        if let Ok(p) = Path::new(part.trim()).canonicalize() {
            let cn = normalize_win_path_entry(&p.display().to_string());
            if targets.iter().any(|t| t == &cn) {
                return true;
            }
        }
    }
    false
}

#[cfg(not(windows))]
pub fn windows_registry_path_contains_bin_dir(_dir: &Path) -> bool {
    false
}

/// True if this PATH segment is the managed `bin_dir` (display, normalized, or same canonical path).
#[cfg(windows)]
fn path_segment_is_managed_bin(seg: &str, bin_dir: &Path) -> bool {
    let needle_n = normalize_win_path_entry(&bin_dir.display().to_string());
    let sn = normalize_win_path_entry(seg);
    if sn.is_empty() {
        return false;
    }
    if sn == needle_n {
        return true;
    }
    if let Ok(bin_canon) = bin_dir.canonicalize() {
        let want = normalize_win_path_entry(&bin_canon.display().to_string());
        if sn == want {
            return true;
        }
        if let Ok(p) = Path::new(seg.trim()).canonicalize() {
            if normalize_win_path_entry(&p.display().to_string()) == want {
                return true;
            }
        }
    }
    path_prefix_equal_normalized(seg.trim(), bin_dir)
}

/// `.../build/Release` or `.../build/Debug` containing `kern.exe` — typical CMake dev output that shadows managed `bin\`.
#[cfg(windows)]
fn path_segment_is_likely_kern_cmake_build(seg: &str) -> bool {
    let p = Path::new(seg.trim());
    if !p.is_dir() || !p.join("kern.exe").is_file() {
        return false;
    }
    let lower = normalize_win_path_entry(seg);
    lower.contains("\\build\\release") || lower.contains("\\build\\debug")
}

/// Drop duplicate PATH segments (canonical / normalized key), keeping the first occurrence.
#[cfg(windows)]
fn dedupe_win_path_segments(segments: &[&str]) -> Vec<String> {
    use std::collections::HashSet;
    let mut seen = HashSet::<String>::new();
    let mut out = Vec::new();
    for s in segments {
        let key = segment_dedupe_key(s);
        if key.is_empty() {
            continue;
        }
        if seen.insert(key) {
            out.push(s.trim().to_string());
        }
    }
    out
}

/// Tell Explorer and other apps that `Environment` changed (user must still restart existing `cmd.exe`).
#[cfg(windows)]
pub fn notify_windows_environment_changed() {
    use std::ffi::OsStr;
    use std::os::windows::ffi::OsStrExt;
    const HWND_BROADCAST: isize = 0xffff;
    const WM_SETTINGCHANGE: u32 = 0x001a;
    const SMTO_ABORTIFHUNG: u32 = 0x0002;
    #[link(name = "user32")]
    extern "system" {
        fn SendMessageTimeoutW(
            h_wnd: isize,
            msg: u32,
            wparam: usize,
            lparam: *const u16,
            fu_flags: u32,
            u_timeout: u32,
            lpdw_result: *mut usize,
        ) -> isize;
    }
    let wide: Vec<u16> = OsStr::new("Environment")
        .encode_wide()
        .chain(std::iter::once(0))
        .collect();
    let mut dw_result = 0usize;
    unsafe {
        SendMessageTimeoutW(
            HWND_BROADCAST,
            WM_SETTINGCHANGE,
            0,
            wide.as_ptr(),
            SMTO_ABORTIFHUNG,
            5000,
            &mut dw_result,
        );
    }
}

/// After writing HKCU `Path`, re-read and assert the first segment is managed `bin` (normalized / canonical).
#[cfg(windows)]
pub fn verify_hkcu_path_invariant(bin_dir: &Path) -> Result<()> {
    use winreg::enums::*;
    use winreg::RegKey;

    let hcu = RegKey::predef(HKEY_CURRENT_USER);
    let env = hcu
        .open_subkey("Environment")
        .map_err(|e| AppError::msg(e.to_string()))?;
    let path: String = env.get_value("Path").unwrap_or_default();
    let first = path
        .split(';')
        .map(|s| s.trim())
        .find(|s| !s.is_empty())
        .ok_or_else(|| AppError::msg("HKCU Path has no segments after rewrite".to_string()))?;
    if !path_segment_is_managed_bin(first, bin_dir) {
        let expected_norm = normalize_win_path_entry(&bin_dir.display().to_string());
        let segs: Vec<&str> = path
            .split(';')
            .map(|s| s.trim())
            .filter(|s| !s.is_empty())
            .take(12)
            .collect();
        let mut detail = String::from("Expected PATH (normalized), managed bin should be first:\n");
        detail.push_str(&format!("  [0] {}\n", expected_norm));
        detail.push_str("Actual HKCU Path (normalized, leading segments):\n");
        for (i, s) in segs.iter().enumerate() {
            detail.push_str(&format!("  [{}] {}\n", i, normalize_win_path_entry(s)));
        }
        return Err(AppError::msg(format!(
            "PATH invariant failed: first HKCU segment is not managed bin.\n{}  raw first segment: {}\n  expected bin: {}",
            detail,
            first,
            bin_dir.display()
        )));
    }
    Ok(())
}

/// HKCU `Path` as stored (for structured install logs).
#[cfg(windows)]
pub fn read_hkcu_path_raw() -> Result<String> {
    use winreg::enums::*;
    use winreg::RegKey;

    let hcu = RegKey::predef(HKEY_CURRENT_USER);
    let env = hcu
        .open_subkey("Environment")
        .map_err(|e| AppError::msg(e.to_string()))?;
    Ok(env.get_value::<String, _>("Path").unwrap_or_default())
}

/// Write HKCU `Path` and confirm the managed `bin` is first; retries briefly for registry propagation races.
#[cfg(windows)]
pub fn ensure_windows_user_path_verified(bin_dir: &Path, install_prefix: &Path) -> Result<bool> {
    use std::thread;
    use std::time::Duration;

    use std::time::{SystemTime, UNIX_EPOCH};

    let mut any_change = false;
    let mut last_err: Option<String> = None;
    for _ in 0..3 {
        let changed = ensure_windows_user_path(bin_dir, install_prefix)?;
        any_change |= changed;
        let jitter_ms = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| (d.subsec_micros() % 30) as u64)
            .unwrap_or(0);
        thread::sleep(Duration::from_millis(50 + jitter_ms));
        match verify_hkcu_path_invariant(bin_dir) {
            Ok(()) => return Ok(any_change),
            Err(e) => last_err = Some(e.to_string()),
        }
    }
    Err(AppError::msg(
        last_err.unwrap_or_else(|| "HKCU Path invariant failed after retries".to_string()),
    ))
}

#[cfg(not(windows))]
pub fn read_hkcu_path_raw() -> Result<String> {
    Ok(String::new())
}

#[cfg(not(windows))]
pub fn ensure_windows_user_path_verified(_bin_dir: &Path, _install_prefix: &Path) -> Result<bool> {
    Ok(false)
}

/// Put managed `bin_dir` at index 0 of HKCU `Path`. Removes managed-bin duplicates, dedupes all segments,
/// strips CMake-style dev `kern.exe` dirs and any **HKCU** directory containing `kern.exe` outside `install_prefix`.
/// Returns `true` if the registry value changed.
#[cfg(windows)]
pub fn ensure_windows_user_path(bin_dir: &Path, install_prefix: &Path) -> Result<bool> {
    use winreg::enums::*;
    use winreg::RegKey;

    let hcu = RegKey::predef(HKEY_CURRENT_USER);
    let (env, _) = hcu.create_subkey("Environment").map_err(|e| AppError::msg(e.to_string()))?;
    let path: String = env.get_value("Path").unwrap_or_default();
    let needle = bin_dir.display().to_string();

    let parts: Vec<&str> = path
        .split(';')
        .map(|s| s.trim())
        .filter(|s| !s.is_empty())
        .collect();

    let filtered: Vec<&str> = parts
        .iter()
        .copied()
        .filter(|p| {
            !path_segment_is_managed_bin(p, bin_dir)
                && !path_segment_is_likely_kern_cmake_build(p)
                && !path_segment_is_external_kern_exe_dir(p, bin_dir, install_prefix)
        })
        .collect();

    let deduped = dedupe_win_path_segments(&filtered);

    let new_path = if deduped.is_empty() {
        needle
    } else {
        format!("{};{}", needle, deduped.join(";"))
    };

    if new_path == path {
        return Ok(false);
    }

    env.set_value("Path", &new_path)
        .map_err(|e| AppError::msg(e.to_string()))?;
    notify_windows_environment_changed();
    Ok(true)
}

#[cfg(not(windows))]
pub fn ensure_windows_user_path(_bin_dir: &Path, _install_prefix: &Path) -> Result<bool> {
    Ok(false)
}

#[cfg(not(windows))]
pub fn verify_hkcu_path_invariant(_bin_dir: &Path) -> Result<()> {
    Ok(())
}

/// Machine PATH segments that contain `kern.exe` or `kern.cmd` (HKLM — bootstrapper does not modify these).
#[cfg(windows)]
pub fn hklm_path_segments_with_kern_launchers() -> Vec<String> {
    use std::collections::HashSet;
    use winreg::enums::*;
    use winreg::RegKey;

    let Ok(env) = RegKey::predef(HKEY_LOCAL_MACHINE).open_subkey(
        r"SYSTEM\CurrentControlSet\Control\Session Manager\Environment",
    ) else {
        return Vec::new();
    };
    let path: String = env.get_value("Path").unwrap_or_default();
    let mut seen = HashSet::<String>::new();
    let mut out = Vec::new();
    for part in path.split(';') {
        let t = part.trim();
        if t.is_empty() {
            continue;
        }
        let p = Path::new(t);
        if !p.join("kern.exe").is_file() && !p.join("kern.cmd").is_file() {
            continue;
        }
        let key = normalize_win_path_entry(t);
        if seen.insert(key) {
            out.push(t.to_string());
        }
    }
    out
}

#[cfg(not(windows))]
pub fn hklm_path_segments_with_kern_launchers() -> Vec<String> {
    Vec::new()
}

pub fn path_contains_bin() -> bool {
    let Some(path_env) = std::env::var_os("PATH") else {
        return false;
    };
    let home = dirs::home_dir();
    let default_bin = home
        .as_ref()
        .map(|h| h.join(".kern").join("bin"));
    for dir in std::env::split_paths(&path_env) {
        let name = dir.file_name().and_then(|s| s.to_str()).unwrap_or("");
        if name == "bin" {
            if let Some(ref db) = default_bin {
                if dir == *db {
                    return true;
                }
            }
            if dir.to_string_lossy().contains(".kern") && dir.to_string_lossy().ends_with("bin") {
                return true;
            }
        }
    }
    false
}
