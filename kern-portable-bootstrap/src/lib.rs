//! Portable `kern-<version>/` bootstrap (Windows-first).
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
pub use paths::{kern_exe_from_cwd_kern_dirs, kern_exe_from_home_env, kern_home_doctor_line};

use std::path::Path;
use std::process::{Command, Stdio};

/// Run local `kern.exe` with forwarded args. Sets `KERN_HOME` for the child to the env root.
pub fn delegate_to_local_kern(forward_args: &[String]) -> ! {
    if !cfg!(windows) {
        eprintln!("error: delegation is only implemented on Windows.");
        std::process::exit(1);
    }

    let cwd = std::env::current_dir().unwrap_or_else(|_| Path::new(".").to_path_buf());
    let (exe, env_root) = if let Some(ex) = kern_exe_from_home_env() {
        let root = ex
            .parent()
            .expect("kern.exe has parent")
            .to_path_buf();
        (ex, root)
    } else {
        match kern_exe_from_cwd_kern_dirs(&cwd) {
            Ok(ex) => {
                let root = ex
                    .parent()
                    .expect("kern.exe has parent")
                    .to_path_buf();
                (ex, root)
            }
            Err(msg) => {
                eprintln!("error: {}", msg);
                eprintln!("hint: run `kern-portable init` or set {} to your environment folder.", paths::KERN_HOME_ENV);
                std::process::exit(2);
            }
        }
    };

    let status = Command::new(&exe)
        .args(forward_args)
        .env(paths::KERN_HOME_ENV, &env_root)
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
