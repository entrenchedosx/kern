//! Recursive directory copy with symlink handling (npm `node_modules/.bin` on Windows).

use crate::error::{path_ctx, AppError, Result};
use std::fs;
use std::path::Path;

/// `node_modules/.bin/*` from Linux-built npm trees are often symlinks. On Windows, `fs::copy` on
/// the link itself can fail with ERROR_INVALID_NAME (123); resolve the target or skip shims Kargo does not need.
fn path_has_node_modules_bin(path: &Path) -> bool {
    let parts: Vec<_> = path.iter().collect();
    parts
        .windows(2)
        .any(|w| w[0] == std::ffi::OsStr::new("node_modules") && w[1] == std::ffi::OsStr::new(".bin"))
}

fn copy_symlink_tree(p: &Path, t: &Path, src_hint: &Path) -> Result<()> {
    let target = match fs::read_link(p) {
        Ok(t) => t,
        Err(e) => {
            #[cfg(windows)]
            if path_has_node_modules_bin(p) {
                return Ok(());
            }
            return Err(path_ctx(p, e));
        }
    };
    let resolved = if target.is_absolute() {
        target
    } else {
        p.parent().unwrap_or(src_hint).join(target)
    };

    let md = fs::metadata(&resolved);
    match md {
        Ok(m) if m.is_dir() => copy_dir_all(&resolved, t),
        Ok(m) if m.is_file() => {
            if let Some(parent) = t.parent() {
                fs::create_dir_all(parent).map_err(|e| path_ctx(&parent.to_path_buf(), e))?;
            }
            fs::copy(&resolved, t).map_err(|e| path_ctx(t, e))?;
            Ok(())
        }
        _ => {
            #[cfg(windows)]
            if path_has_node_modules_bin(p) {
                return Ok(());
            }
            Err(AppError::msg(format!(
                "symlink {:?} -> {:?} has no usable target for install copy",
                p, resolved
            )))
        }
    }
}

pub fn copy_dir_all(src: &Path, dst: &Path) -> Result<()> {
    fs::create_dir_all(dst).map_err(|e| path_ctx(&dst.to_path_buf(), e))?;
    for e in fs::read_dir(src).map_err(|e| path_ctx(&src.to_path_buf(), e))? {
        let e = e.map_err(|e| path_ctx(&src.to_path_buf(), e))?;
        let p = e.path();
        let t = dst.join(e.file_name());
        let ft = e.file_type().map_err(|e| path_ctx(&p, e))?;

        if ft.is_symlink() {
            copy_symlink_tree(&p, &t, src)?;
            continue;
        }

        if ft.is_dir() {
            copy_dir_all(&p, &t)?;
        } else {
            if let Some(parent) = t.parent() {
                fs::create_dir_all(parent).map_err(|e| path_ctx(&parent.to_path_buf(), e))?;
            }
            fs::copy(&p, &t).map_err(|e| path_ctx(&t, e))?;
        }
    }
    Ok(())
}
