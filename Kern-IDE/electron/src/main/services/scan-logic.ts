import { readdir } from 'node:fs/promises'
import { join } from 'node:path'

const SKIP = new Set(['node_modules', '.git', 'out', 'dist', 'build', 'release'])

export async function walkWorkspaceFiles(root: string, maxFiles: number): Promise<string[]> {
  const out: string[] = []
  const stack: string[] = [root]
  while (stack.length && out.length < maxFiles) {
    const dir = stack.pop()!
    let names: Awaited<ReturnType<typeof readdir>>
    try {
      names = await readdir(dir, { withFileTypes: true })
    } catch {
      continue
    }
    for (const d of names) {
      if (out.length >= maxFiles) break
      const full = join(dir, d.name)
      if (d.isDirectory()) {
        if (SKIP.has(d.name)) continue
        stack.push(full)
      } else {
        out.push(full)
      }
    }
  }
  return out
}
