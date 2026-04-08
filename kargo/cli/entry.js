#!/usr/bin/env node
import { readFileSync } from "fs";
import { dirname, join } from "path";
import { fileURLToPath } from "url";
import { runInstall } from "../lib/install.js";
import { runRemove } from "../lib/remove.js";
import { runUpdate } from "../lib/update.js";
import { runList } from "../lib/list.js";
import { runSearch } from "../lib/search.js";
import { runPublish } from "../lib/publish.js";
import { runLogin } from "../lib/login.js";
import { runBuild, runRun } from "../lib/build.js";
import { runGraph } from "../lib/graph.js";
import { runInit } from "../lib/init.js";
import { kargoVerbose } from "../lib/cli-env.js";
import {
  KargoCliError,
  EXIT,
  formatKargoMessage,
  suggestionForError,
  kargoErrorToJson
} from "../lib/cli-error.js";

const __dirname = dirname(fileURLToPath(import.meta.url));

/** Leading only: `kargo -v --json-errors install …` */
function stripLeadingGlobalFlags(argv) {
  let i = 0;
  let verboseCount = 0;
  let jsonErrors = false;
  while (i < argv.length) {
    const a = argv[i];
    if (a === "--verbose" || a === "-v") {
      verboseCount += 1;
      i += 1;
      continue;
    }
    if (a === "--json-errors") {
      jsonErrors = true;
      i += 1;
      continue;
    }
    break;
  }
  return { argv: argv.slice(i), verboseCount, jsonErrors };
}

function kargoCliVersion() {
  const pkgPath = join(__dirname, "..", "package.json");
  const pkg = JSON.parse(readFileSync(pkgPath, "utf8"));
  return pkg.version || "0.0.0";
}

function help() {
  console.log(`kargo — Kern packages: GitHub installs, Kern registry search

Usage:
  kargo --version | -V
  kargo init [--project <dir>]        Create kargo.toml + .kern/ in cwd (or --project)
  kargo install <owner/repo[@tag|@range]>  [--resolve-debug] [--explain] [--verbose] [--dry-run] [--refresh]  Resolve graph, clone, update kargo.lock + .kern/package-paths.json
  kargo remove <owner/repo>
  kargo update [owner/repo[@tag]] [--resolve-debug] [--explain] [--verbose] [--dry-run] [--refresh]   Re-fetch (all [dependencies] if no arg)
  kargo graph [--project <dir>] [--json] [--why owner/repo]   Dependency tree from kargo.toml + kargo.lock; --why lists paths from the project root
  kargo list                        Cached packages under ~/.kargo/packages
  kargo search <query>              Search Kern package registry (see KERN_REGISTRY_URL / ~/.kern/registry-auth.json)
  kargo search --github <query>     Search GitHub repositories (legacy)
  kargo publish --tag vX.Y.Z        Validate kargo.toml, git tag + push (+ release if token)
  kargo login --token <pat>
  kargo build [--project <dir>] [entry.kn]
  kargo run [--project <dir>] [script.kn] [args...]

Project files:
  kargo.toml   name, version, [kargo] flags (resolution_mode, allow_prerelease, strict), [dependencies]
  kargo.lock   sorted keys; semver, commit_sha, resolved_version_range[_normalized], root (reproducible)

Flags:
  -v, --verbose    (global) Place before the subcommand, e.g. kargo -v install … — also KARGO_VERBOSE=1
  --json-errors    (global) On failure, print one JSON object to stderr (code, message, suggestion, exit); also KARGO_JSON_ERRORS=1
  --explain        Human-readable why-selected + dependency tree (stderr); use with install/update
  --dry-run        Resolve/plan only; no lock, package-paths, or git clone (uses cache when present)
  --refresh        Bypass ~/.kargo/cache/tags TTL for remote tag listing (install/update)
  Tag cache TTL    KARGO_TAG_CACHE_TTL_MS (default 600000)

Imports in .kn (after install):
  import "pkg-name"            — from kargo.toml name
  import "owner/repo"          — GitHub-style key
  import "github.com/owner/repo"

Requires: git on PATH, Node 18+, network for install/publish; search uses the registry index (network unless local path).
`);
}

async function main() {
  const raw = process.argv.slice(2);
  const { argv, verboseCount, jsonErrors } = stripLeadingGlobalFlags(raw);
  if (verboseCount > 0) process.env.KARGO_VERBOSE = "1";
  if (jsonErrors) process.env.KARGO_JSON_ERRORS = "1";
  const jsonErr =
    jsonErrors || String(process.env.KARGO_JSON_ERRORS || "").trim() === "1";
  const cmd = argv[0];
  try {
    if (!cmd || cmd === "--help" || cmd === "-h") {
      help();
      return;
    }
    if (cmd === "--version" || cmd === "-V") {
      console.log(kargoCliVersion());
      return;
    }
    if (cmd === "init") return await runInit(argv.slice(1));
    if (cmd === "install") return await runInstall(argv.slice(1));
    if (cmd === "remove") return await runRemove(argv.slice(1));
    if (cmd === "update") return await runUpdate(argv.slice(1));
    if (cmd === "graph") return await runGraph(argv.slice(1));
    if (cmd === "list") return await runList();
    if (cmd === "search") return await runSearch(argv.slice(1));
    if (cmd === "publish") return await runPublish(argv.slice(1));
    if (cmd === "login") return await runLogin(argv.slice(1));
    if (cmd === "build") return await runBuild(argv.slice(1));
    if (cmd === "run") return await runRun(argv.slice(1));
    throw new KargoCliError(`Unknown command: ${cmd}`, EXIT.USAGE);
  } catch (e) {
    let code = EXIT.USER;
    if (e instanceof KargoCliError && Number.isInteger(e.exitCode)) code = e.exitCode;
    if (jsonErr) {
      if (e instanceof KargoCliError) {
        console.error(JSON.stringify(kargoErrorToJson(e)));
      } else {
        const message = e && typeof e.message === "string" ? e.message : String(e);
        console.error(
          JSON.stringify({
            error: { code: "KARGO_ERROR", message, suggestion: "", exit: EXIT.USER }
          })
        );
      }
    } else {
      const msg = formatKargoMessage(e);
      console.error(`kargo: ${msg}`);
      const sug = suggestionForError(e);
      if (sug) {
        console.error("  Suggestion:");
        for (const line of sug.split("\n")) {
          console.error(`    ${line}`);
        }
      }
    }
    if (kargoVerbose() && e && typeof e.stack === "string") console.error(e.stack);
    process.exitCode = code;
  }
}

main();
