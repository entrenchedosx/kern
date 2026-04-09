use std::path::PathBuf;
use thiserror::Error;

pub type Result<T> = std::result::Result<T, PortableError>;

#[derive(Error, Debug)]
pub enum PortableError {
    #[error("{0}")]
    Msg(String),
    #[error("IO error at {path}: {source}")]
    Io {
        path: PathBuf,
        source: std::io::Error,
    },
    #[error("network: {0}")]
    Network(String),
    #[error("extract: {0}")]
    Extract(String),
}

impl PortableError {
    pub fn msg(s: impl Into<String>) -> Self {
        PortableError::Msg(s.into())
    }
}

pub fn path_ctx(path: &std::path::Path, e: std::io::Error) -> PortableError {
    PortableError::Io {
        path: path.to_path_buf(),
        source: e,
    }
}
