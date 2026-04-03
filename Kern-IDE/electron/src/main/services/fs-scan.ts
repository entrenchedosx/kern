import { existsSync } from 'node:fs'
import { Worker } from 'node:worker_threads'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { walkWorkspaceFiles } from './scan-logic'

const __dirname = dirname(fileURLToPath(import.meta.url))

export function scanWorkspaceFiles(root: string, maxFiles?: number): Promise<string[]> {
  const cap = Math.min(maxFiles ?? 80_000, 200_000)
  const workerPath = join(__dirname, 'workers', 'scan-worker.mjs')

  if (!existsSync(workerPath)) {
    return walkWorkspaceFiles(root, cap)
  }

  return new Promise((resolve, reject) => {
    const w = new Worker(workerPath, { type: 'module' })
    const t = setTimeout(() => {
      void w.terminate()
      reject(new Error('scan timeout'))
    }, 120_000)
    w.on('message', (msg: { ok: boolean; files?: string[]; error?: string }) => {
      clearTimeout(t)
      void w.terminate()
      if (msg.ok && msg.files) resolve(msg.files)
      else reject(new Error(msg.error ?? 'scan failed'))
    })
    w.on('error', (e) => {
      clearTimeout(t)
      reject(e)
    })
    w.postMessage({ type: 'scan', root, maxFiles: cap })
  })
}
