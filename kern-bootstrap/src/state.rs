use crate::error::{path_ctx, AppError, Result};
use serde::{Deserialize, Serialize};
use std::path::{Path, PathBuf};

pub const STATE_FILE: &str = "bootstrap-state.json";

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InstallState {
    /// Schema bump when on-disk layout changes (2 = versions/<tag>/kern|kargo + current).
    #[serde(default = "default_schema")]
    pub schema_version: u32,
    pub prefix: PathBuf,
    pub release_tag: String,
    pub kern_semver: String,
    pub kargo_tag: String,
    pub kargo_package_version: Option<String>,
    #[serde(default)]
    pub path_snippet_installed: bool,
    #[serde(default)]
    pub shell_config_path: Option<PathBuf>,
    /// Windows: Node.js version string when bundled under `prefix/tools/nodejs` for Kargo.
    #[serde(default)]
    pub embedded_node_version: Option<String>,
}

fn default_schema() -> u32 {
    1
}

impl InstallState {
    pub fn path_for_prefix(prefix: &Path) -> PathBuf {
        prefix.join(STATE_FILE)
    }

    pub fn load(prefix: &Path) -> Result<Option<Self>> {
        let p = Self::path_for_prefix(prefix);
        if !p.is_file() {
            return Ok(None);
        }
        let raw = std::fs::read_to_string(&p).map_err(|e| path_ctx(&p, e))?;
        let s: InstallState = serde_json::from_str(&raw).map_err(|e| {
            AppError::msg(format!(
                "corrupt state at {}: {}",
                p.display(),
                e
            ))
        })?;
        Ok(Some(s))
    }

    pub fn save(&self) -> Result<()> {
        let p = Self::path_for_prefix(&self.prefix);
        if let Some(parent) = p.parent() {
            std::fs::create_dir_all(parent).map_err(|e| path_ctx(&parent.to_path_buf(), e))?;
        }
        let tmp = p.with_extension("json.tmp");
        let raw = serde_json::to_string_pretty(self).map_err(|e| AppError::msg(e.to_string()))?;
        std::fs::write(&tmp, raw).map_err(|e| path_ctx(&tmp, e))?;
        std::fs::rename(&tmp, &p).map_err(|e| path_ctx(&p, e))?;
        Ok(())
    }

    pub fn bin_dir(&self) -> PathBuf {
        self.prefix.join("bin")
    }

    #[allow(dead_code)]
    pub fn versions_dir(&self) -> PathBuf {
        self.prefix.join("versions")
    }
}
