import path from "node:path";
import { homeDir } from "./io.js";

export function getCacheRoot() {
  return path.join(homeDir(), ".kern", "cache");
}

export function getProjectKernRoot(projectDir) {
  return path.join(projectDir, ".kern");
}

export function getProjectPackagesRoot(projectDir) {
  return path.join(getProjectKernRoot(projectDir), "packages");
}

export function getProjectPackagePathsFile(projectDir) {
  return path.join(getProjectKernRoot(projectDir), "package-paths.json");
}

export function getManifestPath(projectDir) {
  return path.join(projectDir, "kern.json");
}

export function getLockfilePath(projectDir) {
  return path.join(projectDir, "kern.lock");
}
