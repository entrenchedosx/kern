#![cfg(windows)]

//! Process-level checks for PATH shadowing. Run with:
//! `KERN_BOOTSTRAP_TEST_PREFIX=C:\path\to\install cargo test --test doctor_shadow_integration -- --ignored`
//!
//! CI exercises the same scenarios via PowerShell after a real `install`.

use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::Command;

fn bootstrap_exe() -> PathBuf {
    std::env::var_os("CARGO_BIN_EXE_kern-bootstrap")
        .map(PathBuf::from)
        .expect("CARGO_BIN_EXE_kern-bootstrap must be set by cargo test")
}

fn require_test_prefix() -> PathBuf {
    std::env::var_os("KERN_BOOTSTRAP_TEST_PREFIX")
        .map(PathBuf::from)
        .expect(
            "set KERN_BOOTSTRAP_TEST_PREFIX to an installed prefix (see module doc); use --ignored",
        )
}

/// PATH tail so `doctor` does not pick up unrelated `kern.exe` from the machine profile.
fn windows_path_tail() -> OsString {
    let windir = std::env::var_os("WINDIR").unwrap_or_else(|| OsString::from(r"C:\Windows"));
    let sys32: PathBuf = Path::new(&windir).join("System32");
    let mut o = OsString::new();
    o.push(sys32.as_os_str());
    o.push(";");
    o.push(&windir);
    o
}

fn find_kern_impl_under_versions(versions: &Path) -> Option<PathBuf> {
    let rd = std::fs::read_dir(versions).ok()?;
    for e in rd.flatten() {
        let p = e.path().join("kern").join("kern.exe");
        if p.is_file() {
            return Some(p);
        }
    }
    None
}

#[test]
#[ignore = "set KERN_BOOTSTRAP_TEST_PREFIX (installed prefix), then: cargo test ... -- --ignored"]
fn doctor_clean_managed_bin_first_exits_0() {
    let prefix = require_test_prefix();
    let bin = prefix.join("bin");
    let mut path = OsString::from(bin.as_os_str());
    path.push(";");
    path.push(windows_path_tail());

    let out = Command::new(bootstrap_exe())
        .args(["doctor", "--prefix"])
        .arg(&prefix)
        .env("PATH", path)
        .output()
        .expect("spawn kern-bootstrap doctor");

    let text = format!(
        "{}{}",
        String::from_utf8_lossy(&out.stdout),
        String::from_utf8_lossy(&out.stderr)
    );
    assert_eq!(
        out.status.code(),
        Some(0),
        "expected exit 0, got {:?}\n{text}",
        out.status.code()
    );
}

#[test]
#[ignore = "set KERN_BOOTSTRAP_TEST_PREFIX (installed prefix), then: cargo test ... -- --ignored"]
fn doctor_shadowed_external_kern_exits_1() {
    let prefix = require_test_prefix();
    let bin = prefix.join("bin");
    let versions = prefix.join("versions");
    let kern_impl = find_kern_impl_under_versions(&versions).expect("versions/*/kern/kern.exe");

    let fake = std::env::temp_dir().join("kern-bootstrap-fake-kern-shadow-test");
    let _ = std::fs::remove_dir_all(&fake);
    std::fs::create_dir_all(&fake).expect("mkdir fake kern dir");
    std::fs::copy(&kern_impl, fake.join("kern.exe")).expect("copy kern.exe to fake dir");

    let mut path = OsString::from(fake.as_os_str());
    path.push(";");
    path.push(bin.as_os_str());
    path.push(";");
    path.push(windows_path_tail());

    let out = Command::new(bootstrap_exe())
        .args(["doctor", "--prefix"])
        .arg(&prefix)
        .env("PATH", path)
        .output()
        .expect("spawn kern-bootstrap doctor");

    let text = format!(
        "{}{}",
        String::from_utf8_lossy(&out.stdout),
        String::from_utf8_lossy(&out.stderr)
    );

    assert_eq!(
        out.status.code(),
        Some(1),
        "expected exit 1, got {:?}\n{text}",
        out.status.code()
    );
    assert!(
        text.contains("[ACTIVE]") && text.contains("[EXTERNAL]"),
        "expected [ACTIVE] and [EXTERNAL] in output:\n{text}"
    );
    assert!(
        text.contains("NOT using the managed Kern"),
        "expected stale-shell / managed-Kern message:\n{text}"
    );
}
