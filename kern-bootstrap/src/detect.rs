use crate::error::{AppError, Result};
use std::env;
use std::ffi::OsStr;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Debug, Clone)]
pub struct BinaryProbe {
    pub path: PathBuf,
    pub version_line: Option<String>,
}

/// First match on PATH (order preserved).
/// First match for `name` on an explicit PATH string (used when `where` is broken).
pub fn first_on_path_in(path_var: &OsStr, name: &str) -> Option<PathBuf> {
    let pathext = env::var_os("PATHEXT");
    for dir in env::split_paths(path_var) {
        let base = dir.join(name);
        if is_executable_file(&base) {
            return Some(base);
        }
        if let Some(ref ext) = pathext {
            for e in env::split_paths(ext) {
                let with = dir.join(format!("{}{}", name, e.to_string_lossy()));
                if is_executable_file(&with) {
                    return Some(with);
                }
            }
        }
    }
    None
}

pub fn which_first(name: &str) -> Option<PathBuf> {
    let path_env = env::var_os("PATH")?;
    let pathext = env::var_os("PATHEXT");
    for dir in env::split_paths(&path_env) {
        let base = dir.join(name);
        if is_executable_file(&base) {
            return Some(base);
        }
        if let Some(ref ext) = pathext {
            for e in env::split_paths(ext) {
                let e = e.to_string_lossy();
                let with = dir.join(format!("{}{}", name, e));
                if is_executable_file(&with) {
                    return Some(with);
                }
            }
        }
    }
    None
}

fn is_executable_file(p: &Path) -> bool {
    if !p.is_file() {
        return false;
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        return p
            .metadata()
            .map(|m| m.permissions().mode() & 0o111 != 0)
            .unwrap_or(false);
    }
    #[cfg(windows)]
    {
        return true;
    }
    #[cfg(not(any(unix, windows)))]
    {
        return true;
    }
}

/// All `kern` executables found on PATH (same name, different dirs).
pub fn all_on_path(name: &str) -> Vec<PathBuf> {
    let mut out = Vec::new();
    let Some(path_env) = env::var_os("PATH") else {
        return out;
    };
    let pathext = env::var_os("PATHEXT");
    for dir in env::split_paths(&path_env) {
        let base = dir.join(name);
        if is_executable_file(&base) {
            out.push(base);
        }
        if let Some(ref ext) = pathext {
            for e in env::split_paths(ext) {
                let with = dir.join(format!("{}{}", name, e.to_string_lossy()));
                if is_executable_file(&with) {
                    out.push(with);
                }
            }
        }
    }
    out
}

pub fn run_version_line(bin: &Path) -> Option<String> {
    let out = Command::new(bin).arg("--version").output().ok()?;
    if !out.status.success() {
        return None;
    }
    let s = String::from_utf8_lossy(&out.stdout).trim().to_string();
    if s.is_empty() {
        Some(String::from_utf8_lossy(&out.stderr).trim().to_string())
    } else {
        Some(s)
    }
}

#[allow(dead_code)] // Public helper for embedders / future `which`-style tooling
pub fn probe_kern() -> Option<BinaryProbe> {
    let p = which_first("kern")?;
    let v = run_version_line(&p);
    Some(BinaryProbe { path: p, version_line: v })
}

pub fn probe_kargo() -> Option<BinaryProbe> {
    let p = which_first("kargo")?;
    let v = run_version_line(&p);
    Some(BinaryProbe { path: p, version_line: v })
}

pub fn dedupe_paths(paths: &[PathBuf]) -> Vec<PathBuf> {
    let mut seen = std::collections::HashSet::new();
    let mut out = Vec::new();
    for p in paths {
        let canon = p.canonicalize().unwrap_or_else(|_| p.clone());
        if seen.insert(canon) {
            out.push(p.clone());
        }
    }
    out
}

fn where_parent_dir_key(path: &Path) -> String {
    path.parent()
        .map(|p| {
            p.display()
                .to_string()
                .replace('/', "\\")
                .trim_end_matches('\\')
                .to_lowercase()
        })
        .unwrap_or_else(|| ".".to_string())
}

fn where_ext_score(path: &Path) -> u8 {
    match path
        .extension()
        .and_then(|e| e.to_str())
        .unwrap_or("")
        .to_ascii_lowercase()
        .as_str()
    {
        "cmd" => 4,
        "bat" => 3,
        "exe" => 2,
        "com" => 1,
        _ => 0,
    }
}

/// Collapse `where`-style duplicates that share the same directory; prefer `.cmd` over `.exe`.
pub fn normalize_where_by_directory_prefer_cmd(paths: Vec<PathBuf>) -> Vec<PathBuf> {
    use std::collections::HashMap;
    let mut best: HashMap<String, PathBuf> = HashMap::new();
    let mut order: Vec<String> = Vec::new();
    for p in paths {
        let key = where_parent_dir_key(&p);
        if !best.contains_key(&key) {
            order.push(key.clone());
        }
        let insert = match best.get(&key) {
            None => true,
            Some(cur) => where_ext_score(&p) > where_ext_score(cur),
        };
        if insert {
            best.insert(key, p);
        }
    }
    order.into_iter().filter_map(|k| best.get(&k).cloned()).collect()
}

pub fn default_user_prefix() -> PathBuf {
    dirs::home_dir()
        .unwrap_or_else(|| PathBuf::from("."))
        .join(".kern")
}

pub fn default_system_prefix() -> PathBuf {
    #[cfg(windows)]
    {
        PathBuf::from(r"C:\Program Files\Kern")
    }
    #[cfg(not(windows))]
    {
        PathBuf::from("/usr/local")
    }
}

pub fn is_writable_dir(p: &Path) -> bool {
    if !p.exists() {
        return p
            .parent()
            .map(|x| is_writable_dir(x))
            .unwrap_or(false);
    }
    p.metadata()
        .ok()
        .map(|m| !m.permissions().readonly())
        .unwrap_or(false)
}

pub fn ensure_prefix_writable(prefix: &Path) -> Result<()> {
    if prefix.exists() {
        if !is_writable_dir(prefix) {
            return Err(AppError::msg(format!(
                "install prefix is not writable: {} (try --user or run with elevated permissions)",
                prefix.display()
            )));
        }
        return Ok(());
    }
    if let Some(parent) = prefix.parent() {
        if parent.as_os_str().is_empty() {
            return Ok(());
        }
        if !parent.exists() {
            return ensure_prefix_writable(parent);
        }
        if !is_writable_dir(parent) {
            return Err(AppError::msg(format!(
                "cannot create install prefix (parent not writable): {}",
                parent.display()
            )));
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn normalize_where_prefers_cmd_in_same_directory() {
        let v = vec![
            PathBuf::from(r"C:\fake\bin\kern.exe"),
            PathBuf::from(r"C:\fake\bin\kern.cmd"),
        ];
        let out = normalize_where_by_directory_prefer_cmd(v);
        assert_eq!(out.len(), 1);
        assert!(out[0].to_string_lossy().ends_with("kern.cmd"));
    }

    #[test]
    fn normalize_where_keeps_distinct_directories() {
        let v = vec![
            PathBuf::from(r"D:\a\kern.exe"),
            PathBuf::from(r"D:\b\kern.cmd"),
        ];
        let out = normalize_where_by_directory_prefer_cmd(v);
        assert_eq!(out.len(), 2);
    }

    #[test]
    #[cfg(windows)]
    fn first_on_path_in_finds_temp_kern() {
        let dir = std::env::temp_dir().join("kern-bs-path-scan-test");
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).unwrap();
        let fake = dir.join("kern.exe");
        std::fs::write(&fake, b"x").unwrap();
        let mut path = std::ffi::OsString::from(dir.as_os_str());
        path.push(";");
        path.push(std::env::var_os("PATH").unwrap_or_default());
        let found = first_on_path_in(&path, "kern").expect("kern on synthetic PATH");
        let a = found.file_name().and_then(|s| s.to_str()).unwrap_or("");
        let b = fake.file_name().and_then(|s| s.to_str()).unwrap_or("");
        assert!(
            a.eq_ignore_ascii_case(b),
            "expected first match in our temp dir (got {a} vs {b})"
        );
        assert_eq!(found.parent(), Some(dir.as_path()));
        let _ = std::fs::remove_dir_all(&dir);
    }
}
