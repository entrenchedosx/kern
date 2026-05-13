import fs from "node:fs/promises";
import path from "node:path";
import semver from "semver";
import { parseKargoToml } from "./parse-kargo-toml.js";
import { runGit, listRemoteSemverTags } from "./git.js";
import { parseGithubSpec, cloneUrl, tagToSemver } from "./spec.js";
import { globalPackageRoot, kargoCacheDir } from "./paths.js";
import { readPersistentTagCache, writePersistentTagCache } from "./tag-cache.js";
import { readConfig } from "./config.js";
import { readLock, writeLock, lockKey } from "./lockfile.js";
import { mergeKernPaths } from "./package-paths.js";
import { parseDependencyDeclarations, constraintFromInstallSpec } from "./dep-entry.js";
import {
  pickHighestSatisfyingDetailed,
  versionSatisfiesConstraint,
  assertValidConstraintRaw,
  formatMergedVersionRange,
  normalizedConstraintIntersection,
  normalizeTagList
} from "./ranges.js";
import { formatResolutionFailure } from "./resolve-errors.js";
import { buildResolveDebugStep } from "./resolve-trace.js";
import { summarizeWhySelected } from "./why-version.js";
import { kargoVerbose } from "./cli-env.js";
import { KargoCliError, EXIT, kargoIoError } from "./cli-error.js";

async function readText(p) {
  try {
    return await fs.readFile(p, "utf8");
  } catch {
    return "";
  }
}

async function resolveMainPath(packageRoot) {
  const kernJson = path.join(packageRoot, "kern.json");
  let main = "src/index.kn";
  try {
    const kj = JSON.parse(await readText(kernJson));
    if (kj.main) main = String(kj.main);
  } catch {
    /* default */
  }
  const abs = path.resolve(packageRoot, main);
  try {
    await fs.access(abs);
  } catch {
    throw new KargoCliError(`Package entry missing: ${abs} (set kern.json "main")`, EXIT.USER);
  }
  return abs;
}

async function readKargoName(packageRoot) {
  const raw = await readText(path.join(packageRoot, "kargo.toml"));
  if (!raw) return "";
  return parseKargoToml(raw).name || "";
}

async function ensureDir(p) {
  try {
    await fs.mkdir(p, { recursive: true });
  } catch (e) {
    throw kargoIoError(`kargo: could not create directory ${p}`, e);
  }
}

export async function fetchPackage(owner, repo, tag, token, opts = {}) {
  const dryRun = opts.dryRun === true;
  const url = cloneUrl(owner, repo, token);
  const sem = tagToSemver(tag);
  if (!semver.valid(sem)) throw new KargoCliError(`Invalid semver from tag: ${tag}`, EXIT.USER);

  const dest = globalPackageRoot(owner, repo, sem);

  if (dryRun) {
    await ensureDir(kargoCacheDir());
    try {
      await fs.access(path.join(dest, "kargo.toml"));
    } catch {
      console.error(
        `kargo: (dry-run) ${owner}/${repo}@${tag} — not in local cache (${dest}); transitive dependencies skipped`
      );
      return {
        owner,
        repo,
        tag,
        semver: sem,
        commit: "(dry-run)",
        root: dest,
        name: "",
        mainAbs: ""
      };
    }
    const commit = runGit(dest, ["rev-parse", "HEAD"]);
    const name = await readKargoName(dest);
    let mainAbs = "";
    try {
      mainAbs = await resolveMainPath(dest);
    } catch {
      /* optional entry */
    }
    return {
      owner,
      repo,
      tag,
      semver: sem,
      commit,
      root: dest,
      name,
      mainAbs
    };
  }

  try {
    await ensureDir(kargoCacheDir());
    await fs.rm(dest, { recursive: true, force: true });
    await fs.mkdir(path.dirname(dest), { recursive: true });
    runGit(process.cwd(), ["clone", "--depth", "1", "--branch", tag, url, dest], {
      ...process.env,
      GIT_TERMINAL_PROMPT: "0"
    });
    const commit = runGit(dest, ["rev-parse", "HEAD"]);
    const name = await readKargoName(dest);
    const mainAbs = await resolveMainPath(dest);
    return {
      owner,
      repo,
      tag,
      semver: sem,
      commit,
      root: dest,
      name,
      mainAbs
    };
  } catch (e) {
    if (e instanceof KargoCliError) throw e;
    throw kargoIoError(`kargo: could not fetch ${owner}/${repo}@${tag}`, e);
  }
}

async function metaFromLockDisk(depKey, ent, tag) {
  const mainAbs = await resolveMainPath(ent.root);
  const name = await readKargoName(ent.root);
  const [o, r] = depKey.split("/");
  return {
    owner: o,
    repo: r,
    tag,
    semver: ent.semver,
    commit: ent.commit_sha,
    root: ent.root,
    name,
    mainAbs
  };
}

/**
 * @param {{ owner: string; repo: string }} or - remote identity for re-clone
 */
async function loadMetaFromLockedEntry(depKey, ent, sortedRaws, or, token, force, dryRun) {
  const incPre = Boolean(semver.prerelease(ent.semver));
  if (
    !sortedRaws.every((r) =>
      versionSatisfiesConstraint(ent.semver, r, { includePrerelease: incPre })
    )
  ) {
    throw new KargoCliError(
      `kargo: resolution_mode=locked but ${depKey} @ ${ent.semver} in kargo.lock does not satisfy current constraints:\n  ${sortedRaws.join("\n  ")}\nSwitch to resolution_mode=latest and run kargo update, then lock again.`,
      EXIT.USER
    );
  }
  const tag =
    ent.resolved_tag && tagToSemver(ent.resolved_tag) === ent.semver
      ? ent.resolved_tag
      : `v${ent.semver}`;
  try {
    await fs.access(ent.root);
    return await metaFromLockDisk(depKey, ent, tag);
  } catch {
    if (force) {
      return await fetchPackage(or.owner, or.repo, tag, token, { dryRun });
    }
    throw new KargoCliError(
      `kargo: resolution_mode=locked — package ${depKey} is missing on disk at ${ent.root}\n  Re-fetch with kargo install/update --force or set resolution_mode=latest.`,
      EXIT.USER
    );
  }
}

function sortedUniqueConstraints(raws) {
  return [...new Set(raws.map((r) => String(r).trim()))].sort((a, b) => a.localeCompare(b));
}

/**
 * @param {string[]} constraintRaws
 */
async function useCachedIfPossible(key, lock, force, constraintRaws, resolvedTag) {
  if (force) return null;
  const existing = lock.packages[key];
  if (!existing?.root || !existing?.semver) return null;
  try {
    await fs.access(existing.root);
  } catch {
    return null;
  }
  const raws = constraintRaws.length ? constraintRaws : ["*"];
  const incPre = Boolean(semver.prerelease(existing.semver));
  if (!raws.every((r) => versionSatisfiesConstraint(existing.semver, r, { includePrerelease: incPre }))) {
    return null;
  }
  if (resolvedTag && tagToSemver(resolvedTag) !== existing.semver) return null;
  const mainAbs = await resolveMainPath(existing.root);
  const name = await readKargoName(existing.root);
  const [o, r] = key.split("/");
  return {
    owner: o,
    repo: r,
    tag: existing.resolved_tag || `v${existing.semver}`,
    semver: existing.semver,
    commit: existing.commit_sha,
    root: existing.root,
    name,
    mainAbs
  };
}

function formatResolvedFrom(depKey, edges, raws) {
  const bits = edges
    .filter((e) => e.depKey === depKey)
    .sort(
      (a, b) =>
        a.sourceKey.localeCompare(b.sourceKey) || a.constraintRaw.localeCompare(b.constraintRaw)
    )
    .map((e) => `${e.constraintRaw} ← ${e.sourceKey}`);
  return [...new Set(bits)].join("; ") || sortedUniqueConstraints(raws).join(" + ");
}

function lockPackageEntry(depKey, meta, raws, edges) {
  const sortedRaws = sortedUniqueConstraints(raws);
  const entry = {
    commit_sha: meta.commit,
    resolved_constraints: sortedRaws,
    resolved_from: formatResolvedFrom(depKey, edges, raws),
    resolved_tag: meta.tag,
    resolved_version_range: formatMergedVersionRange(raws),
    root: meta.root,
    semver: meta.semver
  };
  const norm = normalizedConstraintIntersection(sortedRaws, meta.semver);
  if (norm) entry.resolved_version_range_normalized = norm;
  return entry;
}

function pruneResolved(edges, resolved) {
  const needed = new Set(edges.map((e) => e.depKey));
  for (const k of [...resolved.keys()]) {
    if (!needed.has(k)) resolved.delete(k);
  }
}

function printDryRunPlan(resolved) {
  console.log("kargo: dry-run — would record in kargo.lock:");
  for (const k of [...resolved.keys()].sort((a, b) => a.localeCompare(b))) {
    const m = resolved.get(k);
    const c = String(m.commit || "").slice(0, 7);
    console.log(`  ${k} @ ${m.semver} (${m.tag})  commit ${c || "(n/a)"}`);
  }
}

function printResolveTree(edges, resolved) {
  const children = new Map();
  for (const e of edges) {
    if (!children.has(e.sourceKey)) children.set(e.sourceKey, []);
    children.get(e.sourceKey).push(e);
  }
  for (const [, list] of children) {
    list.sort(
      (a, b) =>
        a.depKey.localeCompare(b.depKey) || a.constraintRaw.localeCompare(b.constraintRaw)
    );
  }

  function walk(sourceKey, prefix) {
    const outs = children.get(sourceKey) || [];
    for (let i = 0; i < outs.length; i += 1) {
      const e = outs[i];
      const last = i === outs.length - 1;
      const branch = last ? "└── " : "├── ";
      const meta = resolved.get(e.depKey);
      const ver = meta ? meta.semver : "?";
      const pre = meta && semver.prerelease(meta.semver) ? " (prerelease)" : "";
      console.error(`${prefix}${branch}${e.depKey} @ ${ver}${pre}  [${e.constraintRaw}]`);
      const nextPrefix = prefix + (last ? "    " : "│   ");
      walk(e.depKey, nextPrefix);
    }
  }

  const roots = [...new Set(edges.map((e) => e.sourceKey))].filter(
    (s) => s === "__install__" || s === "__project__"
  );
  roots.sort((a, b) => a.localeCompare(b));
  if (!roots.length) {
    console.error("(no __install__ / __project__ root in edge list)");
    return;
  }
  for (const r of roots) {
    console.error(`${r}`);
    walk(r, "");
  }
}

export async function readProjectKargoFlags(projectDir) {
  const raw = await readText(path.join(projectDir, "kargo.toml"));
  if (!raw) return { allow_prerelease: true, resolution_mode: "latest", strict: false };
  const p = parseKargoToml(raw);
  const mode =
    String(p.kargo?.resolution_mode || "latest").toLowerCase() === "locked" ? "locked" : "latest";
  const strict = p.kargo?.strict === true;
  return {
    allow_prerelease: strict ? false : p.kargo?.allow_prerelease !== false,
    resolution_mode: mode,
    strict
  };
}

/**
 * @param {Array<{ sourceKey: string; depKey: string; owner: string; repo: string; constraintRaw: string }>} initialEdges
 */
async function solveAndFetch({
  force,
  token,
  lock,
  initialEdges,
  allowPrereleaseFallback,
  resolutionMode,
  refreshRemoteTags,
  dryRun,
  debug,
  explain = false
}) {
  let edges = initialEdges.map((e) => ({ ...e }));
  edges.sort(
    (a, b) =>
      a.depKey.localeCompare(b.depKey) ||
      a.sourceKey.localeCompare(b.sourceKey) ||
      a.constraintRaw.localeCompare(b.constraintRaw)
  );
  for (const e of edges) {
    assertValidConstraintRaw(e.constraintRaw);
  }

  const ownerMap = new Map();
  function register(e) {
    if (!ownerMap.has(e.depKey)) ownerMap.set(e.depKey, { owner: e.owner, repo: e.repo });
  }
  for (const e of edges) register(e);

  /** @type {Map<string, { semver: string; tag: string; root: string; commit: string; name: string; mainAbs: string }>} */
  const resolved = new Map();

  /** @type {Map<string, string[]>} */
  const tagCache = new Map();

  /** @type {object[]} */
  const trace = [];

  /** @type {Map<string, { pick: object; sortedRaws: string[] }>} */
  const lastPicks = new Map();

  async function tagsForRemote(or, url) {
    if (tagCache.has(url)) return tagCache.get(url);
    let tags = null;
    if (!refreshRemoteTags) {
      const hit = readPersistentTagCache(or.owner, or.repo);
      if (hit?.tags?.length) tags = hit.tags;
    }
    if (!tags) {
      tags = listRemoteSemverTags(url);
      writePersistentTagCache(or.owner, or.repo, tags);
    }
    tagCache.set(url, tags);
    return tags;
  }

  let iter = 0;
  while (iter++ < 100) {
    let changed = false;
    const depKeys = [...new Set(edges.map((e) => e.depKey))].sort((a, b) => a.localeCompare(b));

    for (const depKey of depKeys) {
      const raws = edges.filter((e) => e.depKey === depKey).map((e) => e.constraintRaw);
      const sortedRaws = sortedUniqueConstraints(raws);
      const or = ownerMap.get(depKey);
      if (!or) throw new KargoCliError(`kargo: internal error: missing owner for ${depKey}`, EXIT.INTERNAL);

      const url = cloneUrl(or.owner, or.repo, token);
      let tags = [];
      /** @type {{ choice: { tag: string; sv: string } | null; usedPrereleaseFallback: boolean; sortedTags: string[] }} */
      let pick;

      if (resolutionMode === "locked") {
        const ent = lock.packages[depKey];
        if (!ent?.semver) {
          throw new KargoCliError(
            `kargo: resolution_mode=locked requires "${depKey}" in kargo.lock.\n  Resolve once with resolution_mode=latest (kargo install / kargo update), then switch to locked for CI.`,
            EXIT.USER
          );
        }
        const tagNorm =
          ent.resolved_tag && tagToSemver(ent.resolved_tag) === ent.semver
            ? ent.resolved_tag
            : `v${ent.semver}`;
        pick = {
          choice: { sv: ent.semver, tag: tagNorm },
          usedPrereleaseFallback: Boolean(semver.prerelease(ent.semver)),
          sortedTags: normalizeTagList([tagNorm])
        };
      } else {
        try {
          tags = await tagsForRemote(or, url);
        } catch (err) {
          if (err instanceof KargoCliError) throw err;
          throw new KargoCliError(`kargo: could not list tags for ${depKey}: ${err.message}`, EXIT.USER);
        }
        pick = pickHighestSatisfyingDetailed(tags, sortedRaws, { allowPrereleaseFallback });
      }

      const best = pick.choice;

      if (debug) {
        trace.push(
          buildResolveDebugStep({
            iteration: iter,
            depKey,
            sortedRaws,
            sortedTags: pick.sortedTags,
            pick,
            lsRemoteCacheSize: tagCache.size,
            resolutionMode
          })
        );
      }

      if (!best) {
        const tagList = resolutionMode === "locked" ? [] : tags;
        throw new KargoCliError(
          formatResolutionFailure(depKey, edges, sortedRaws, tagList, {
            allowPrereleaseFallback
          }),
          EXIT.USER
        );
      }

      lastPicks.set(depKey, { pick, sortedRaws });

      const prev = resolved.get(depKey);
      if (prev && prev.semver === best.sv) {
        lock.packages[depKey] = lockPackageEntry(depKey, prev, sortedRaws, edges);
        continue;
      }

      if (prev) {
        edges = edges.filter((e) => e.sourceKey !== depKey);
      }

      let meta;
      if (resolutionMode === "locked") {
        const ent = lock.packages[depKey];
        meta =
          (await useCachedIfPossible(depKey, lock, force, sortedRaws, best.tag)) ||
          (await loadMetaFromLockedEntry(depKey, ent, sortedRaws, or, token, force, dryRun));
      } else {
        meta =
          (await useCachedIfPossible(depKey, lock, force, sortedRaws, best.tag)) ||
          (await fetchPackage(or.owner, or.repo, best.tag, token, { dryRun }));
      }

      const incPre = Boolean(semver.prerelease(meta.semver));
      if (
        !sortedRaws.every((r) =>
          versionSatisfiesConstraint(meta.semver, r, { includePrerelease: incPre })
        )
      ) {
        throw new KargoCliError(
          `kargo: resolved ${depKey} @ ${meta.semver} does not satisfy constraints`,
          EXIT.INTERNAL
        );
      }

      resolved.set(depKey, {
        semver: meta.semver,
        tag: meta.tag,
        root: meta.root,
        commit: meta.commit,
        name: meta.name,
        mainAbs: meta.mainAbs
      });

      lock.packages[depKey] = lockPackageEntry(depKey, meta, sortedRaws, edges);

      const kargoRaw = await readText(path.join(meta.root, "kargo.toml"));
      const kargo = kargoRaw ? parseKargoToml(kargoRaw) : { dependencies: {} };
      const sub = parseDependencyDeclarations(kargo.dependencies || {});
      sub.sort((a, b) => a.depKey.localeCompare(b.depKey));
      for (const d of sub) {
        assertValidConstraintRaw(d.constraintRaw);
        edges.push({
          sourceKey: depKey,
          depKey: d.depKey,
          owner: d.owner,
          repo: d.repo,
          constraintRaw: d.constraintRaw
        });
        register({ depKey: d.depKey, owner: d.owner, repo: d.repo });
      }

      edges.sort(
        (a, b) =>
          a.depKey.localeCompare(b.depKey) ||
          a.sourceKey.localeCompare(b.sourceKey) ||
          a.constraintRaw.localeCompare(b.constraintRaw)
      );

      changed = true;
    }

    pruneResolved(edges, resolved);
    if (!changed) break;
  }

  if (iter >= 100) {
    throw new KargoCliError(
      "kargo: resolver did not stabilize (internal limit; please report)",
      EXIT.INTERNAL
    );
  }

  for (const k of Object.keys(lock.packages || {})) {
    if (!resolved.has(k)) delete lock.packages[k];
  }

  if (debug) {
    console.error("\n── kargo resolve-debug: trace (sorted order per iteration) ──");
    console.error(JSON.stringify(trace, null, 2));
    console.error("── dependency tree (resolved versions) ──");
    printResolveTree(edges, resolved);
    console.error("── end resolve-debug ──\n");
  }

  if (explain) {
    console.error("\n── kargo explain: why these versions ──");
    const expKeys = [...lastPicks.keys()].sort((a, b) => a.localeCompare(b));
    for (const k of expKeys) {
      const w = lastPicks.get(k);
      if (w?.pick?.choice) {
        console.error(`\n[${k}]`);
        console.error(summarizeWhySelected(w.sortedRaws, w.pick, resolutionMode));
      }
    }
    if (!debug) {
      console.error("\n── kargo explain: dependency tree ──");
      printResolveTree(edges, resolved);
    }
    console.error("── end explain ──\n");
  }

  const pathEntries = [];
  if (!dryRun) {
    const orderedKeys = [...resolved.keys()].sort((a, b) => a.localeCompare(b));
    for (const key of orderedKeys) {
      const meta = resolved.get(key);
      pathEntries.push({
        keys: [key, `github.com/${key}`, ...(meta.name ? [meta.name] : [])],
        root: meta.root,
        semver: meta.semver,
        mainAbs: meta.mainAbs
      });
    }
  }

  return { pathEntries, resolved, lastPicks };
}

/**
 * Install a root package by spec and resolve the full graph (semver ranges, single version per repo).
 */
export async function installFromSpec(spec, options = {}) {
  const {
    projectDir = process.cwd(),
    force = false,
    resolveDebug = false,
    explain = false,
    verbose = false,
    dryRun = false,
    refreshRemoteTags = false
  } = options;
  const cfg = await readConfig();
  const token = cfg.github?.token || process.env.GITHUB_TOKEN || "";
  const flags = await readProjectKargoFlags(projectDir);

  const rootParsed = parseGithubSpec(spec);
  const lock = await readLock(projectDir);
  lock.lockVersion = 2;
  lock.packages = lock.packages || {};

  const c = constraintFromInstallSpec(rootParsed);
  assertValidConstraintRaw(c);
  const rootKey = lockKey(rootParsed.owner, rootParsed.repo);

  const initialEdges = [
    {
      sourceKey: "__install__",
      depKey: rootKey,
      owner: rootParsed.owner,
      repo: rootParsed.repo,
      constraintRaw: c
    }
  ];

  const allowPrereleaseFallback = flags.strict ? false : flags.allow_prerelease;

  const { pathEntries, lastPicks, resolved } = await solveAndFetch({
    force,
    token,
    lock,
    initialEdges,
    allowPrereleaseFallback,
    resolutionMode: flags.resolution_mode,
    refreshRemoteTags,
    dryRun,
    debug: resolveDebug,
    explain
  });

  if (verbose && !explain) {
    const w = lastPicks.get(rootKey);
    if (w?.pick?.choice) {
      console.log(summarizeWhySelected(w.sortedRaws, w.pick, flags.resolution_mode));
    }
  }

  if (dryRun) {
    printDryRunPlan(resolved);
    console.log("kargo: dry-run — no writes (kargo.lock, .kern/package-paths.json, git clone)");
    return lock.packages[rootKey];
  }

  await writeLock(projectDir, lock);
  for (const e of pathEntries) {
    await mergeKernPaths(projectDir, [e]);
  }

  return lock.packages[rootKey];
}

/**
 * Resolve and fetch all [dependencies] from project kargo.toml (no root package).
 */
export async function installFromProjectManifest(projectDir, options = {}) {
  const {
    force = false,
    resolveDebug = false,
    explain = false,
    verbose = false,
    dryRun = false,
    refreshRemoteTags = false
  } = options;
  const cfg = await readConfig();
  const token = cfg.github?.token || process.env.GITHUB_TOKEN || "";

  const raw = await readText(path.join(projectDir, "kargo.toml"));
  if (!raw) throw new KargoCliError("kargo: kargo.toml not found", EXIT.USER);
  const kargo = parseKargoToml(raw);
  const decls = parseDependencyDeclarations(kargo.dependencies || {});
  if (!decls.length) {
    return { count: 0 };
  }

  const lock = await readLock(projectDir);
  lock.lockVersion = 2;
  lock.packages = lock.packages || {};

  const initialEdges = decls
    .map((d) => ({
      sourceKey: "__project__",
      depKey: d.depKey,
      owner: d.owner,
      repo: d.repo,
      constraintRaw: d.constraintRaw
    }))
    .sort(
      (a, b) =>
        a.depKey.localeCompare(b.depKey) || a.constraintRaw.localeCompare(b.constraintRaw)
    );

  const resolutionMode =
    String(kargo.kargo?.resolution_mode || "latest").toLowerCase() === "locked"
      ? "locked"
      : "latest";

  const strict = kargo.kargo?.strict === true;
  const allowPrereleaseFallback = strict ? false : kargo.kargo?.allow_prerelease !== false;

  const { pathEntries, resolved, lastPicks } = await solveAndFetch({
    force,
    token,
    lock,
    initialEdges,
    allowPrereleaseFallback,
    resolutionMode,
    refreshRemoteTags,
    dryRun,
    debug: resolveDebug,
    explain
  });

  if (verbose && !explain) {
    const vk = [...lastPicks.keys()].sort((a, b) => a.localeCompare(b));
    for (const k of vk) {
      const w = lastPicks.get(k);
      if (w?.pick?.choice) {
        console.log(`\n[${k}]`);
        console.log(summarizeWhySelected(w.sortedRaws, w.pick, resolutionMode));
      }
    }
  }

  if (dryRun) {
    printDryRunPlan(resolved);
    console.log("kargo: dry-run — no writes (kargo.lock, .kern/package-paths.json, git clone)");
    return { count: resolved.size };
  }

  await writeLock(projectDir, lock);
  for (const e of pathEntries) {
    await mergeKernPaths(projectDir, [e]);
  }

  return { count: pathEntries.length };
}

export async function runInstall(argv) {
  let project = process.cwd();
  let spec = null;
  let force = false;
  let resolveDebug = false;
  let explain = false;
  let verbose = false;
  let dryRun = false;
  let refreshRemoteTags = false;
  for (let i = 0; i < argv.length; i += 1) {
    const a = argv[i];
    if (a === "--project" && i + 1 < argv.length) {
      project = path.resolve(argv[++i]);
    } else if (a === "--force") {
      force = true;
    } else if (a === "--resolve-debug") {
      resolveDebug = true;
    } else if (a === "--explain") {
      explain = true;
    } else if (a === "--verbose" || a === "-v") {
      verbose = true;
    } else if (a === "--dry-run") {
      dryRun = true;
    } else if (a === "--refresh") {
      refreshRemoteTags = true;
    } else if (!spec) {
      spec = a;
    } else {
      throw new KargoCliError(`Unknown argument: ${a}`, EXIT.USAGE);
    }
  }
  if (!spec) {
    throw new KargoCliError(
      "Usage: kargo install <owner/repo[@tag|@range]> [--project <dir>] [--force] [--resolve-debug] [--explain] [--verbose|-v] [--dry-run] [--refresh]\n" +
        "  Example: kargo install org/repo@^1.0.0\n" +
        "  To install all [dependencies] from kargo.toml: kargo update",
      EXIT.USAGE
    );
  }
  verbose = verbose || kargoVerbose();
  const p = parseGithubSpec(spec);
  const meta = await installFromSpec(spec, {
    projectDir: project,
    force,
    resolveDebug,
    explain,
    verbose,
    dryRun,
    refreshRemoteTags
  });
  if (dryRun) {
    console.log(`kargo: dry-run complete for ${p.owner}/${p.repo}`);
    return;
  }
  if (!meta || !meta.resolved_tag) {
    throw new KargoCliError(
      "kargo: install finished but lock entry is missing (internal error); try --resolve-debug or report a bug",
      EXIT.INTERNAL
    );
  }
  console.log(
    `kargo: locked ${p.owner}/${p.repo} @ ${meta.resolved_tag} (${String(meta.commit_sha).slice(0, 7)})`
  );
  console.log(`kargo: wrote kargo.lock and .kern/package-paths.json`);
}
