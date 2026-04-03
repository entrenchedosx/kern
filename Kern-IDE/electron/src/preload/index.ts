import { contextBridge, ipcRenderer } from 'electron'
import { IPC, type WatchEventPayload, type DirEntry, type CommandDescriptor } from '../shared/ipc'

export type ThemeOption = { id: string; label: string }

export type KernApi = {
  openFolder: () => Promise<string | null>
  readDir: (dir: string) => Promise<DirEntry[]>
  readFile: (path: string) => Promise<string>
  writeFile: (path: string, content: string) => Promise<void>
  stat: (path: string) => Promise<{ isDirectory: boolean; mtime: number }>
  scanWorkspace: (root: string) => Promise<string[]>
  watchStart: (root: string) => Promise<void>
  watchStop: () => Promise<void>
  listCommands: () => Promise<CommandDescriptor[]>
  executeCommand: (id: string) => Promise<void>
  getTheme: () => Promise<string>
  setTheme: (id: string) => Promise<void>
  listThemes: () => Promise<ThemeOption[]>
  readCustomTheme: (path: string) => Promise<string>
  reloadExtensions: () => Promise<void>
  onWatchEvent: (handler: (payload: WatchEventPayload) => void) => () => void
  onThemeUpdated: (handler: (theme: string) => void) => () => void
  onWorkbenchCommand: (handler: (id: string) => void) => () => void
  onShowCommandPalette: (handler: () => void) => () => void
}

const kernAPI: KernApi = {
  openFolder: () => ipcRenderer.invoke(IPC.DIALOG_OPEN_FOLDER),
  readDir: (dir) => ipcRenderer.invoke(IPC.FS_READ_DIR, dir),
  readFile: (path) => ipcRenderer.invoke(IPC.FS_READ_FILE, path),
  writeFile: (path, content) => ipcRenderer.invoke(IPC.FS_WRITE_FILE, path, content),
  stat: (path) => ipcRenderer.invoke(IPC.FS_STAT, path),
  scanWorkspace: (root) => ipcRenderer.invoke(IPC.FS_SCAN, root),
  watchStart: (root) => ipcRenderer.invoke(IPC.WATCH_START, root),
  watchStop: () => ipcRenderer.invoke(IPC.WATCH_STOP),
  listCommands: () => ipcRenderer.invoke(IPC.COMMANDS_LIST),
  executeCommand: (id) => ipcRenderer.invoke(IPC.COMMANDS_EXECUTE, id),
  getTheme: () => ipcRenderer.invoke(IPC.THEME_GET),
  setTheme: (id) => ipcRenderer.invoke(IPC.THEME_SET, id),
  listThemes: () => ipcRenderer.invoke(IPC.THEME_LIST),
  readCustomTheme: (path) => ipcRenderer.invoke('kern:theme:readCustom', path),
  reloadExtensions: () => ipcRenderer.invoke('kern:extensions:reload'),

  onWatchEvent: (handler) => {
    const fn = (_e: Electron.IpcRendererEvent, p: WatchEventPayload) => handler(p)
    ipcRenderer.on(IPC.WATCH_EVENT, fn)
    return () => ipcRenderer.removeListener(IPC.WATCH_EVENT, fn)
  },
  onThemeUpdated: (handler) => {
    const fn = (_e: Electron.IpcRendererEvent, t: string) => handler(t)
    ipcRenderer.on('kern:theme:updated', fn)
    return () => ipcRenderer.removeListener('kern:theme:updated', fn)
  },
  onWorkbenchCommand: (handler) => {
    const fn = (_e: Electron.IpcRendererEvent, id: string) => handler(id)
    ipcRenderer.on('kern:command:invoke', fn)
    return () => ipcRenderer.removeListener('kern:command:invoke', fn)
  },
  onShowCommandPalette: (handler) => {
    const fn = () => handler()
    ipcRenderer.on('kern:command:showPalette', fn)
    return () => ipcRenderer.removeListener('kern:command:showPalette', fn)
  }
}

contextBridge.exposeInMainWorld('kernAPI', kernAPI)

declare global {
  interface Window {
    kernAPI: KernApi
  }
}
