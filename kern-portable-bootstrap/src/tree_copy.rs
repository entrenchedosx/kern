//! Recursive directory copy (subset of kern-bootstrap).

use crate::error::{path_ctx, PortableError, Result};
use std::fs;
use std::path::Path;

pub fn copy_dir_all(src: &Path, dst: &Path) -> Result<()> {
    fs::create_dir_all(dst).map_err(|e| path_ctx(&dst.to_path_buf(), e))?;
    for e in fs::read_dir(src).map_err(|e| path_ctx(&src.to_path_buf(), e))? {
        let e = e.map_err(|e| path_ctx(&src.to_path_buf(), e))?;
        let p = e.path();
        let t = dst.join(e.file_name());
        let ft = e.file_type().map_err(|e| path_ctx(&p, e))?;
        if ft.is_symlink() {
            return Err(PortableError::msg(format!(
                "unexpected symlink in copy (use extract on host): {}",
                p.display()
            )));
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
