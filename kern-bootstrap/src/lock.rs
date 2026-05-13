//! Cross-process install lock under `<prefix>/.install.lock`.

use crate::error::{path_ctx, AppError, Result};
use crate::layout::lock_path;
use crate::progress::Progress;
use fs4::fs_std::FileExt;
use std::fs::OpenOptions;
use std::io::{BufRead, ErrorKind, Write};
use std::path::Path;
use std::time::Duration;

pub struct InstallLock {
    _file: std::fs::File,
}

impl InstallLock {
    /// Acquire exclusive lock; optionally wait with interactive prompt.
    pub fn acquire(
        prefix: &Path,
        prog: &Progress,
        wait_prompt: bool,
        non_interactive: bool,
    ) -> Result<Self> {
        if let Some(parent) = prefix.parent() {
            if !parent.as_os_str().is_empty() {
                std::fs::create_dir_all(parent).map_err(|e| path_ctx(parent, e))?;
            }
        }
        std::fs::create_dir_all(prefix).map_err(|e| path_ctx(prefix, e))?;

        let path = lock_path(prefix);
        let mut f = OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .open(&path)
            .map_err(|e| path_ctx(&path, e))?;

        loop {
            match f.try_lock_exclusive() {
                Ok(()) => {
                    let pid = std::process::id();
                    let _ = f.set_len(0);
                    let _ = writeln!(&mut f, "{}", pid);
                    let _ = f.sync_all();
                    return Ok(InstallLock { _file: f });
                }
                Err(e) if e.kind() == ErrorKind::WouldBlock => {
                    if non_interactive {
                        return Err(AppError::LockHeld(path.display().to_string()));
                    }
                    if !wait_prompt {
                        return Err(AppError::LockHeld(path.display().to_string()));
                    }
                    prog.warn(&format!(
                        "Another installation holds {}.\n  [1] Wait and retry\n  [2] Exit now",
                        path.display()
                    ));
                    prog.always("Choice (default 1): ");
                    let mut line = String::new();
                    std::io::stdin()
                        .lock()
                        .read_line(&mut line)
                        .map_err(|e| AppError::msg(e.to_string()))?;
                    match line.trim() {
                        "" | "1" => {
                            std::thread::sleep(Duration::from_secs(2));
                            continue;
                        }
                        _ => {
                            return Err(AppError::LockHeld(path.display().to_string()));
                        }
                    }
                }
                Err(e) => {
                    return Err(AppError::msg(format!(
                        "lock {}: {}",
                        path.display(),
                        e
                    )));
                }
            }
        }
    }
}

// Lock is released when `_file` is closed on drop.
