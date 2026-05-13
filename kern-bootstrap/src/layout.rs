//! Install layout: `versions/<tag>/{kern,kargo}/`, active pointer, and `bin/` launchers.

use crate::error::{path_ctx, AppError, Result};
use std::fs;
use std::path::{Path, PathBuf};

pub const VERSIONS: &str = "versions";
pub const CURRENT: &str = "current";
pub const INSTALL_LOCK: &str = ".install.lock";
/// Legacy single sentinel filename (still detected for older runs).
pub const INSTALLING_SENTINEL_LEGACY: &str = ".installing";
/// Per-process sentinel: `.installing.<pid>` under the prefix (avoids stale collision across PIDs).
pub const INSTALLING_SENTINEL_PREFIX: &str = ".installing.";
pub const ACTIVE_RELEASE_FILE: &str = "active-release.txt";
/// Embedded in `.cmd` shims; `doctor` strict verify prefers this over legacy shape-only checks.
pub const SHIM_TEMPLATE_MARKER: &str = "kern-bootstrap shim v1";
pub const KERN_SUBDIR: &str = "kern";
pub const KARGO_SUBDIR: &str = "kargo";

pub fn versions_dir(prefix: &Path) -> PathBuf {
    prefix.join(VERSIONS)
}

pub fn version_home(prefix: &Path, release_tag: &str) -> PathBuf {
    versions_dir(prefix).join(release_tag)
}

pub fn current_link_path(prefix: &Path) -> PathBuf {
    prefix.join(CURRENT)
}

pub fn lock_path(prefix: &Path) -> PathBuf {
    prefix.join(INSTALL_LOCK)
}

fn installing_sentinel_legacy_path(prefix: &Path) -> PathBuf {
    prefix.join(INSTALLING_SENTINEL_LEGACY)
}

/// True if a prior run left `.installing`, `.installing.<pid>`, or other `.installing.*` crash markers.
pub fn any_installing_sentinel(prefix: &Path) -> bool {
    if installing_sentinel_legacy_path(prefix).is_file() {
        return true;
    }
    let Ok(rd) = fs::read_dir(prefix) else {
        return false;
    };
    for e in rd.flatten() {
        let name = e.file_name();
        let ns = name.to_string_lossy();
        if ns.starts_with(INSTALLING_SENTINEL_PREFIX) && e.path().is_file() {
            return true;
        }
    }
    false
}

/// Create `.installing.<pid>`; returns its path (cleared only after a successful install).
pub fn create_installing_sentinel(prefix: &Path) -> Result<PathBuf> {
    let pid = std::process::id();
    let p = prefix.join(format!("{}{}", INSTALLING_SENTINEL_PREFIX, pid));
    fs::write(&p, format!("{}\n", pid)).map_err(|e| path_ctx(&p, e))?;
    Ok(p)
}

pub fn clear_installing_sentinel_file(path: &Path) {
    let _ = fs::remove_file(path);
}

/// Remove legacy `.installing` and every `.installing.*` file (after a successful repair or install).
pub fn clear_all_installing_sentinels(prefix: &Path) {
    clear_installing_sentinel_file(&installing_sentinel_legacy_path(prefix));
    let Ok(rd) = fs::read_dir(prefix) else {
        return;
    };
    for e in rd.flatten() {
        let name = e.file_name();
        let ns = name.to_string_lossy();
        if ns.starts_with(INSTALLING_SENTINEL_PREFIX) && e.path().is_file() {
            let _ = fs::remove_file(e.path());
        }
    }
}

pub fn read_active_release_file(prefix: &Path) -> Result<Option<String>> {
    let p = prefix.join(ACTIVE_RELEASE_FILE);
    if !p.is_file() {
        return Ok(None);
    }
    let raw = fs::read_to_string(&p).map_err(|e| path_ctx(&p, e))?;
    let line = raw.lines().next().unwrap_or("").trim();
    if line.is_empty() {
        return Ok(None);
    }
    Ok(Some(line.to_string()))
}

/// Resolved absolute path to the active version root (`.../versions/vX.Y.Z`), if any.
/// Broken `current` symlink falls through to `active-release.txt` when present.
pub fn active_version_dir(prefix: &Path) -> Result<Option<PathBuf>> {
    #[cfg(unix)]
    {
        let cur = current_link_path(prefix);
        if cur.is_symlink() {
            let t = fs::read_link(&cur).map_err(|e| path_ctx(&cur, e))?;
            let abs = if t.is_absolute() {
                t
            } else {
                prefix.join(t)
            };
            if let Ok(canon) = abs.canonicalize() {
                if canon.is_dir() {
                    return Ok(Some(canon));
                }
            }
        }
    }
    if let Some(tag) = read_active_release_file(prefix)? {
        let p = version_home(prefix, &tag);
        if p.is_dir() {
            if let Ok(c) = p.canonicalize() {
                return Ok(Some(c));
            }
        }
    }
    Ok(None)
}

/// Unix: `current` is a symlink whose target path does not exist.
#[cfg(unix)]
pub fn current_symlink_broken(prefix: &Path) -> Result<bool> {
    let cur = current_link_path(prefix);
    if !cur.is_symlink() {
        return Ok(false);
    }
    let t = fs::read_link(&cur).map_err(|e| path_ctx(&cur, e))?;
    let abs = if t.is_absolute() {
        t
    } else {
        prefix.join(t)
    };
    Ok(!abs.exists())
}

#[cfg(not(unix))]
pub fn current_symlink_broken(_prefix: &Path) -> Result<bool> {
    Ok(false)
}

pub fn read_active_release_tag(prefix: &Path) -> Result<Option<String>> {
    #[cfg(unix)]
    {
        let cur = current_link_path(prefix);
        if cur.is_symlink() {
            let t = fs::read_link(&cur).map_err(|e| path_ctx(&cur, e))?;
            if let Some(name) = t.file_name().and_then(|s| s.to_str()) {
                return Ok(Some(name.to_string()));
            }
        }
    }
    read_active_release_file(prefix)
}

pub fn list_installed_tags(prefix: &Path) -> Result<Vec<String>> {
    let vd = versions_dir(prefix);
    if !vd.is_dir() {
        return Ok(Vec::new());
    }
    let mut out = Vec::new();
    for e in fs::read_dir(&vd).map_err(|e| path_ctx(&vd, e))? {
        let e = e.map_err(|e| path_ctx(&vd, e))?;
        let p = e.path();
        if p.is_dir() {
            if let Some(name) = e.file_name().to_str() {
                let kern = p.join(KERN_SUBDIR);
                if kern.is_dir() {
                    out.push(name.to_string());
                }
            }
        }
    }
    out.sort();
    Ok(out)
}

/// Atomically switch the active version. Returns the previous active tag (if any) for rollback.
pub fn set_active_version(prefix: &Path, release_tag: &str) -> Result<Option<String>> {
    let prev = read_active_release_tag(prefix)?;
    let vdir = version_home(prefix, release_tag);
    if !vdir.is_dir() {
        return Err(AppError::VersionNotFound(release_tag.to_string()));
    }

    #[cfg(unix)]
    {
        let target_rel = format!("{}/{}", VERSIONS, release_tag);
        let tmp = prefix.join(".current.tmp");
        let _ = fs::remove_file(&tmp);
        std::os::unix::fs::symlink(&target_rel, &tmp).map_err(|e| path_ctx(&tmp, e))?;
        let cur = current_link_path(prefix);
        fs::rename(&tmp, &cur).map_err(|e| path_ctx(&cur, e))?;
        let art = prefix.join("active-release.txt.new");
        fs::write(&art, format!("{}\n", release_tag)).map_err(|e| path_ctx(&art, e))?;
        fs::rename(&art, prefix.join(ACTIVE_RELEASE_FILE)).map_err(|e| {
            path_ctx(&prefix.join(ACTIVE_RELEASE_FILE), e)
        })?;
    }

    #[cfg(windows)]
    {
        let tmp = prefix.join("active-release.txt.new");
        let final_p = prefix.join(ACTIVE_RELEASE_FILE);
        fs::write(&tmp, format!("{}\n", release_tag)).map_err(|e| path_ctx(&tmp, e))?;
        fs::rename(&tmp, &final_p).map_err(|e| path_ctx(&final_p, e))?;

        let link = current_link_path(prefix);
        let _ = remove_windows_current_link(&link);
        let vdir_abs = vdir.canonicalize().map_err(|e| path_ctx(&vdir, e))?;
        let _ = junction::create(&vdir_abs, &link);
    }

    #[cfg(not(any(unix, windows)))]
    {
        let _ = (prefix, release_tag);
        return Err(AppError::UnsupportedPlatform);
    }

    Ok(prev)
}

pub fn rollback_active_version(prefix: &Path, previous: Option<&str>) -> Result<()> {
    if let Some(tag) = previous {
        let _ = set_active_version(prefix, tag);
    }
    Ok(())
}

#[cfg(windows)]
fn remove_windows_current_link(link: &Path) -> Result<()> {
    if link.exists() {
        let _ = fs::remove_dir(link);
        let _ = fs::remove_file(link);
    }
    Ok(())
}

pub fn remove_active_pointer(prefix: &Path) -> Result<()> {
    let f = prefix.join(ACTIVE_RELEASE_FILE);
    let _ = fs::remove_file(&f);
    let cur = current_link_path(prefix);
    if cur.exists() {
        #[cfg(unix)]
        {
            fs::remove_file(&cur).map_err(|e| path_ctx(&cur, e))?;
        }
        #[cfg(windows)]
        {
            remove_windows_current_link(&cur)?;
        }
        #[cfg(not(any(unix, windows)))]
        {
            let _ = cur;
        }
    }
    Ok(())
}

pub fn kern_bundle_dir(version_root: &Path) -> PathBuf {
    version_root.join(KERN_SUBDIR)
}

pub fn kargo_bundle_dir(version_root: &Path) -> PathBuf {
    version_root.join(KARGO_SUBDIR)
}
