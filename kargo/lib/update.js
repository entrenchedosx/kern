import path from "node:path";
import { parseGithubSpec } from "./spec.js";
import { installFromSpec, installFromProjectManifest } from "./install.js";
import { kargoVerbose } from "./cli-env.js";
import { KargoCliError, EXIT } from "./cli-error.js";

export async function runUpdate(argv) {
  let project = process.cwd();
  let spec = null;
  let resolveDebug = false;
  let explain = false;
  let verbose = false;
  let dryRun = false;
  let refreshRemoteTags = false;
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--project" && i + 1 < argv.length) project = path.resolve(argv[++i]);
    else if (a === "--resolve-debug") resolveDebug = true;
    else if (a === "--explain") explain = true;
    else if (a === "--verbose" || a === "-v") verbose = true;
    else if (a === "--dry-run") dryRun = true;
    else if (a === "--refresh") refreshRemoteTags = true;
    else if (!spec) spec = a;
    else throw new KargoCliError(`Unknown argument: ${a}`, EXIT.USAGE);
  }

  verbose = verbose || kargoVerbose();

  if (spec) {
    await installFromSpec(spec, {
      projectDir: project,
      force: true,
      resolveDebug,
      explain,
      verbose,
      dryRun,
      refreshRemoteTags
    });
    if (dryRun) {
      console.log(`kargo: dry-run update complete for ${spec}`);
      return;
    }
    console.log(`kargo: updated ${spec}`);
    return;
  }

  const r = await installFromProjectManifest(project, {
    force: true,
    resolveDebug,
    explain,
    verbose,
    dryRun,
    refreshRemoteTags
  });
  if (r.count === 0) {
    console.log("kargo: no [dependencies] in kargo.toml");
    return;
  }
  if (dryRun) {
    console.log(`kargo: dry-run — would resolve ${r.count} package(s) from kargo.toml`);
    return;
  }
  console.log(`kargo: resolved ${r.count} package(s) from kargo.toml`);
}
