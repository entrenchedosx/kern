#!/usr/bin/env node
import { runLogin } from "./login.js";
import { runPublish } from "./publish.js";
import { runInstall } from "./install.js";
import { runSearch } from "./search.js";
import { runInfo } from "./info.js";

function help() {
  console.log(`kern-gh-pkg

Use GitHub as package registry/auth server.

Commands:
  kern-gh-pkg login --token <ghp_...> --repo <owner/repo> [--no-validate]
  kern-gh-pkg login --show | --logout
  kern-gh-pkg publish [--dir <path>] [--bump patch|minor|major]
  kern-gh-pkg install [<pkg>[@range]] [--project <path>]
  kern-gh-pkg search <query>
  kern-gh-pkg info <pkg> [range]
`);
}

async function main() {
  const argv = process.argv.slice(2);
  const cmd = argv[0];
  try {
    if (!cmd || cmd === "--help" || cmd === "-h") {
      help();
      return;
    }
    if (cmd === "login") return runLogin(argv.slice(1));
    if (cmd === "publish") return runPublish(argv.slice(1));
    if (cmd === "install") return runInstall(argv.slice(1));
    if (cmd === "search") return runSearch(argv.slice(1));
    if (cmd === "info") return runInfo(argv.slice(1));
    throw new Error(`Unknown command: ${cmd}`);
  } catch (err) {
    console.error(`kern-gh-pkg: ${err.message}`);
    process.exitCode = 1;
  }
}

main();
