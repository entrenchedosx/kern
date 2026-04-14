//! Resolve `kernc.exe` for delegation: `KERN_HOME`, then `./kern-*/kernc.exe` in cwd (deterministic pick).

use std::cmp::Ordering;
use std::path::{Path, PathBuf};
use std::time::UNIX_EPOCH;

pub const KERN_HOME_ENV: &str = "KERN_HOME";
const KERN_EXE: &str = "kernc.exe";

fn exe_in_env_root(root: &Path) -> PathBuf {
    root.join(KERN_EXE)
}

/// True if `root/kern.exe` exists (layout root, no `bin/` subfolder).
pub fn kern_env_root_is_valid(root: &Path) -> bool {
    exe_in_env_root(root).is_file()
}

/// One-line status for `kern-portable doctor`.
pub fn kern_home_doctor_line() -> String {
    match std::env::var(KERN_HOME_ENV) {
        Ok(p) => {
            let pb = PathBuf::from(p.trim());
            if kern_env_root_is_valid(&pb) {
                format!(
                    "[OK]   {} = {} (valid environment root)",
                    KERN_HOME_ENV,
                    pb.display()
                )
            } else {
                format!(
                    "[WARN] {} set but invalid (missing kern.exe at root): {}",
                    KERN_HOME_ENV,
                    pb.display()
                )
            }
        }
        Err(_) => format!(
            "[OK]   {} not set (delegate picks highest kern-<ver>/ version, then newest mtime)",
            KERN_HOME_ENV
        ),
    }
}

/// If `KERN_HOME` points at a valid root, return that `kern.exe` path.
pub fn kern_exe_from_home_env() -> Option<PathBuf> {
    let Ok(p) = std::env::var(KERN_HOME_ENV) else {
        return None;
    };
    let root = PathBuf::from(p.trim());
    let exe = exe_in_env_root(&root);
    if exe.is_file() {
        Some(exe)
    } else {
        None
    }
}

fn kern_dir_numeric_key(name: &str) -> Vec<u32> {
    let rest = name.strip_prefix("kern-").unwrap_or(name);
    let mut nums = Vec::new();
    let mut cur = String::new();
    for c in rest.chars() {
        if c.is_ascii_digit() {
            cur.push(c);
        } else if !cur.is_empty() {
            if let Ok(n) = cur.parse::<u32>() {
                nums.push(n);
            }
            cur.clear();
        }
    }
    if !cur.is_empty() {
        if let Ok(n) = cur.parse::<u32>() {
            nums.push(n);
        }
    }
    nums
}

/// Compare folder names so **higher** `kern-<ver>` wins (e.g. 1.0.20 > 1.0.19); tie → newer mtime → name.
fn cmp_kern_dir_preference(a: &(PathBuf, u64, String), b: &(PathBuf, u64, String)) -> Ordering {
    let va = kern_dir_numeric_key(&a.2);
    let vb = kern_dir_numeric_key(&b.2);
    let n = va.len().max(vb.len());
    for i in 0..n {
        let av = va.get(i).copied().unwrap_or(0);
        let bv = vb.get(i).copied().unwrap_or(0);
        if av != bv {
            return bv.cmp(&av);
        }
    }
    b.1.cmp(&a.1).then_with(|| b.2.cmp(&a.2))
}

/// `kern-*/kern.exe` under `cwd`. If several exist, pick **highest parsed version** from the folder name,
/// then **newest mtime**, then lexicographically greatest name.
pub fn kern_exe_from_cwd_kern_dirs(cwd: &Path) -> Result<PathBuf, String> {
    let rd = std::fs::read_dir(cwd).map_err(|e| format!("read cwd: {}", e))?;
    let mut candidates: Vec<(PathBuf, u64, String)> = Vec::new();
    for ent in rd.flatten() {
        let p = ent.path();
        if !p.is_dir() {
            continue;
        }
        let name = ent.file_name();
        let name_s = name.to_string_lossy();
        if !name_s.starts_with("kern-") {
            continue;
        }
        let exe = exe_in_env_root(&p);
        if !exe.is_file() {
            continue;
        }
        let mt = std::fs::metadata(&p)
            .ok()
            .and_then(|m| m.modified().ok())
            .and_then(|t| t.duration_since(UNIX_EPOCH).ok())
            .map(|d| d.as_secs())
            .unwrap_or(0);
        candidates.push((exe, mt, name_s.into_owned()));
    }
    if candidates.is_empty() {
        return Err(
            "no kern-*/kern.exe in current directory (set KERN_HOME or run kern-portable init)"
                .into(),
        );
    }
    candidates.sort_by(|a, b| cmp_kern_dir_preference(a, b));
    Ok(candidates[0].0.clone())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    #[test]
    fn finds_kern_home_style() {
        let tmp = std::env::temp_dir().join(format!("kern-path-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&tmp);
        let env_root = tmp.join("kern-v1.0.0");
        fs::create_dir_all(&env_root).unwrap();
        fs::write(env_root.join(KERN_EXE), b"x").unwrap();
        assert!(kern_env_root_is_valid(&env_root));
        let _ = fs::remove_dir_all(&tmp);
    }

    #[test]
    fn picks_higher_version_over_lower_in_cwd() {
        let tmp = std::env::temp_dir().join(format!("kern-pick-test-{}", std::process::id()));
        let _ = fs::remove_dir_all(&tmp);
        fs::create_dir_all(&tmp).unwrap();
        let old = tmp.join("kern-1.0.0");
        let new = tmp.join("kern-1.0.20");
        fs::create_dir_all(&old).unwrap();
        fs::create_dir_all(&new).unwrap();
        fs::write(old.join(KERN_EXE), b"o").unwrap();
        fs::write(new.join(KERN_EXE), b"n").unwrap();
        let picked = kern_exe_from_cwd_kern_dirs(&tmp).unwrap();
        assert_eq!(picked, new.join(KERN_EXE));
        let _ = fs::remove_dir_all(&tmp);
    }
}
