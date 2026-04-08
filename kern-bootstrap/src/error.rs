use std::path::Path;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum AppError {
    #[error("network: {0}")]
    Network(String),
    #[error("io: {0}")]
    Io(#[from] std::io::Error),
    #[error("json: {0}")]
    Json(#[from] serde_json::Error),
    #[error("checksum mismatch for {path}: expected {expected}, got {got}")]
    Checksum {
        path: String,
        expected: String,
        got: String,
    },
    #[error("release asset not found: {0}")]
    AssetNotFound(String),
    #[error("unsupported platform for automatic install")]
    UnsupportedPlatform,
    #[error("user cancelled")]
    Cancelled,
    #[error("{0}")]
    Msg(String),
    #[error("extract: {0}")]
    Extract(String),
    #[error("another installation is in progress ({0}); wait or remove the lock file if stale")]
    LockHeld(String),
    #[error("version {0} is not installed under this prefix")]
    VersionNotFound(String),
}

pub type Result<T> = std::result::Result<T, AppError>;

impl AppError {
    pub fn msg(s: impl Into<String>) -> Self {
        AppError::Msg(s.into())
    }
}

/// Reserved for call sites that want `AppError` with IO context without `path_ctx`.
#[allow(dead_code)]
pub fn io_err(ctx: impl Into<String>, e: std::io::Error) -> AppError {
    AppError::Msg(format!("{}: {}", ctx.into(), e))
}

pub fn path_ctx(p: &Path, e: std::io::Error) -> AppError {
    AppError::Msg(format!("{}: {}", p.display(), e))
}
