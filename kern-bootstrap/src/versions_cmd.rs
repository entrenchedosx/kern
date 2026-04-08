//! `list` / `use` / `remove` — rustup-style version switching.

use crate::error::{path_ctx, AppError, Result};
use crate::layout::{self, list_installed_tags, read_active_release_tag, version_home};
use crate::repair;
use crate::progress::Progress;
use crate::state::InstallState;
use std::fs;
use std::io::BufRead;
use std::path::{Path, PathBuf};
use std::process::Command;

pub struct VersionsParams {
    pub prefix: PathBuf,
    pub verbose: bool,
    pub color: bool,
}

pub fn run_list(p: VersionsParams) -> Result<()> {
    let mut prog = Progress::new(p.color, p.verbose, 1);
    prog.step("Installed versions");
    let tags = list_installed_tags(&p.prefix)?;
    let active = read_active_release_tag(&p.prefix).ok().flatten();
    if tags.is_empty() {
        prog.warn("No version directories under versions/");
        return Ok(());
    }
    for t in &tags {
        let mark = active.as_ref().map(|a| a == t).unwrap_or(false);
        if mark {
            prog.ok(&format!("  * {}  (active)", t));
        } else {
            prog.always(&format!("    {}", t));
        }
    }
    Ok(())
}

pub fn run_use(p: VersionsParams, tag: &str) -> Result<()> {
    let tag = resolve_version_dir_name(&p.prefix, tag)?;
    let vdir = version_home(&p.prefix, &tag);
    if !vdir.is_dir() {
        return Err(AppError::VersionNotFound(tag));
    }
    let mut prog = Progress::new(p.color, p.verbose, 2);
    prog.step("Switching active version...");
    let prev = layout::set_active_version(&p.prefix, &tag)?;
    let res = repair::refresh_bin(&p.prefix, &mut prog);
    if res.is_err() {
        let _ = layout::rollback_active_version(&p.prefix, prev.as_deref());
        return res;
    }
    update_state_after_switch(&p.prefix, &tag, &mut prog)?;
    prog.ok(&format!("Now using {}", tag));
    Ok(())
}

pub fn run_remove(p: VersionsParams, tag: &str) -> Result<()> {
    let tag = resolve_version_dir_name(&p.prefix, tag)?;
    let active = read_active_release_tag(&p.prefix).ok().flatten();
    if active.as_ref() == Some(&tag) {
        return Err(AppError::msg(format!(
            "refusing to remove active version {} — `kern-bootstrap use <other>` first",
            tag
        )));
    }
    let vdir = version_home(&p.prefix, &tag);
    if !vdir.is_dir() {
        return Err(AppError::VersionNotFound(tag));
    }
    let prog = Progress::new(p.color, p.verbose, 1);
    fs::remove_dir_all(&vdir).map_err(|e| path_ctx(&vdir, e))?;
    prog.ok(&format!("Removed {}", vdir.display()));
    Ok(())
}

pub fn run_clean_old(p: VersionsParams) -> Result<()> {
    let prog = Progress::new(p.color, p.verbose, 1);
    let tags = list_installed_tags(&p.prefix)?;
    let active = read_active_release_tag(&p.prefix)?.ok_or_else(|| {
        AppError::msg("no active version — set one with `kern-bootstrap use <tag>` before cleaning")
    })?;
    for t in tags {
        if t == active {
            continue;
        }
        let vdir = version_home(&p.prefix, &t);
        fs::remove_dir_all(&vdir).map_err(|e| path_ctx(&vdir, e))?;
        prog.info(&format!("removed {}", t));
    }
    prog.ok("Clean complete (kept active version).");
    Ok(())
}

pub fn interactive_pick_version(prefix: &Path) -> Result<Option<String>> {
    let tags = list_installed_tags(prefix)?;
    let active = read_active_release_tag(prefix).ok().flatten();
    if tags.is_empty() {
        return Ok(None);
    }
    eprintln!("Installed:");
    for (i, t) in tags.iter().enumerate() {
        let m = active.as_ref().map(|a| a == t).unwrap_or(false);
        eprintln!(
            "  [{}] {}{}",
            i + 1,
            t,
            if m { " (active)" } else { "" }
        );
    }
    eprint!("Enter number: ");
    let mut line = String::new();
    std::io::stdin()
        .lock()
        .read_line(&mut line)
        .map_err(|e| AppError::msg(e.to_string()))?;
    let n: usize = line.trim().parse().map_err(|_| {
        AppError::msg("invalid selection")
    })?;
    if n == 0 || n > tags.len() {
        return Ok(None);
    }
    Ok(Some(tags[n - 1].clone()))
}

fn resolve_version_dir_name(prefix: &Path, tag: &str) -> Result<String> {
    let t = tag.trim();
    let mut candidates = vec![t.to_string()];
    if t.starts_with('v') {
        candidates.push(t.trim_start_matches('v').to_string());
    } else {
        candidates.push(format!("v{}", t));
    }
    for c in candidates {
        if version_home(prefix, &c).is_dir() {
            return Ok(c);
        }
    }
    Err(AppError::VersionNotFound(t.to_string()))
}

fn update_state_after_switch(prefix: &Path, tag: &str, prog: &mut Progress) -> Result<()> {
    let Some(mut st) = InstallState::load(prefix)? else {
        return Ok(());
    };
    st.release_tag = tag.to_string();
    st.kargo_tag = tag.to_string();
    st.schema_version = 2;
    let bin = prefix.join("bin").join(crate::platform::kern_executable_name());
    if let Ok(out) = Command::new(&bin).arg("--version").output() {
        if out.status.success() {
            let line = String::from_utf8_lossy(&out.stdout).trim().to_string();
            if !line.is_empty() {
                st.kern_semver = line.clone();
            }
            prog.info(&format!("kern reports: {}", line));
        }
    }
    st.save()?;
    Ok(())
}
