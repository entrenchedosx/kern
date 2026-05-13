//! Version for User-Agent; optional alignment with monorepo `KERN_VERSION.txt` or crate version.
//! (UAC admin manifest is applied to the `kern-portable` binary in the main Kern repo workspace.)

use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));
    let kver_path = manifest_dir.join("KERN_VERSION.txt");
    println!("cargo:rerun-if-changed={}", kver_path.display());
    let kver_alt = manifest_dir.join("../KERN_VERSION.txt");
    println!("cargo:rerun-if-changed={}", kver_alt.display());

    let ver = std::fs::read_to_string(&kver_path)
        .ok()
        .or_else(|| std::fs::read_to_string(&kver_alt).ok())
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| env::var("CARGO_PKG_VERSION").expect("CARGO_PKG_VERSION"));

    println!("cargo:rustc-env=KERN_PORTABLE_CRATE_VER={ver}");
}
