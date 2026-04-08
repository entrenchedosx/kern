//! Best-effort parent shell detection for post-install UX (Windows).

use crate::progress::Progress;
use std::path::Path;

#[cfg(windows)]
use std::process::Command;

fn ps_single_quoted_literal(s: &str) -> String {
    format!("'{}'", s.replace('\'', "''"))
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ShellFlavor {
    Cmd,
    PowerShell,
    WindowsTerminal,
}

/// Infer shell from environment (child inherits parent’s vars).
pub fn detect_shell_flavor() -> ShellFlavor {
    if std::env::var("WT_SESSION").is_ok() {
        return ShellFlavor::WindowsTerminal;
    }
    if std::env::var("PSModulePath").is_ok() {
        return ShellFlavor::PowerShell;
    }
    ShellFlavor::Cmd
}

/// Print exact one-liners to fix PATH in the **current** window.
pub fn print_windows_session_path_hint(bin_dir: &Path, prog: &Progress) {
    let flavor = detect_shell_flavor();
    let cmd_init = bin_dir.join("kern-shell-init.cmd");
    let ps1_init = bin_dir.join("kern-shell-init.ps1");

    prog.always("");
    prog.ok("Install is correct. This window is outdated (stale PATH).");
    prog.always("A child installer cannot rewrite the parent terminal; new windows read HKCU Path.");
    prog.always("");
    prog.always("Refresh **this** window — pick one:");
    prog.always("");
    match flavor {
        ShellFlavor::PowerShell | ShellFlavor::WindowsTerminal => {
            prog.always("Detected: PowerShell / Windows Terminal — prefer:");
            prog.always(&format!(r#"  & "{}""#, ps1_init.display()));
            prog.always("Or:");
            prog.always(&format!(r#"  call "{}""#, cmd_init.display()));
        }
        ShellFlavor::Cmd => {
            prog.always("Detected: cmd.exe — run:");
            prog.always(&format!(r#"  call "{}""#, cmd_init.display()));
            prog.always("Or from PowerShell:");
            prog.always(&format!(r#"  & "{}""#, ps1_init.display()));
        }
    }
    prog.always("");
    prog.always("Manual one-liners:");
    prog.always(&format!(
        r#"  cmd.exe:     set "PATH={};%PATH%""#,
        bin_dir.display()
    ));
    prog.always(&format!(
        "  PowerShell:  $env:Path = \"{};\" + $env:Path",
        bin_dir.display()
    ));
    prog.always("");
    prog.always("Or use:  kern-bootstrap install --activate-here   (same after install)");
}

/// Spawn a **new** interactive shell with managed `bin` prepended (parent process exits separately).
#[cfg(windows)]
pub fn spawn_activated_shell_here(bin_dir: &Path) -> std::io::Result<()> {
    let cwd = std::env::current_dir().unwrap_or_else(|_| bin_dir.to_path_buf());
    let cmd_init = bin_dir.join("kern-shell-init.cmd");
    let ps1_init = bin_dir.join("kern-shell-init.ps1");
    match detect_shell_flavor() {
        ShellFlavor::Cmd => {
            let inner = format!(
                "cd /d \"{}\" & call \"{}\"",
                cwd.display(),
                cmd_init.display()
            );
            Command::new("cmd.exe").arg("/k").arg(inner).spawn()?;
        }
        ShellFlavor::PowerShell | ShellFlavor::WindowsTerminal => {
            let cmd = format!(
                "Set-Location -LiteralPath {}; & {}",
                ps_single_quoted_literal(&cwd.to_string_lossy()),
                ps_single_quoted_literal(&ps1_init.to_string_lossy())
            );
            Command::new("powershell.exe")
                .arg("-NoExit")
                .arg("-Command")
                .arg(cmd)
                .spawn()?;
        }
    }
    Ok(())
}

#[cfg(not(windows))]
pub fn spawn_activated_shell_here(_bin_dir: &Path) -> std::io::Result<()> {
    Ok(())
}
