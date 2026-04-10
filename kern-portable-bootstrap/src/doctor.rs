//! `kern-portable doctor` — sanity checks for `kern-*/` layout.

use crate::artifact_cache;
use crate::error::{path_ctx, PortableError, Result};
use crate::install::find_installed_env_root;
use crate::paths::{kern_exe_from_cwd_kern_dirs, kern_exe_from_home_env, kern_home_doctor_line};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

fn normalize_version_output(s: &str) -> String {
    s.lines()
        .map(str::trim)
        .filter(|l| !l.is_empty())
        .collect::<Vec<_>>()
        .join(" ")
}

fn resolve_env_root(project_root: &Path) -> Result<PathBuf> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| project_root.to_path_buf());
    if let Some(exe) = kern_exe_from_home_env() {
        return exe
            .parent()
            .map(|p| p.to_path_buf())
            .ok_or_else(|| PortableError::msg("invalid KERN_HOME"));
    }
    if let Ok(exe) = kern_exe_from_cwd_kern_dirs(&cwd) {
        return exe
            .parent()
            .map(|p| p.to_path_buf())
            .ok_or_else(|| PortableError::msg("invalid kern.exe path"));
    }
    find_installed_env_root(project_root)
}

pub fn run_doctor(project_root: &Path, fix: bool) -> Result<()> {
    if !cfg!(windows) {
        return Err(PortableError::msg("doctor is only supported on Windows."));
    }

    let env_dir = resolve_env_root(project_root)?;

    println!("{}", kern_home_doctor_line());

    let kern_exe = env_dir.join("kern.exe");
    let kargo_exe = env_dir.join("kargo.exe");
    let runtime = env_dir.join("runtime");
    let cfg = env_dir.join("config.toml");

    let mut failed = false;

    if kern_exe.is_file() {
        println!("[OK]   kern.exe at environment root");
        let st = Command::new(&kern_exe)
            .arg("--version")
            .stdin(std::process::Stdio::null())
            .output()
            .map_err(|e| PortableError::msg(format!("kern --version: {}", e)))?;
        let inner_ver = if st.status.success() {
            let t = normalize_version_output(&String::from_utf8_lossy(&st.stdout));
            println!("[OK]   kern.exe --version: {}", t);
            Some(t)
        } else {
            println!("[FAIL] kern.exe --version failed");
            failed = true;
            None
        };

        if let Ok(bootstrap) = std::env::current_exe() {
            let st_b = Command::new(&bootstrap)
                .arg("--version")
                .stdin(std::process::Stdio::null())
                .output();
            match st_b {
                Ok(o) if o.status.success() => {
                    let outer = normalize_version_output(&String::from_utf8_lossy(&o.stdout));
                    println!("[OK]   bootstrapper --version: {}", outer);
                    if let Some(ref inn) = inner_ver {
                        let same_exe = fs::canonicalize(&bootstrap)
                            .ok()
                            .zip(fs::canonicalize(&kern_exe).ok())
                            .map(|(a, b)| a == b)
                            .unwrap_or(false);
                        if same_exe {
                            println!("[OK]   delegation: bootstrapper and core are the same binary");
                        } else if outer == *inn {
                            println!("[OK]   delegation: version strings match (bootstrapper vs core)");
                        } else {
                            println!(
                                "[WARN] delegation: bootstrapper vs core --version differ"
                            );
                            println!("       bootstrapper: {}", outer);
                            println!("       core:         {}", inn);
                        }
                    }
                }
                Ok(_) => println!("[WARN] bootstrapper --version failed (non-fatal)"),
                Err(_) => println!("[WARN] could not run bootstrapper --version"),
            }
        }
    } else {
        println!("[FAIL] missing kern.exe at environment root");
        failed = true;
    }

    if kargo_exe.is_file() {
        println!("[OK]   kargo.exe at environment root");
        let st = Command::new(&kargo_exe)
            .arg("--version")
            .stdin(std::process::Stdio::null())
            .output();
        match st {
            Ok(o) if o.status.success() => {
                println!(
                    "[OK]   kargo --version: {}",
                    String::from_utf8_lossy(&o.stdout).trim()
                );
            }
            Ok(_) => println!("[WARN] kargo --version non-zero"),
            Err(e) => println!("[WARN] could not run kargo.exe: {}", e),
        }
    } else {
        println!("[FAIL] missing kargo.exe");
        failed = true;
    }

    if runtime.is_dir() && runtime.read_dir().map(|d| d.count() > 0).unwrap_or(false) {
        println!("[OK]   runtime/ is non-empty");
    } else {
        println!("[WARN] runtime/ missing or empty");
    }

    let lib_kern = env_dir.join("lib").join("kern");
    if lib_kern.is_dir() {
        println!("[OK]   lib/kern present");
    } else {
        println!("[WARN] lib/kern missing (stdlib may not resolve)");
    }

    if cfg.is_file() {
        let raw = fs::read_to_string(&cfg).map_err(|e| path_ctx(&cfg, e))?;
        println!("[OK]   config.toml present");
        let mut pinned: Option<String> = None;
        if let Some(line) = raw.lines().find(|l| l.contains("kern_version")) {
            println!("       {}", line.trim());
            let rest: String = line
                .split_once('=')
                .map(|(_, r)| r.trim().trim_matches('"').to_string())
                .unwrap_or_default();
            if !rest.is_empty() {
                pinned = Some(rest);
            }
        }
        if let (Some(p), true) = (pinned, kern_exe.is_file()) {
            let st = Command::new(&kern_exe)
                .arg("--version")
                .stdin(std::process::Stdio::null())
                .output();
            if let Ok(o) = st {
                let reported = String::from_utf8_lossy(&o.stdout);
                if reported.contains(&p) {
                    println!("[OK]   kern --version mentions config kern_version ({})", p);
                } else {
                    println!(
                        "[WARN] kern --version output may not match config kern_version ({})",
                        p
                    );
                }
            }
        }
    } else {
        println!("[WARN] config.toml missing");
    }

    check_optional_cache_integrity(&env_dir, fix);

    if failed {
        std::process::exit(2);
    }
    println!("Doctor: environment looks usable.");
    Ok(())
}

fn check_optional_cache_integrity(env_dir: &Path, fix: bool) {
    let base = env_dir.join("cache").join("artifacts");
    if !base.is_dir() {
        println!("[OK]   cache/artifacts (nothing to verify yet)");
        return;
    }

    let Ok(rd) = fs::read_dir(&base) else {
        println!("[WARN] could not read cache/artifacts");
        return;
    };

    let mut any_manifest = false;
    for entry in rd.flatten() {
        let tag_dir = entry.path();
        if !tag_dir.is_dir() {
            continue;
        }
        let tag_label = tag_dir.file_name().and_then(|s| s.to_str()).unwrap_or("?");
        if tag_dir.join("integrity.json").is_file() {
            any_manifest = true;
        }

        match artifact_cache::verify_cached_artifacts_offline(&tag_dir) {
            Ok(()) => {
                if tag_dir.join("integrity.json").is_file() {
                    println!(
                        "[OK]   cache artifacts/{} — integrity.json matches files on disk",
                        tag_label
                    );
                }
            }
            Err(e) => {
                println!("[WARN] cache [{}]: {}", tag_label, e);
                if fix {
                    if let Err(rem) = fs::remove_dir_all(&tag_dir) {
                        println!("[WARN] could not remove {}: {}", tag_dir.display(), rem);
                    } else {
                        println!(
                            "[FIX]  removed corrupted cache directory `{}`. Run `kern-portable upgrade` (or `kern-portable init`) to re-download.",
                            tag_label
                        );
                    }
                } else {
                    println!(
                        "       Hint: `kern-portable doctor --fix` removes bad cache dirs (no network)."
                    );
                }
            }
        }
    }
    if !any_manifest {
        println!("[WARN] cache/artifacts has no integrity.json yet (created after verified installs)");
    }
}
