use crate::detect::{self, dedupe_paths, probe_kargo, run_version_line};
use crate::env_paths;
use crate::error::Result;
use crate::install::kern_executable_name;
use crate::layout::{
    active_version_dir, current_symlink_broken, kern_bundle_dir, kargo_bundle_dir,
    read_active_release_tag, version_home,
};
use crate::progress::Progress;
use crate::state::InstallState;
use std::path::PathBuf;

pub struct DoctorParams {
    pub prefix: PathBuf,
    pub fix_path: bool,
    pub explain: bool,
    pub verbose: bool,
    pub color: bool,
}

/// Exit: `0` healthy, `1` warnings only, `2` failures / strict verify failed.
pub fn run_doctor(p: DoctorParams) -> Result<i32> {
    let mut prog = Progress::new(p.color, p.verbose, 1);
    if p.explain {
        print_doctor_explain(&prog);
    }
    prog.step("Running diagnostics...");
    let mut fails = 0u32;
    let mut warns = 0u32;
    let mut stale_shell_external = false;

    let kern_raw = dedupe_paths(&detect::all_on_path("kern"));
    #[cfg(windows)]
    let kerns = detect::normalize_where_by_directory_prefer_cmd(kern_raw);
    #[cfg(not(windows))]
    let kerns = kern_raw;
    if kerns.is_empty() {
        prog.err("[FAIL] kern not found on PATH.");
        fails += 1;
    } else {
        prog.ok(&format!(
            "[OK]   {} `kern` path(s) on PATH (search order below)",
            kerns.len()
        ));
        for (i, path) in kerns.iter().enumerate() {
            let active = if i == 0 { "[ACTIVE]" } else { "[SHADOWED]" };
            let managed = if path.starts_with(&p.prefix) {
                "[MANAGED]"
            } else {
                "[EXTERNAL]"
            };
            match run_version_line(path) {
                Some(v) if !v.trim().is_empty() => {
                    prog.info(&format!(
                        "         {} {} {}",
                        active,
                        managed,
                        path.display()
                    ));
                    prog.info(&format!("              {}", v.trim()));
                }
                _ => {
                    prog.warn(&format!(
                        "[WARN] {} {} {} — `kern --version` failed.",
                        active,
                        managed,
                        path.display()
                    ));
                    warns += 1;
                }
            }
            if i == 0 && !path.starts_with(&p.prefix) {
                stale_shell_external = true;
                prog.err(&format!(
                    "[PATH] {} [EXTERNAL] The `kern` that runs first is NOT under managed prefix {} — you are not using the managed Kern from this install.\n         {}",
                    active,
                    p.prefix.display(),
                    path.display()
                ));
                prog.err(&format!(
                    "         Run `doctor --fix` if needed, then open a **new** terminal or run: call \"{}\"",
                    p.prefix.join("bin").join("kern-shell-init.cmd").display()
                ));
                warns += 1;
            }
        }
        if kerns.len() > 1 {
            warns += 1;
            prog.warn("[WARN] Multiple `kern` on PATH — only [ACTIVE] is used when you type `kern`.");
        }
    }

    #[cfg(windows)]
    {
        let hklm_kern = env_paths::hklm_path_segments_with_kern_launchers();
        if !hklm_kern.is_empty() {
            prog.always("");
            prog.always("[INFO] A system-wide PATH entry (HKLM) contains `kern`.");
            prog.always("         This installer does NOT modify system PATH (safety). It may still affect search order.");
            prog.always("         Keep managed user PATH first (handled on install / doctor --fix), or remove old system installs manually if unused:");
            for seg in &hklm_kern {
                prog.always(&format!("         {}", seg));
            }
        }
    }

    match probe_kargo() {
        Some(k) => {
            prog.ok(&format!("[OK]   kargo on PATH: {}", k.path.display()));
            prog.info(&format!("         version: {:?}", k.version_line));
        }
        None => {
            prog.warn("[WARN] kargo not found on PATH.");
            warns += 1;
            #[cfg(not(windows))]
            if !crate::nodejs::system_node_works() {
                prog.warn("[WARN] Node.js is not on PATH either — install Node 18+ for Kargo (macOS/Linux).");
                warns += 1;
            }
        }
    }

    let managed_bin = p.prefix.join("bin");
    #[cfg(windows)]
    let bin_in_user_env =
        env_paths::windows_registry_path_contains_bin_dir(&managed_bin);
    #[cfg(not(windows))]
    let bin_in_user_env = false;

    if env_paths::path_contains_bin() {
        prog.ok("[OK]   PATH includes a managed-style Kern bin directory.");
    } else if bin_in_user_env {
        prog.ok("[OK]   Install bin is in Windows user PATH (registry); this shell was started before that change.");
        prog.always("  Refresh PATH in this window — cmd.exe:");
        prog.always(&format!(
            r#"    set "PATH={};%PATH%""#,
            managed_bin.display()
        ));
        prog.always("  PowerShell:");
        prog.always(&format!(
            "    $env:Path = \"{};\" + $env:Path",
            managed_bin.display()
        ));
    } else {
        prog.warn("[WARN] Managed bin directory is not on PATH (run install with PATH updates, or `kern-bootstrap doctor --fix` on Unix).");
        warns += 1;
    }

    let broken_cur = current_symlink_broken(&p.prefix)?;
    let active_dir = active_version_dir(&p.prefix)?;
    if broken_cur {
        match &active_dir {
            Some(_) => {
                prog.warn(
                    "[WARN] `current` symlink is broken; active tree resolved via active-release.txt — run `repair`.",
                );
                warns += 1;
            }
            None => {
                prog.err("[FAIL] `current` symlink is broken and no active version directory found.");
                fails += 1;
            }
        }
    }
    match &active_dir {
        None if !broken_cur => {
            prog.err("[FAIL] No active version (missing `current` / active-release.txt or versions/<tag>).");
            fails += 1;
        }
        None => {}
        Some(d) => {
            prog.ok(&format!("[OK]   Active version dir: {}", d.display()));
            let kb = kern_bundle_dir(d);
            let kg = kargo_bundle_dir(d);
            let kern_path = kb.join(kern_executable_name());
            if !kern_path.is_file() {
                prog.err("[FAIL] Kern binary missing under active kern/ bundle.");
                fails += 1;
            } else {
                #[cfg(unix)]
                {
                    use std::os::unix::fs::PermissionsExt;
                    if let Ok(meta) = std::fs::metadata(&kern_path) {
                        if meta.permissions().mode() & 0o111 == 0 {
                            prog.warn("[WARN] Kern binary is not executable (chmod +x or run `repair`).");
                            warns += 1;
                        }
                    }
                }
            }
            if !kg.join("cli").join("entry.js").is_file() {
                prog.err("[FAIL] Kargo entry.js missing under active kargo/ bundle.");
                fails += 1;
            }
        }
    }

    match InstallState::load(&p.prefix)? {
        Some(st) => {
            prog.ok(&format!(
                "[OK]   State: release {} (kern semver {}), prefix {}",
                st.release_tag, st.kern_semver, st.prefix.display()
            ));
            let vhome = version_home(&p.prefix, &st.release_tag);
            if !vhome.is_dir() {
                prog.err(&format!(
                    "[FAIL] State references release {} but directory is missing (broken/partial install):\n         {}",
                    st.release_tag,
                    vhome.display()
                ));
                prog.always("         → `kern-bootstrap install` menu [6] Reinstall, or uninstall + fresh install.");
                fails += 1;
            }
            if st.schema_version < 2 {
                prog.warn("[WARN] Old state schema; reinstall or run `kern-bootstrap repair`.");
                warns += 1;
            }
            let tag_state = &st.release_tag;
            let tag_disk = read_active_release_tag(&p.prefix).ok().flatten();
            if tag_disk.as_ref() != Some(tag_state) {
                prog.warn(&format!(
                    "[WARN] Version mismatch: state.release_tag={} active pointer={:?}",
                    tag_state, tag_disk
                ));
                warns += 1;
            }
            if st.kargo_tag != st.release_tag {
                prog.warn(&format!(
                    "[WARN] state kargo_tag ({}) != release_tag ({}) — unusual; reinstall if unsure.",
                    st.kargo_tag, st.release_tag
                ));
                warns += 1;
            }
            let bin = st.bin_dir();
            let kern_bin = bin.join(kern_executable_name());
            let kern_cmd = bin.join("kern.cmd");
            if !kern_bin.is_file() && !kern_cmd.is_file() {
                prog.err("[FAIL] bin/ missing kern launcher.");
                fails += 1;
            }
            #[cfg(windows)]
            if let Some(ref nv) = st.embedded_node_version {
                prog.ok(&format!(
                    "[OK]   Bundled Node.js for Kargo: {} ({})",
                    nv,
                    crate::nodejs::embedded_node_exe(&p.prefix).display()
                ));
            }
        }
        None => {
            prog.info("[INFO] No bootstrap-state.json for this prefix.");
        }
    }

    check_dup_path_entries(&mut prog, &mut warns);

    if p.fix_path && (fails > 0 || warns > 0) {
        if InstallState::load(&p.prefix)?.is_none() {
            prog.warn("[WARN] No bootstrap state in prefix — run `kern-bootstrap install` first.");
        } else {
            crate::repair::run_repair(crate::repair::RepairParams {
                prefix: p.prefix.clone(),
                fix_path: true,
                verbose: p.verbose,
                color: p.color,
                log_format: crate::log::LogFormat::Human,
            })?;
            prog.ok("[OK]   Ran repair (refreshed bin/ shims + PATH).");
        }
    }

    #[cfg(windows)]
    let mut strict_ok = false;

    #[cfg(windows)]
    if let Some(st) = InstallState::load(&p.prefix)? {
        let exp = crate::win_toolchain::VerifyExpectations {
            kern_semver: &st.kern_semver,
            kargo_package_version: st.kargo_package_version.as_deref(),
        };
        match crate::win_toolchain::verify_toolchain_minimal_path(
            &st.bin_dir(),
            &st.prefix,
            exp,
            &prog,
            true,
        )
        {
            Ok(_) => {
                strict_ok = true;
                prog.ok("[OK]   Strict verify (minimal PATH): kern + kargo OK — on-disk install is healthy.");
            }
            Err(e) => {
                prog.err(&format!(
                    "[FAIL] Strict toolchain verify failed (install may be broken):\n{}",
                    e
                ));
                prog.err("         Conflicting `kern`/`kargo` on your interactive PATH do not affect this test.");
                fails += 1;
            }
        }
    }

    #[cfg(windows)]
    if stale_shell_external && strict_ok && fails == 0 {
        prog.always("");
        prog.err("Your shell is NOT using the managed Kern.");
        prog.always("The install is fine; this window’s PATH is stale. Fix it now with one command:");
        prog.always(&format!(
            "    call \"{}\"",
            p.prefix.join("bin").join("kern-shell-init.cmd").display()
        ));
        prog.always("Or:  kern-bootstrap doctor --fix --activate-here   (repair + new shell with PATH fixed)");
    }

    if fails > 0 || warns > 0 {
        prog.always("");
        prog.always("Suggested fixes:");
        if fails > 0 {
            prog.always("  → Run: kern-bootstrap repair (or install menu [6] Reinstall if versions/ is missing)");
        }
        if warns > 0 {
            prog.always("  → Run: kern-bootstrap doctor --fix   (PATH + bin/ shims)");
            prog.always("  → Run: kern-bootstrap repair        (shims + active pointer)");
            prog.always("  → Multiple/old `kern` on PATH: put managed …\\bin first, or remove stale Kern folders from PATH");
        }
    }

    if fails == 0 && warns == 0 {
        prog.ok("[OK]   No major issues detected.");
    } else {
        prog.warn(&format!(
            "Summary: {} failure(s), {} warning(s).",
            fails, warns
        ));
    }

    let code = if fails > 0 {
        2
    } else if warns > 0 {
        1
    } else {
        0
    };
    Ok(code)
}

fn print_doctor_explain(prog: &Progress) {
    prog.always("");
    prog.always("━━ kern-bootstrap: why PATH looks “wrong” on Windows ━━");
    prog.always("");
    prog.always("• Verification uses a **fresh subprocess** with PATH = managed bin + Windows system dirs.");
    prog.always("  That is **ground truth** for whether Kern + Kargo were installed correctly.");
    prog.always("");
    prog.always("• Your **current** cmd.exe / PowerShell keeps the PATH it inherited at startup.");
    prog.always("  A child program (this installer) **cannot** change the parent’s environment — Windows design.");
    prog.always("");
    prog.always("• HKCU Path is updated for **new** terminals; existing windows need either:");
    prog.always("    – open a new terminal, or");
    prog.always("    – `call …\\\\bin\\\\kern-shell-init.cmd` (cmd) / `& …\\\\bin\\\\kern-shell-init.ps1` (PowerShell).");
    prog.always("");
    prog.always("• HKLM (system) PATH is never modified here; entries there can still affect search order.");
    prog.always("");
}

fn check_dup_path_entries(prog: &mut Progress, warns: &mut u32) {
    let Ok(path_var) = std::env::var("PATH") else {
        return;
    };
    let mut seen = std::collections::HashSet::<String>::new();
    let mut dupes = 0u32;
    for part in std::env::split_paths(&path_var) {
        let s = part.to_string_lossy().to_string();
        if s.is_empty() {
            continue;
        }
        let key = s.to_lowercase();
        if !seen.insert(key) {
            dupes += 1;
        }
    }
    if dupes > 0 {
        prog.warn(&format!(
            "[WARN] PATH contains {} duplicate segment(s) (harmless but noisy).",
            dupes
        ));
        *warns += 1;
    }
}
