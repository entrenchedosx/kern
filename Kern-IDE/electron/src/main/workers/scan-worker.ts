import { parentPort } from 'node:worker_threads'
import { walkWorkspaceFiles } from '../services/scan-logic'

type ScanMsg = { type: 'scan'; root: string; maxFiles?: number }

parentPort?.on('message', async (msg: ScanMsg) => {
  if (msg?.type !== 'scan') return
  const maxFiles = Math.min(msg.maxFiles ?? 50_000, 200_000)
  try {
    const files = await walkWorkspaceFiles(msg.root, maxFiles)
    parentPort?.postMessage({ ok: true, files })
  } catch (e) {
    const err = e instanceof Error ? e.message : String(e)
    parentPort?.postMessage({ ok: false, error: err })
  }
})
