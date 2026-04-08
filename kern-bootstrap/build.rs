//! `kern-bootstrap --version` tracks repo `KERN_VERSION.txt` (same as `kern --version`).

use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));
    let kver_path = manifest_dir.join("..").join("KERN_VERSION.txt");
    println!("cargo:rerun-if-changed={}", kver_path.display());

    let ver = std::fs::read_to_string(&kver_path)
        .ok()
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| env::var("CARGO_PKG_VERSION").expect("CARGO_PKG_VERSION"));

    let cargo_ver = env::var("CARGO_PKG_VERSION").expect("CARGO_PKG_VERSION");
    if ver != cargo_ver {
        panic!(
            "kern-bootstrap: KERN_VERSION.txt ({ver}) must match Cargo.toml version ({cargo_ver})"
        );
    }

    println!("cargo:rustc-env=KERN_BOOTSTRAP_VER={ver}");
}
