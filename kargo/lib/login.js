import { readConfig, writeConfig } from "./config.js";
import { KargoCliError, EXIT } from "./cli-error.js";

export async function runLogin(argv) {
  if (argv[0] !== "--token" || !argv[1]) {
    throw new KargoCliError(
      "Usage: kargo login --token <github_pat>\nWrites ~/.kargo/config.json (github.token).",
      EXIT.USAGE
    );
  }
  const token = argv[1];
  const cfg = await readConfig();
  cfg.github = cfg.github || {};
  cfg.github.token = token;
  await writeConfig(cfg);
  console.log("kargo: saved github.token to ~/.kargo/config.json");
}
