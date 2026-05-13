//! CLI entry: `kern-portable init` or delegate to `kern-*/kern.exe`.

use std::path::PathBuf;

fn main() {
    if let Err(e) = run() {
        eprintln!("error: {}", e);
        std::process::exit(1);
    }
}

struct InitCli {
    force: bool,
    project: Option<PathBuf>,
    release_spec: Option<String>,
}

impl Default for InitCli {
    fn default() -> Self {
        Self {
            force: false,
            project: None,
            release_spec: None,
        }
    }
}

fn parse_init_like_args(
    args: &[String],
    start: usize,
    repo: &mut String,
) -> kern_portable_bootstrap::Result<InitCli> {
    let mut out = InitCli::default();
    let mut i = start;
    while i < args.len() {
        match args[i].as_str() {
            "--repo" => {
                i += 1;
                if i >= args.len() {
                    return Err(kern_portable_bootstrap::PortableError::msg("--repo needs a value"));
                }
                *repo = args[i].clone();
                i += 1;
            }
            "--force" | "-f" => {
                out.force = true;
                i += 1;
            }
            "--project" => {
                i += 1;
                if i >= args.len() {
                    return Err(kern_portable_bootstrap::PortableError::msg(
                        "--project needs a path",
                    ));
                }
                out.project = Some(PathBuf::from(&args[i]));
                i += 1;
            }
            "--release" => {
                i += 1;
                if i >= args.len() {
                    return Err(kern_portable_bootstrap::PortableError::msg(
                        "--release needs a value",
                    ));
                }
                out.release_spec = Some(args[i].clone());
                i += 1;
            }
            "--latest" => {
                out.release_spec = Some("latest".to_string());
                i += 1;
            }
            "--nightly" => {
                out.release_spec = Some("nightly".to_string());
                i += 1;
            }
            "--version" => {
                i += 1;
                if i >= args.len() {
                    return Err(kern_portable_bootstrap::PortableError::msg(
                        "--version needs a value (e.g. 1.2.3)",
                    ));
                }
                out.release_spec = Some(args[i].clone());
                i += 1;
            }
            _ => {
                return Err(kern_portable_bootstrap::PortableError::msg(format!(
                    "unknown argument: {}",
                    args[i]
                )));
            }
        }
    }
    Ok(out)
}

fn run() -> kern_portable_bootstrap::Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        print_help();
        return Ok(());
    }

    let mut repo = std::env::var("KERN_PORTABLE_REPO")
        .unwrap_or_else(|_| "entrenchedosx/kern-installer-src".to_string());
    let default_rel =
        std::env::var("KERN_PORTABLE_RELEASE").unwrap_or_else(|_| "latest".to_string());
    let token = std::env::var("GITHUB_TOKEN").ok();

    if args[1] == "init" {
        let init = parse_init_like_args(&args, 2, &mut repo)?;
        let release = init
            .release_spec
            .unwrap_or_else(|| default_rel.clone());
        let root = init
            .project
            .unwrap_or_else(|| std::env::current_dir().expect("cwd"));
        kern_portable_bootstrap::install::run_init(
            &root,
            &release,
            &repo,
            token.as_deref(),
            init.force,
        )?;
        return Ok(());
    }

    if args[1] == "upgrade" {
        let init = parse_init_like_args(&args, 2, &mut repo)?;
        let release = init
            .release_spec
            .unwrap_or_else(|| default_rel.clone());
        let root = init
            .project
            .unwrap_or_else(|| std::env::current_dir().expect("cwd"));
        kern_portable_bootstrap::install::run_upgrade(
            &root,
            &release,
            &repo,
            token.as_deref(),
        )?;
        return Ok(());
    }

    if args[1] == "doctor" {
        let mut project: Option<PathBuf> = None;
        let mut fix = false;
        let mut i = 2;
        while i < args.len() {
            match args[i].as_str() {
                "--project" => {
                    i += 1;
                    if i >= args.len() {
                        return Err(kern_portable_bootstrap::PortableError::msg(
                            "--project needs a path",
                        ));
                    }
                    project = Some(PathBuf::from(&args[i]));
                    i += 1;
                }
                "--fix" => {
                    fix = true;
                    i += 1;
                }
                "--repo" => {
                    i += 1;
                    if i >= args.len() {
                        return Err(kern_portable_bootstrap::PortableError::msg(
                            "--repo needs a value",
                        ));
                    }
                    i += 1;
                }
                _ => {
                    return Err(kern_portable_bootstrap::PortableError::msg(format!(
                        "unknown argument: {}",
                        args[i]
                    )));
                }
            }
        }
        let root = project.unwrap_or_else(|| std::env::current_dir().expect("cwd"));
        kern_portable_bootstrap::doctor::run_doctor(&root, fix)?;
        return Ok(());
    }

    let forwarded: Vec<String> = args.into_iter().skip(1).collect();
    kern_portable_bootstrap::delegate_to_local_kern(&forwarded);
}

fn print_help() {
    eprintln!(
        "kern-portable — project-local Kern environment (Windows)\n\n\
         kern-portable init [--force] [--project DIR] [--repo O/R] [--release SPEC]\n\
           [--latest] [--nightly] [--version X.Y.Z]\n\
         kern-portable upgrade [--project DIR] [--repo O/R] [--release SPEC] ...\n\
         kern-portable doctor [--project DIR] [--fix]\n\
         kern-portable <args>...   # runs kern-*/kern.exe with same args (sets KERN_HOME)\n\n\
         After init:  . .\\kern-NN\\Scripts\\Activate.ps1   (PowerShell; NN = folder created)\n\
                 or:   kern-NN\\Scripts\\activate.bat       (cmd)\n\
         Sets KERN_HOME and PATH.  kern-deactivate / deactivate-kern.cmd to undo.\n\n\
         Default repo: entrenchedosx/kern-installer-src (falls back to entrenchedosx/kern).\n\
         Env: KERN_PORTABLE_REPO, KERN_PORTABLE_RELEASE, GITHUB_TOKEN, KERN_HOME"
    );
}
