//! Portable `.kern/` bootstrap (Windows-first).
#![forbid(unsafe_code)]

pub mod artifact_cache;
pub mod doctor;
pub mod download;
pub mod error;
pub mod extract;
pub mod github;
pub mod install;
pub mod node_embed;
pub mod paths;
pub mod sha256_verify;
pub mod tree_copy;

pub use error::{PortableError, Result};
pub use paths::{find_kern_env_dir, find_kern_env_dir_with_cache};

/// Walk parents from `start` for `.kern/bin/kern.exe` (respects [`paths::KERN_ROOT_CACHE_ENV`]).
pub fn find_kern_root(start: &std::path::Path) -> Option<std::path::PathBuf> {
    find_kern_env_dir_with_cache(start)
}

use std::path::Path;
use std::process::{Command, Stdio};

/// Run `.kern/bin/kern.exe` with the same arguments (argv after program name).
/// On Windows there is no Unix `execve`; we spawn and exit with the child status.
pub fn delegate_to_local_kern(forward_args: &[String]) -> ! {
    if !cfg!(windows) {
        eprintln!("error: delegation is only implemented on Windows.");
        std::process::exit(1);
    }

    let cwd = std::env::current_dir().unwrap_or_else(|_| Path::new(".").to_path_buf());
    let Some(env_dir) = find_kern_env_dir_with_cache(&cwd) else {
        eprintln!("No Kern environment found. Run `kern-portable init`.");
        std::process::exit(2);
    };

    let exe = env_dir.join("bin").join("kern.exe");
    if !exe.is_file() {
        eprintln!("No Kern environment found. Run `kern-portable init`.");
        std::process::exit(2);
    }

    let status = Command::new(&exe)
        .args(forward_args)
        .stdin(Stdio::inherit())
        .stdout(Stdio::inherit())
        .stderr(Stdio::inherit())
        .status()
        .unwrap_or_else(|e| {
            eprintln!("failed to run {}: {}", exe.display(), e);
            std::process::exit(1);
        });

    let code = status.code().unwrap_or(1);
    std::process::exit(code);
}
