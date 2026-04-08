use crate::env_paths::{has_marker_block, remove_marker_block};
use crate::error::{path_ctx, Result};
use crate::layout::{self, lock_path};
use crate::log::{self, LogFormat};
use crate::progress::Progress;
use crate::state::InstallState;
use std::io::BufRead;
use std::path::PathBuf;

pub struct UninstallParams {
    pub prefix: PathBuf,
    pub purge_home: bool,
    pub modify_path: bool,
    pub non_interactive: bool,
    pub yes: bool,
    pub verbose: bool,
    pub color: bool,
    pub log_format: LogFormat,
}

pub fn run_uninstall(p: UninstallParams) -> Result<()> {
    let mut prog = Progress::new(p.color, p.verbose, 3);
    let _ = log::append_line(&format!("uninstall prefix={}", p.prefix.display()));

    prog.step("Checking managed install...");
    let state = InstallState::load(&p.prefix)?;
    if state.is_none() && !p.prefix.join("bin").is_dir() {
        prog.warn("No bootstrap state and no bin/ under prefix; nothing to remove.");
        return Ok(());
    }

    if !p.non_interactive && !p.yes {
        prog.always(&format!(
            "This will remove Kern/Kargo from:\n  {}",
            p.prefix.display()
        ));
        prog.always("Type 'yes' to continue: ");
        let mut line = String::new();
        std::io::stdin()
            .lock()
            .read_line(&mut line)
            .map_err(|e| crate::error::AppError::msg(e.to_string()))?;
        if line.trim() != "yes" {
            prog.warn("Aborted.");
            return Ok(());
        }
    }

    prog.step("Removing files...");
    let _ = layout::remove_active_pointer(&p.prefix);

    if let Some(ref st) = state {
        if p.modify_path {
            if let Some(ref cfg) = st.shell_config_path {
                if cfg.is_file() {
                    let mut raw = String::new();
                    std::fs::File::open(cfg)
                        .and_then(|mut f| std::io::Read::read_to_string(&mut f, &mut raw))
                        .map_err(|e| path_ctx(&cfg.to_path_buf(), e))?;
                    if has_marker_block(&raw) {
                        let new_content = remove_marker_block(&raw);
                        std::fs::write(cfg, new_content)
                            .map_err(|e| path_ctx(&cfg.to_path_buf(), e))?;
                        prog.ok(&format!("Removed PATH snippet from {}", cfg.display()));
                    }
                }
            }
            #[cfg(windows)]
            {
                let _ = remove_windows_path_entry(&st.bin_dir());
            }
        }
    }

    let state_file = InstallState::path_for_prefix(&p.prefix);
    let targets = [p.prefix.join("versions"), p.prefix.join("bin"), state_file];
    for t in &targets {
        if t.exists() {
            if t.is_dir() {
                std::fs::remove_dir_all(t).map_err(|e| path_ctx(t, e))?;
            } else {
                std::fs::remove_file(t).map_err(|e| path_ctx(t, e))?;
            }
            prog.info(&format!("removed {}", t.display()));
        }
    }

    let dl = p.prefix.join("downloads");
    if dl.is_dir() {
        std::fs::remove_dir_all(&dl).map_err(|e| path_ctx(&dl, e))?;
    }

    let tools = p.prefix.join("tools");
    if tools.is_dir() {
        std::fs::remove_dir_all(&tools).map_err(|e| path_ctx(&tools, e))?;
        prog.info(&format!("removed {}", tools.display()));
    }

    if let Ok(rd) = std::fs::read_dir(&p.prefix) {
        for e in rd.flatten() {
            let path = e.path();
            let name = e.file_name();
            let s = name.to_string_lossy();
            if (s.starts_with(".staging-") || s.starts_with(".node-unpack-")) && path.is_dir() {
                std::fs::remove_dir_all(&path).map_err(|err| path_ctx(&path, err))?;
                prog.info(&format!("removed stale {}", path.display()));
            }
        }
    }
    let lk = lock_path(&p.prefix);
    if lk.is_file() {
        let _ = std::fs::remove_file(&lk);
    }

    prog.step("Done.");
    prog.ok("Uninstall complete.");
    let _ = log::write_line(p.log_format, "INFO", "uninstall", "uninstall ok", None);

    if p.purge_home {
        let home_kern = dirs::home_dir()
            .unwrap_or_else(|| PathBuf::from("."))
            .join(".kern");
        if home_kern.is_dir() && p.verbose {
            prog.info(&format!("purge: removing {}", home_kern.display()));
        }
        if home_kern.is_dir() {
            std::fs::remove_dir_all(&home_kern).map_err(|e| path_ctx(&home_kern, e))?;
        }
    }
    Ok(())
}

#[cfg(windows)]
fn remove_windows_path_entry(bin_dir: &std::path::Path) -> Result<()> {
    use winreg::enums::*;
    use winreg::RegKey;
    let hcu = RegKey::predef(HKEY_CURRENT_USER);
    let env = hcu.open_subkey("Environment").map_err(|_| {
        crate::error::AppError::msg("could not open HKCU\\Environment")
    })?;
    let path: String = env.get_value("Path").unwrap_or_default();
    let needle = bin_dir.display().to_string();
    let parts: Vec<_> = path
        .split(';')
        .filter(|s| !s.is_empty() && !s.eq_ignore_ascii_case(&needle))
        .collect();
    let new_path = parts.join(";");
    let (env, _) = hcu
        .create_subkey("Environment")
        .map_err(|e| crate::error::AppError::msg(e.to_string()))?;
    env.set_value("Path", &new_path)
        .map_err(|e| crate::error::AppError::msg(e.to_string()))?;
    Ok(())
}
