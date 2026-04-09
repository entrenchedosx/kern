//! Locate project-local `.kern` directory (parent walk).

use std::path::{Path, PathBuf};

const KERN_DIR: &str = ".kern";
const KERN_EXE: &str = "kern.exe";
pub const KERN_ROOT_CACHE_ENV: &str = "KERN_ROOT_CACHE";

fn kern_env_valid(env_dir: &Path) -> bool {
    env_dir.join("bin").join(KERN_EXE).is_file()
}

/// True if `dir` looks like a `.kern` root (`bin/kern.exe` present).
pub fn kern_env_root_is_valid(env_dir: &Path) -> bool {
    kern_env_valid(env_dir)
}

/// One-line status for `kern-portable doctor` (`KERN_ROOT_CACHE`).
pub fn kern_root_cache_doctor_line() -> String {
    match std::env::var(KERN_ROOT_CACHE_ENV) {
        Ok(p) => {
            let pb = PathBuf::from(p.trim());
            if kern_env_valid(&pb) {
                format!(
                    "[OK]   {} = {} (points at a valid environment)",
                    KERN_ROOT_CACHE_ENV,
                    pb.display()
                )
            } else {
                format!(
                    "[WARN] {} set but invalid (missing bin/kern.exe): {}",
                    KERN_ROOT_CACHE_ENV,
                    pb.display()
                )
            }
        }
        Err(_) => format!(
            "[OK]   {} not set (using parent directory search)",
            KERN_ROOT_CACHE_ENV
        ),
    }
}

/// If `KERN_ROOT_CACHE` is set to an absolute path to `.kern`, use it when valid.
pub fn find_kern_env_dir_with_cache(start: &Path) -> Option<PathBuf> {
    if let Ok(p) = std::env::var(KERN_ROOT_CACHE_ENV) {
        let pb = PathBuf::from(p.trim());
        if kern_env_valid(&pb) {
            return Some(pb);
        }
    }
    find_kern_env_dir(start)
}

/// Walk from `start` upward to filesystem root. Returns the path to `.kern` if
/// `.kern/bin/kern.exe` exists.
pub fn find_kern_env_dir(start: &Path) -> Option<PathBuf> {
    let mut cur = start.to_path_buf();
    loop {
        let candidate = cur.join(KERN_DIR).join("bin").join(KERN_EXE);
        if candidate.is_file() {
            return Some(cur.join(KERN_DIR));
        }
        if !cur.pop() {
            break;
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    #[test]
    fn finds_kern_in_parent() {
        let tmp = std::env::temp_dir().join(format!(
            "kern-port-test-{}",
            std::process::id()
        ));
        let _ = fs::remove_dir_all(&tmp);
        fs::create_dir_all(tmp.join("a").join("b")).unwrap();
        fs::create_dir_all(tmp.join("a").join(".kern").join("bin")).unwrap();
        fs::write(tmp.join("a").join(".kern").join("bin").join("kern.exe"), b"x").unwrap();
        let cwd = tmp.join("a").join("b");
        let k = find_kern_env_dir(&cwd).unwrap();
        assert!(k.ends_with(".kern"));
        assert_eq!(k, tmp.join("a").join(".kern"));
        let _ = fs::remove_dir_all(&tmp);
    }
}
