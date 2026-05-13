//! `kern-bootstrap which` / `env` — PATH diagnostics.

use crate::detect::{self, dedupe_paths, run_version_line};
use crate::error::Result;
use crate::progress::Progress;
pub fn run_which(verbose: bool, color: bool) -> Result<()> {
    let mut prog = Progress::new(color, verbose, 1);
    prog.step("`kern` / `kargo` resolution (current process PATH)...");

    let kern_raw = dedupe_paths(&detect::all_on_path("kern"));
    #[cfg(windows)]
    let kerns = detect::normalize_where_by_directory_prefer_cmd(kern_raw);
    #[cfg(not(windows))]
    let kerns = kern_raw;
    if kerns.is_empty() {
        prog.warn("kern: not found on PATH");
    } else {
        prog.ok("kern:");
        for (i, p) in kerns.iter().enumerate() {
            let tag = if i == 0 { "[ACTIVE]" } else { "[shadowed]" };
            let v = run_version_line(p)
                .filter(|s| !s.trim().is_empty())
                .unwrap_or_else(|| "(kern --version failed)".into());
            prog.always(&format!("  {} {}", tag, p.display()));
            prog.always(&format!("         {}", v.trim()));
        }
    }

    let kargo_raw = dedupe_paths(&detect::all_on_path("kargo"));
    #[cfg(windows)]
    let kargos = detect::normalize_where_by_directory_prefer_cmd(kargo_raw);
    #[cfg(not(windows))]
    let kargos = kargo_raw;
    if kargos.is_empty() {
        prog.warn("kargo: not found on PATH");
    } else {
        prog.ok("kargo:");
        for (i, p) in kargos.iter().enumerate() {
            let tag = if i == 0 { "[ACTIVE]" } else { "[shadowed]" };
            let v = run_version_line(p)
                .filter(|s| !s.trim().is_empty())
                .unwrap_or_else(|| "(kargo --version failed)".into());
            prog.always(&format!("  {} {}", tag, p.display()));
            prog.always(&format!("         {}", v.trim()));
        }
    }
    Ok(())
}

pub fn run_env(verbose: bool, color: bool) -> Result<()> {
    let mut prog = Progress::new(color, verbose, 1);
    prog.step("PATH segments (order = search order)...");

    let Ok(path_var) = std::env::var("PATH") else {
        prog.err("PATH is not set in this process.");
        return Ok(());
    };

    for (i, part) in std::env::split_paths(&path_var).enumerate() {
        let s = part.to_string_lossy();
        if s.is_empty() {
            continue;
        }
        prog.always(&format!("[{:>3}] {}", i, s));
    }
    Ok(())
}
