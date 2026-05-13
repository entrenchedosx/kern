use crate::error::{path_ctx, PortableError, Result};
use flate2::read::GzDecoder;
use std::fs::File;
use std::io::Read;
use std::path::{Component, Path, PathBuf};

/// Zip external attributes use standard file-type bits (see `zip` crate `unix_mode()`).
const S_IFMT: u32 = 0o170000;
const S_IFDIR: u32 = 0o0040000;

/// `Compress-Archive` / Windows zips often mark dirs via DOS directory bit, not a trailing `/`.
fn zip_entry_is_dir(file: &zip::read::ZipFile<'_>) -> bool {
    if file.is_dir() {
        return true;
    }
    file.unix_mode()
        .map(|m| (m & S_IFMT) == S_IFDIR)
        .unwrap_or(false)
}

/// If a parent path was wrongly created as an empty file, remove it so `create_dir_all` can succeed (Windows 183).
fn create_dir_all_with_file_shadow_fix(path: &Path) -> Result<()> {
    let mut acc = PathBuf::new();
    for c in path.components() {
        acc.push(c);
        if acc.exists() && acc.is_file() {
            std::fs::remove_file(&acc).map_err(|e| path_ctx(&acc, e))?;
        }
    }
    std::fs::create_dir_all(path).map_err(|e| path_ctx(path, e))
}

fn safe_rel_path(path: &Path) -> Result<PathBuf> {
    let mut out = PathBuf::new();
    for c in path.components() {
        match c {
            Component::Normal(x) => {
                let s = x.to_string_lossy();
                if s == ".." {
                    return Err(PortableError::Extract("path traversal in archive".into()));
                }
                out.push(x);
            }
            Component::ParentDir => {
                return Err(PortableError::Extract("path traversal in archive".into()));
            }
            Component::CurDir => {}
            Component::RootDir | Component::Prefix(_) => {}
        }
    }
    Ok(out)
}

pub fn extract_tar_gz(archive: &Path, dest: &Path) -> Result<()> {
    let f = File::open(archive).map_err(|e| path_ctx(&archive.to_path_buf(), e))?;
    let gz = GzDecoder::new(f);
    let mut ar = tar::Archive::new(gz);
    for entry in ar.entries().map_err(|e| PortableError::Extract(e.to_string()))? {
        let mut entry = entry.map_err(|e| PortableError::Extract(e.to_string()))?;
        let path = entry.path().map_err(|e| PortableError::Extract(e.to_string()))?;
        if path.is_absolute() {
            return Err(PortableError::Extract("absolute path in tar".into()));
        }
        let rel = safe_rel_path(&path)?;
        let out = dest.join(&rel);
        if entry.header().entry_type().is_dir() {
            std::fs::create_dir_all(&out).map_err(|e| path_ctx(&out, e))?;
        } else {
            if let Some(p) = out.parent() {
                std::fs::create_dir_all(p).map_err(|e| path_ctx(&p.to_path_buf(), e))?;
            }
            entry
                .unpack(&out)
                .map_err(|e| PortableError::Extract(format!("{}: {}", archive.display(), e)))?;
        }
    }
    Ok(())
}

pub fn extract_zip(archive: &Path, dest: &Path) -> Result<()> {
    let f = File::open(archive).map_err(|e| path_ctx(&archive.to_path_buf(), e))?;
    let mut ar = zip::ZipArchive::new(f).map_err(|e| PortableError::Extract(e.to_string()))?;
    for i in 0..ar.len() {
        let mut file = ar.by_index(i).map_err(|e| PortableError::Extract(e.to_string()))?;
        let name = Path::new(file.name());
        let rel = safe_rel_path(name)?;
        let out = dest.join(rel);
        if zip_entry_is_dir(&file) {
            create_dir_all_with_file_shadow_fix(&out)?;
        } else {
            if let Some(p) = out.parent() {
                create_dir_all_with_file_shadow_fix(p)?;
            }
            if out.exists() && out.is_dir() {
                std::fs::remove_dir_all(&out).map_err(|e| path_ctx(&out, e))?;
            }
            let mut buf: Vec<u8> = Vec::new();
            file
                .read_to_end(&mut buf)
                .map_err(|e| PortableError::Extract(e.to_string()))?;
            std::fs::write(&out, buf).map_err(|e| path_ctx(&out, e))?;
        }
    }
    Ok(())
}

pub fn find_single_subdirectory(dir: &Path) -> Result<PathBuf> {
    let mut entries: Vec<_> = std::fs::read_dir(dir)
        .map_err(|e| path_ctx(&dir.to_path_buf(), e))?
        .filter_map(|e| e.ok())
        .collect();
    entries.retain(|e| e.file_name().to_string_lossy() != ".DS_Store");
    if entries.len() == 1 && entries[0].path().is_dir() {
        return Ok(entries[0].path());
    }
    Err(PortableError::Extract(format!(
        "expected exactly one subdirectory in {}, found {}",
        dir.display(),
        entries.len()
    )))
}
