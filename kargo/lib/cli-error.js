/**
 * Stable kargo CLI exit codes (CI / scripts).
 *
 * | Code | Meaning |
 * |------|---------|
 * | 0 | Success |
 * | 1 (USER) | Fixable user/environment issues: bad spec, missing kargo.toml, resolution unsat, git/kern/network, lock drift |
 * | 2 (USAGE) | Wrong invocation: unknown command/flag, missing required args |
 * | 3 (INTERNAL) | Should not happen: resolver invariant, missing lock entry after successful install |
 */
export const EXIT = {
  OK: 0,
  USER: 1,
  USAGE: 2,
  INTERNAL: 3
};

/** Stable ids for logs / CI (prefix kargo stderr lines). */
export const ERR = {
  INVALID_CONFIG_JSON: "KARGO_INVALID_CONFIG_JSON",
  INVALID_LOCK_JSON: "KARGO_INVALID_LOCK_JSON",
  INVALID_PACKAGE_PATHS_JSON: "KARGO_INVALID_PACKAGE_PATHS_JSON",
  IO_ERROR: "KARGO_IO_ERROR"
};

/** Default fix hints keyed by errorCode (multiline OK). */
export const ERR_SUGGESTIONS = {
  [ERR.INVALID_CONFIG_JSON]:
    "Fix JSON syntax (commas/brackets/quotes), or delete ~/.kargo/config.json to start fresh.",
  [ERR.INVALID_LOCK_JSON]:
    "Fix JSON in kargo.lock, or remove it and run kargo install / kargo update to regenerate.",
  [ERR.INVALID_PACKAGE_PATHS_JSON]:
    "Fix JSON in .kern/package-paths.json, or remove it and run kargo install / kargo update to regenerate.",
  [ERR.IO_ERROR]: "Check file permissions, disk space, and that paths are writable."
};

const USAGE_SUGGESTION = "Run kargo --help for commands and examples.";

export class KargoCliError extends Error {
  /**
   * @param {string} message
   * @param {number} [exitCode=EXIT.USER]
   * @param {string} [errorCode=""] stable id from ERR.* when set
   * @param {string} [customSuggestion=""] overrides ERR_SUGGESTIONS[errorCode] when non-empty
   */
  constructor(message, exitCode = EXIT.USER, errorCode = "", customSuggestion = "") {
    super(message);
    this.name = "KargoCliError";
    this.exitCode = exitCode;
    this.errorCode = errorCode || "";
    this.customSuggestion = customSuggestion || "";
  }
}

/** One-line stderr body after `kargo: ` prefix. */
export function formatKargoMessage(e) {
  const msg = e && typeof e.message === "string" ? e.message : String(e);
  if (e instanceof KargoCliError && e.errorCode) return `[${e.errorCode}] ${msg}`;
  return msg;
}

/** Suggestion text for stderr (empty if none). */
export function suggestionForError(e) {
  if (!(e instanceof KargoCliError)) return "";
  if (e.customSuggestion) return e.customSuggestion;
  if (e.errorCode && Object.prototype.hasOwnProperty.call(ERR_SUGGESTIONS, e.errorCode)) {
    return ERR_SUGGESTIONS[e.errorCode];
  }
  if (e.exitCode === EXIT.USAGE) return USAGE_SUGGESTION;
  return "";
}

/**
 * Machine-readable error (single JSON line on stderr with --json-errors).
 * @param {KargoCliError} e
 */
export function kargoErrorToJson(e) {
  const code = e.errorCode || "KARGO_ERROR";
  return {
    error: {
      code,
      message: e.message,
      suggestion: suggestionForError(e),
      exit: e.exitCode
    }
  };
}

/**
 * Turn fs / IO failures into KargoCliError(USER). Preserves existing KargoCliError.
 * @param {string} context Human-readable prefix (no trailing colon)
 * @param {unknown} err
 */
export function kargoIoError(context, err) {
  if (err instanceof KargoCliError) return err;
  const code =
    err && typeof err === "object" && err !== null && "code" in err ? String(err.code) : "";
  const msg = err && typeof err === "object" && err !== null && "message" in err ? String(err.message) : String(err);
  const suffix = code ? ` (${code})` : "";
  return new KargoCliError(`${context}: ${msg}${suffix}`, EXIT.USER, ERR.IO_ERROR);
}
