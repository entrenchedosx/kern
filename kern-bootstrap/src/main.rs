mod detect;
mod doctor;
mod download;
mod env_paths;
mod error;
mod extract;
mod github;
mod install;
mod layout;
mod lock;
mod log;
mod nodejs;
mod platform;
mod progress;
mod repair;
mod state;
mod tree_copy;
mod uninstall;
mod versions_cmd;
mod path_inspect;
mod shell_hint;
#[cfg(windows)]
mod win_toolchain;

use crate::error::AppError;
use crate::install::{default_prefix, ExistingAction, InstallParams};
use crate::log::LogFormat;
use clap::{Parser, Subcommand};
use std::io::IsTerminal;
use std::path::PathBuf;

#[derive(Parser, Debug)]
#[command(
    name = "kern-bootstrap",
    version = env!("KERN_BOOTSTRAP_VER"),
    about = "Production bootstrapper for Kern + Kargo (GitHub Releases)"
)]
struct Cli {
    /// More detail on stderr; use twice (`-vv`) for JSON lines in `~/.kern/install.log` (same as `--log-json`).
    #[arg(short = 'v', long, global = true, action = clap::ArgAction::Count)]
    verbose: u8,

    #[arg(long, global = true)]
    no_color: bool,

    #[arg(long, global = true, default_value = "entrenchedosx/kern")]
    repo: String,

    #[arg(long, global = true, env = "GITHUB_TOKEN")]
    github_token: Option<String>,

    #[arg(
        long,
        global = true,
        env = "KERN_BOOTSTRAP_MIRRORS",
        value_delimiter = ',',
        num_args = 0..=20
    )]
    mirror: Vec<String>,

    /// Write structured JSON lines to ~/.kern/install.log (same effect as `-vv`).
    #[arg(long, global = true, default_value_t = false)]
    log_json: bool,

    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand, Debug)]
enum Commands {
    /// Install or refresh Kern + Kargo under the chosen prefix
    #[command(visible_alias = "INSTALL")]
    Install(InstallOpts),
    /// Same as install with latest release; skips interactive menu when state exists
    #[command(visible_alias = "UPGRADE")]
    Upgrade(InstallOpts),
    /// Remove managed binaries, versions, and optional PATH changes
    #[command(visible_alias = "UNINSTALL")]
    Uninstall(UninstallOpts),
    /// Report PATH, binaries, bootstrap state, and active version
    #[command(visible_alias = "DOCTOR")]
    Doctor(DoctorOpts),
    /// Recreate bin/ shims, fix active pointer from state, optional PATH
    #[command(visible_alias = "REPAIR")]
    Repair(RepairOpts),
    /// List installed release directories
    #[command(visible_alias = "LIST")]
    List(ListOpts),
    /// Switch active version (updates `current` / active-release.txt + bin/)
    #[command(visible_alias = "USE")]
    Use(UseOpts),
    /// Remove one installed version directory (not the active one)
    #[command(visible_alias = "REMOVE")]
    Remove(RemoveOpts),
    /// Show all `kern` / `kargo` matches on current PATH (active vs shadowed)
    #[command(visible_alias = "WHICH")]
    Which,
    /// Print PATH entries with indices (search order)
    #[command(visible_alias = "ENV")]
    Env,
}

#[derive(Parser, Debug)]
struct InstallOpts {
    #[arg(long)]
    version: Option<String>,

    #[arg(long)]
    prefix: Option<PathBuf>,

    #[arg(long, default_value_t = false)]
    system: bool,

    #[arg(long, default_value_t = false)]
    reinstall: bool,

    #[arg(long, default_value_t = true)]
    modify_path: bool,

    #[arg(short, long, default_value_t = false)]
    yes: bool,

    #[arg(long, default_value_t = false)]
    non_interactive: bool,

    /// Windows: open a new cmd/PowerShell with PATH fixed after install (skipped without a TTY)
    #[arg(long, default_value_t = false)]
    activate_here: bool,
}

#[derive(Parser, Debug)]
struct UninstallOpts {
    #[arg(long)]
    prefix: Option<PathBuf>,

    #[arg(long, default_value_t = false)]
    purge: bool,

    #[arg(long, default_value_t = true)]
    modify_path: bool,

    #[arg(short, long, default_value_t = false)]
    yes: bool,

    #[arg(long, default_value_t = false)]
    non_interactive: bool,
}

#[derive(Parser, Debug)]
struct DoctorOpts {
    #[arg(long)]
    prefix: Option<PathBuf>,

    #[arg(long, default_value_t = false)]
    fix: bool,

    /// Explain why Windows PATH / shells behave this way (prints before diagnostics)
    #[arg(long, default_value_t = false)]
    explain: bool,

    /// Windows: after doctor, open a new shell with managed bin on PATH (exit 0–1; TTY only)
    #[arg(long, default_value_t = false)]
    activate_here: bool,
}

#[derive(Parser, Debug)]
struct RepairOpts {
    #[arg(long)]
    prefix: Option<PathBuf>,

    #[arg(long, default_value_t = false)]
    fix_path: bool,
}

#[derive(Parser, Debug)]
struct ListOpts {
    #[arg(long)]
    prefix: Option<PathBuf>,
}

#[derive(Parser, Debug)]
struct UseOpts {
    #[arg(long)]
    prefix: Option<PathBuf>,

    /// Release tag directory name (e.g. v1.2.0)
    version: String,
}

#[derive(Parser, Debug)]
struct RemoveOpts {
    #[arg(long)]
    prefix: Option<PathBuf>,

    /// Release tag directory name to remove
    version: String,
}

fn verbose(cli: &Cli) -> bool {
    cli.verbose > 0
}

fn color(cli: &Cli) -> bool {
    !cli.no_color && std::io::stderr().is_terminal()
}

fn mirrors(cli: &Cli) -> Vec<String> {
    cli.mirror.clone()
}

fn log_format(cli: &Cli) -> LogFormat {
    if cli.log_json || cli.verbose >= 2 {
        LogFormat::Json
    } else {
        LogFormat::Human
    }
}

fn noninteractive(cli: &InstallOpts) -> bool {
    cli.non_interactive
        || cli.yes
        || std::env::var("KERN_BOOTSTRAP_NONINTERACTIVE")
            .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
            .unwrap_or(false)
        || std::env::var("CI")
            .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
            .unwrap_or(false)
}

fn release_spec(cmd: &InstallOpts) -> String {
    cmd.version
        .clone()
        .filter(|s| !s.is_empty())
        .unwrap_or_else(|| "latest".to_string())
}

fn lock_wait_prompt(ni: bool) -> bool {
    !ni && std::io::stdin().is_terminal()
}

fn main() {
    match run() {
        Ok(()) => {}
        Err(AppError::Cancelled) => {}
        Err(e) => {
            eprintln!("error: {}", e);
            std::process::exit(1);
        }
    }
}

fn run() -> Result<(), AppError> {
    let cli = Cli::parse();
    let verbose = verbose(&cli);
    let color = color(&cli);
    let mirrors = mirrors(&cli);
    let lf = log_format(&cli);

    match &cli.command {
        Commands::Install(opts) => {
            let prefix = opts
                .prefix
                .clone()
                .unwrap_or_else(|| default_prefix(opts.system));
            let existing = if opts.reinstall {
                Some(ExistingAction::Reinstall)
            } else {
                None
            };
            let ni = noninteractive(opts);
            install::run_install(InstallParams {
                repo: &cli.repo,
                release_spec: &release_spec(opts),
                prefix,
                token: cli.github_token.as_deref(),
                mirrors: &mirrors,
                modify_path: opts.modify_path,
                verbose,
                color,
                existing,
                non_interactive: ni,
                log_format: lf,
                lock_wait_prompt: lock_wait_prompt(ni),
                activate_here: opts.activate_here,
            })?;
        }
        Commands::Upgrade(opts) => {
            let prefix = opts
                .prefix
                .clone()
                .unwrap_or_else(|| default_prefix(opts.system));
            install::run_install(InstallParams {
                repo: &cli.repo,
                release_spec: &release_spec(opts),
                prefix,
                token: cli.github_token.as_deref(),
                mirrors: &mirrors,
                modify_path: opts.modify_path,
                verbose,
                color,
                existing: Some(ExistingAction::Upgrade),
                non_interactive: true,
                log_format: lf,
                lock_wait_prompt: false,
                activate_here: false,
            })?;
        }
        Commands::Uninstall(opts) => {
            let prefix = opts.prefix.clone().unwrap_or_else(detect::default_user_prefix);
            uninstall::run_uninstall(uninstall::UninstallParams {
                prefix,
                purge_home: opts.purge,
                modify_path: opts.modify_path,
                non_interactive: opts.non_interactive,
                yes: opts.yes,
                verbose,
                color,
                log_format: lf,
            })?;
        }
        Commands::Doctor(opts) => {
            let prefix = opts
                .prefix
                .clone()
                .unwrap_or_else(detect::default_user_prefix);
            #[cfg(windows)]
            let prefix_for_shell = prefix.clone();
            let code = doctor::run_doctor(doctor::DoctorParams {
                prefix,
                fix_path: opts.fix,
                explain: opts.explain,
                verbose,
                color,
            })?;
            #[cfg(windows)]
            if opts.activate_here {
                if !std::io::stdin().is_terminal() {
                    eprintln!("warning: --activate-here ignored (stdin is not a TTY)");
                } else if code <= 1 {
                    let bin = prefix_for_shell.join("bin");
                    match shell_hint::spawn_activated_shell_here(&bin) {
                        Ok(_) => eprintln!(
                            "Opened a new shell with managed bin on PATH. Exiting with status {}.",
                            code
                        ),
                        Err(e) => eprintln!("warning: --activate-here failed: {}", e),
                    }
                }
            }
            std::process::exit(code);
        }
        Commands::Which => {
            path_inspect::run_which(verbose, color)?;
        }
        Commands::Env => {
            path_inspect::run_env(verbose, color)?;
        }
        Commands::Repair(opts) => {
            let prefix = opts
                .prefix
                .clone()
                .unwrap_or_else(detect::default_user_prefix);
            repair::run_repair(repair::RepairParams {
                prefix,
                fix_path: opts.fix_path,
                verbose,
                color,
                log_format: lf,
            })?;
        }
        Commands::List(opts) => {
            let prefix = opts
                .prefix
                .clone()
                .unwrap_or_else(detect::default_user_prefix);
            versions_cmd::run_list(versions_cmd::VersionsParams {
                prefix,
                verbose,
                color,
            })?;
        }
        Commands::Use(opts) => {
            let prefix = opts
                .prefix
                .clone()
                .unwrap_or_else(detect::default_user_prefix);
            versions_cmd::run_use(
                versions_cmd::VersionsParams {
                    prefix,
                    verbose,
                    color,
                },
                &opts.version,
            )?;
        }
        Commands::Remove(opts) => {
            let prefix = opts
                .prefix
                .clone()
                .unwrap_or_else(detect::default_user_prefix);
            versions_cmd::run_remove(
                versions_cmd::VersionsParams {
                    prefix,
                    verbose,
                    color,
                },
                &opts.version,
            )?;
        }
    }
    Ok(())
}
