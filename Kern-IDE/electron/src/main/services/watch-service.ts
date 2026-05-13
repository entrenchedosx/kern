import chokidar from 'chokidar'
import type { BrowserWindow } from 'electron'
import { IPC, type WatchEventPayload } from '../../shared/ipc'

let watcher: chokidar.FSWatcher | null = null
let boundRoot = ''

export function stopWatch(): void {
  if (watcher) {
    void watcher.close()
    watcher = null
  }
  boundRoot = ''
}

export function startWatch(root: string, win: BrowserWindow): void {
  stopWatch()
  boundRoot = root
  watcher = chokidar.watch(root, {
    ignoreInitial: true,
    awaitWriteFinish: { stabilityThreshold: 100, pollInterval: 50 },
    ignored: [
      /(^|[\\/])node_modules([\\/]|$)/,
      /(^|[\\/])\.git([\\/]|$)/,
      /(^|[\\/])out([\\/]|$)/,
      /(^|[\\/])dist([\\/]|$)/
    ]
  })

  const send = (type: WatchEventPayload['type'], path: string) => {
    if (win.isDestroyed()) return
    const payload: WatchEventPayload = { type, path, root: boundRoot }
    win.webContents.send(IPC.WATCH_EVENT, payload)
  }

  watcher.on('add', (p) => send('add', p))
  watcher.on('change', (p) => send('change', p))
  watcher.on('unlink', (p) => send('unlink', p))
  watcher.on('addDir', (p) => send('addDir', p))
  watcher.on('unlinkDir', (p) => send('unlinkDir', p))
}
