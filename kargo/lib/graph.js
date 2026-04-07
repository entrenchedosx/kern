import path from "node:path";
import fs from "node:fs/promises";
import { readLock, lockKey } from "./lockfile.js";
import { parseKargoToml } from "./parse-kargo-toml.js";
import { parseDependencyDeclarations, parseGithubIdent } from "./dep-entry.js";
import { KargoCliError, EXIT } from "./cli-error.js";

async function readText(p) {
  try {
    return await fs.readFile(p, "utf8");
  } catch {
    return "";
  }
}

/**
 * Build a dependency tree from kargo.toml roots + on-disk manifests, limited to packages in the lockfile.
 * @param {string} projectDir
 */
export async function buildLockedGraph(projectDir) {
  const lock = await readLock(projectDir);
  const packages = lock.packages && typeof lock.packages === "object" ? lock.packages : {};

  const raw = await readText(path.join(projectDir, "kargo.toml"));
  const rootPkg = raw ? parseKargoToml(raw) : { dependencies: {} };
  const decls = parseDependencyDeclarations(rootPkg.dependencies || {});

  /** @type {Array<{ from: string; to: string; constraint: string }>} */
  const edges = [];
  const visitedManifest = new Set();

  async function walk(fromKey, depKey, constraint) {
    const ent = packages[depKey];
    if (!ent?.root) return;
    edges.push({ from: fromKey, to: depKey, constraint });
    if (visitedManifest.has(depKey)) return;
    visitedManifest.add(depKey);

    const kr = await readText(path.join(ent.root, "kargo.toml"));
    const sub = kr ? parseKargoToml(kr) : { dependencies: {} };
    const ds = parseDependencyDeclarations(sub.dependencies || {});
    ds.sort((a, b) => a.depKey.localeCompare(b.depKey));
    for (const d of ds) {
      await walk(depKey, d.depKey, d.constraintRaw);
    }
  }

  const VIRTUAL = "__project__";
  decls.sort((a, b) => a.depKey.localeCompare(b.depKey));
  for (const d of decls) {
    await walk(VIRTUAL, d.depKey, d.constraintRaw);
  }

  return { lock, edges, virtualRoot: VIRTUAL, hasProjectDeps: decls.length > 0 };
}

function printAsciiTree(lock, edges, virtualRoot) {
  const children = new Map();
  for (const e of edges) {
    if (!children.has(e.from)) children.set(e.from, []);
    children.get(e.from).push(e);
  }
  for (const [, list] of children) {
    list.sort((a, b) => a.to.localeCompare(b.to) || a.constraint.localeCompare(b.constraint));
  }

  function walk(from, prefix) {
    const outs = children.get(from) || [];
    for (let i = 0; i < outs.length; i += 1) {
      const e = outs[i];
      const last = i === outs.length - 1;
      const branch = last ? "└── " : "├── ";
      const ent = lock.packages[e.to];
      const ver = ent?.semver ?? "?";
      console.log(`${prefix}${branch}${e.to} @ ${ver}  [${e.constraint}]`);
      const next = prefix + (last ? "    " : "│   ");
      walk(e.to, next);
    }
  }

  if (!edges.length) {
    const keys = Object.keys(lock.packages || {}).sort((a, b) => a.localeCompare(b));
    if (!keys.length) {
      console.log("(no packages in kargo.lock)");
      return;
    }
    console.log("(no [dependencies] in kargo.toml — flat package list from lock)");
    for (const k of keys) {
      const v = lock.packages[k]?.semver ?? "?";
      console.log(`  ${k} @ ${v}`);
    }
    return;
  }

  console.log(virtualRoot);
  walk(virtualRoot, "");
}

/**
 * All simple paths virtualRoot → target over locked manifest edges (cap for large graphs).
 * @param {Array<{ from: string; to: string; constraint: string }>} edges
 * @param {string} virtualRoot
 * @param {string} targetKey
 */
export function pathsIntroducingPackage(edges, virtualRoot, targetKey) {
  const adj = new Map();
  for (const e of edges) {
    if (!adj.has(e.from)) adj.set(e.from, []);
    adj.get(e.from).push({ to: e.to, constraint: e.constraint });
  }
  for (const [, list] of adj) {
    list.sort((a, b) => a.to.localeCompare(b.to) || a.constraint.localeCompare(b.constraint));
  }

  const maxPaths = 64;
  const maxDepth = 48;
  /** @type {Array<Array<{ to: string; constraint: string }>>} */
  const paths = [];

  function dfs(node, depth, seen, segments) {
    if (paths.length >= maxPaths) return;
    if (node === targetKey) {
      paths.push(segments.map((s) => ({ ...s })));
      return;
    }
    if (depth >= maxDepth) return;
    for (const { to, constraint } of adj.get(node) || []) {
      if (seen.has(to)) continue;
      seen.add(to);
      segments.push({ to, constraint });
      dfs(to, depth + 1, seen, segments);
      segments.pop();
      seen.delete(to);
    }
  }

  dfs(virtualRoot, 0, new Set([virtualRoot]), []);
  return paths;
}

function formatWhyPath(virtualRoot, segments) {
  const parts = [virtualRoot, ...segments.map((s) => `→ ${s.to} [${s.constraint}]`)];
  return parts.join(" ");
}

export async function runGraph(argv) {
  let project = process.cwd();
  let asJson = false;
  let whySpec = null;
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--project" && i + 1 < argv.length) project = path.resolve(argv[++i]);
    else if (a === "--json") asJson = true;
    else if (a === "--why") {
      if (i + 1 >= argv.length) throw new KargoCliError("kargo graph --why requires owner/repo", EXIT.USAGE);
      whySpec = argv[++i];
    } else throw new KargoCliError(`Unknown argument: ${a}`, EXIT.USAGE);
  }

  const { lock, edges, virtualRoot, hasProjectDeps } = await buildLockedGraph(project);

  if (whySpec) {
    const { owner, repo } = parseGithubIdent(whySpec);
    const targetKey = lockKey(owner, repo);
    const pkgs = lock.packages && typeof lock.packages === "object" ? lock.packages : {};
    if (!pkgs[targetKey]) {
      throw new KargoCliError(
        `kargo graph --why: "${targetKey}" is not in kargo.lock (install or update first)`,
        EXIT.USER
      );
    }
    const paths = pathsIntroducingPackage(edges, virtualRoot, targetKey);
    if (asJson) {
      const formatted = paths.map((segs) => formatWhyPath(virtualRoot, segs));
      console.log(
        JSON.stringify(
          {
            projectDir: project,
            target: targetKey,
            path_count: paths.length,
            paths: formatted,
            path_edges: paths
          },
          null,
          2
        )
      );
      return;
    }
    if (!paths.length) {
      console.log(
        `No dependency path from ${virtualRoot} to ${targetKey} (package is in the lockfile but not reachable from [dependencies] in this project’s manifests).`
      );
      return;
    }
    console.log(`Paths that introduce ${targetKey}:`);
    for (const segs of paths) {
      console.log(`  ${formatWhyPath(virtualRoot, segs)}`);
    }
    if (paths.length >= 64) {
      console.log("(path listing capped at 64; simplify the graph or use --json for machine output)");
    }
    return;
  }

  if (asJson) {
    const nodes = {};
    for (const k of Object.keys(lock.packages || {}).sort((a, b) => a.localeCompare(b))) {
      const ent = lock.packages[k];
      nodes[k] = {
        semver: ent.semver,
        resolved_tag: ent.resolved_tag,
        root: ent.root
      };
    }
    const sortedEdges = [...edges].sort(
      (a, b) =>
        a.from.localeCompare(b.from) || a.to.localeCompare(b.to) || a.constraint.localeCompare(b.constraint)
    );
    console.log(
      JSON.stringify(
        {
          projectDir: project,
          virtual_root: virtualRoot,
          has_project_dependencies: hasProjectDeps,
          nodes,
          edges: sortedEdges
        },
        null,
        2
      )
    );
    return;
  }

  printAsciiTree(lock, edges, virtualRoot);
}
