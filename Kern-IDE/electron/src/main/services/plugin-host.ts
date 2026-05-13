import { readdir, readFile } from 'node:fs/promises'
import { join } from 'node:path'
import { pathToFileURL } from 'node:url'
import type { CommandDescriptor } from '../../shared/ipc'

export type KernExtensionApi = {
  registerCommand: (id: string, handler: () => void | Promise<void>) => void
}

export type LoadedExtension = {
  id: string
  path: string
  commands: CommandDescriptor[]
  dispose?: () => void
}

const commandHandlers = new Map<string, () => void | Promise<void>>()

export function clearExtensionCommands(extensionId: string): void {
  for (const [id, fn] of [...commandHandlers.entries()]) {
    if (id.startsWith(`${extensionId}:`)) {
      commandHandlers.delete(id)
    }
  }
}

export async function executeExtensionCommand(fullId: string): Promise<void> {
  const fn = commandHandlers.get(fullId)
  if (fn) await fn()
}

/** Manifest: kern-extension.json next to entry, or package.json "kernExtension" */
type KernManifest = {
  name: string
  main?: string
  contributes?: {
    commands?: Array<{ command: string; title: string; category?: string }>
  }
}

async function readManifest(dir: string): Promise<KernManifest | null> {
  try {
    const raw = await readFile(join(dir, 'kern-extension.json'), 'utf8')
    return JSON.parse(raw) as KernManifest
  } catch {
    try {
      const pkg = JSON.parse(await readFile(join(dir, 'package.json'), 'utf8')) as {
        kernExtension?: KernManifest
      }
      return pkg.kernExtension ?? null
    } catch {
      return null
    }
  }
}

export async function loadExtensions(roots: string[]): Promise<LoadedExtension[]> {
  const loaded: LoadedExtension[] = []
  commandHandlers.clear()

  for (const root of roots) {
    let names: string[]
    try {
      names = await readdir(root, { withFileTypes: true }).then((d) =>
        d.filter((x) => x.isDirectory()).map((x) => x.name)
      )
    } catch {
      continue
    }

    for (const name of names) {
      const dir = join(root, name)
      const manifest = await readManifest(dir)
      if (!manifest?.name) continue

      const entry = manifest.main ?? 'main.js'
      const entryPath = join(dir, entry)

      const extId = manifest.name
      const staticCommands: CommandDescriptor[] = (manifest.contributes?.commands ?? []).map((c) => ({
        id: `${extId}:${c.command}`,
        title: c.title,
        category: c.category ?? extId,
        source: 'extension' as const,
        extensionId: extId
      }))

      try {
        const url = pathToFileURL(entryPath).href
        const mod = await import(url)
        const activate = mod.activate as ((api: KernExtensionApi) => void | Promise<void>) | undefined

        const registered: CommandDescriptor[] = [...staticCommands]

        if (typeof activate === 'function') {
          const api: KernExtensionApi = {
            registerCommand: (id, handler) => {
              const fullId = `${extId}:${id}`
              commandHandlers.set(fullId, handler)
              if (!registered.some((r) => r.id === fullId)) {
                registered.push({
                  id: fullId,
                  title: id,
                  category: extId,
                  source: 'extension',
                  extensionId: extId
                })
              }
            }
          }
          await activate(api)
        }

        for (const c of staticCommands) {
          if (!commandHandlers.has(c.id)) {
            commandHandlers.set(c.id, async () => {
              console.info(`[kern] extension command not implemented: ${c.id}`)
            })
          }
        }

        loaded.push({ id: extId, path: dir, commands: registered })
      } catch (e) {
        console.warn(`[kern] extension load failed ${dir}:`, e)
      }
    }
  }

  return loaded
}
