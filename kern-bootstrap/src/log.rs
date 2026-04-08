use crate::error::{path_ctx, Result};
use chrono::Utc;
use serde::Serialize;
use std::io::Write;
use std::path::PathBuf;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum LogFormat {
    #[default]
    Human,
    Json,
}

#[derive(Serialize)]
struct JsonRecord<'a> {
    timestamp: String,
    level: &'a str,
    step: &'a str,
    message: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    meta: Option<serde_json::Value>,
}

pub fn log_path() -> PathBuf {
    dirs::home_dir()
        .unwrap_or_else(|| PathBuf::from("."))
        .join(".kern")
        .join("install.log")
}

/// Human-readable line (always safe for log file).
pub fn append_line(msg: &str) -> Result<()> {
    write_line(LogFormat::Human, "INFO", "general", msg, None)
}

pub fn write_line(
    format: LogFormat,
    level: &str,
    step: &str,
    message: &str,
    meta: Option<serde_json::Value>,
) -> Result<()> {
    let p = log_path();
    if let Some(parent) = p.parent() {
        std::fs::create_dir_all(parent).map_err(|e| path_ctx(&parent.to_path_buf(), e))?;
    }
    let mut f = std::fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open(&p)
        .map_err(|e| path_ctx(&p, e))?;

    match format {
        LogFormat::Human => {
            writeln!(
                f,
                "[{}] [{}] [{}] {}",
                Utc::now().to_rfc3339(),
                level,
                step,
                message
            )
            .map_err(|e| path_ctx(&p, e))?;
        }
        LogFormat::Json => {
            let rec = JsonRecord {
                timestamp: Utc::now().to_rfc3339(),
                level,
                step,
                message: message.to_string(),
                meta,
            };
            let line = serde_json::to_string(&rec).map_err(|e| {
                crate::error::AppError::msg(e.to_string())
            })?;
            writeln!(f, "{}", line).map_err(|e| path_ctx(&p, e))?;
        }
    }
    Ok(())
}
