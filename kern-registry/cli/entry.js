#!/usr/bin/env node
import { runPublish } from "./publish.js";
import { runInstall } from "./install.js";
import { runSearch } from "./search.js";
import { runInfo } from "./info.js";

function printHelp() {
  console.log(`kern-pkg

Usage:
  kern-pkg publish [--dir <path>] [--bump patch|minor|major] [--public] [--dry-run]
  kern-pkg install [<pkg>[@range]] [--project <path>] [--update]
  kern-pkg search <query>
  kern-pkg info <package> [range]
`);
}

async function main() {
  const argv = process.argv.slice(2);
  const cmd = argv[0];
  try {
    if (!cmd || cmd === "--help" || cmd === "-h") {
      printHelp();
      return;
    }
    if (cmd === "publish") {
      await runPublish(argv.slice(1));
      return;
    }
    if (cmd === "install") {
      await runInstall(argv.slice(1));
      return;
    }
    if (cmd === "search") {
      await runSearch(argv.slice(1));
      return;
    }
    if (cmd === "info") {
      await runInfo(argv.slice(1));
      return;
    }
    throw new Error(`Unknown command: ${cmd}`);
  } catch (err) {
    console.error(`kern-pkg: ${err.message}`);
    process.exitCode = 1;
  }
}

main();
