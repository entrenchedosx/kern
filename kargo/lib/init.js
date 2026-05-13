import { existsSync, mkdirSync, writeFileSync } from "fs";
import { basename, resolve } from "path";
import { cwd } from "process";
import { KargoCliError, EXIT, kargoIoError } from "./cli-error.js";

/**
 * kargo init [--project <dir>]
 * Writes kargo.toml and ensures .kern/ exists.
 */
export async function runInit(argv) {
  let dir = cwd();
  let i = 0;
  while (i < argv.length) {
    if (argv[i] === "--project" && argv[i + 1]) {
      dir = resolve(argv[i + 1]);
      i += 2;
      continue;
    }
    if (argv[i] === "--help" || argv[i] === "-h") {
      console.log(`kargo init [--project <dir>]

Creates kargo.toml (GitHub-first Kern package) and .kern/ if missing.
`);
      return;
    }
    throw new KargoCliError(`init: unknown argument: ${argv[i]}`, EXIT.USAGE);
  }

  const tomlPath = resolve(dir, "kargo.toml");
  if (existsSync(tomlPath)) {
    throw new KargoCliError(`init: ${tomlPath} already exists`, EXIT.USER);
  }

  let name = basename(dir).replace(/[^a-zA-Z0-9_.-]/g, "-");
  if (!name || name === "." || name === "..") name = "my-kern-pkg";

  const body = `# GitHub-first Kern package — add deps under [dependencies], then: kargo install owner/repo@^1.0.0
name = "${name}"
version = "0.1.0"

[kargo]
resolution_mode = "latest"
allow_prerelease = true

[dependencies]
`;

  try {
    writeFileSync(tomlPath, body, "utf8");
    const kernDir = resolve(dir, ".kern");
    if (!existsSync(kernDir)) mkdirSync(kernDir, { recursive: true });
  } catch (e) {
    throw kargoIoError(`kargo init: could not write project files under ${dir}`, e);
  }
  console.log(`kargo init: wrote ${tomlPath}`);
}
