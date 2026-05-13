/** Mirror of preload `KernApi` for renderer typing. */
import type { WatchEventPayload, DirEntry, CommandDescriptor } from '../../../shared/ipc'

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

export function getKernApi(): KernApi {
  if (typeof window === 'undefined' || !window.kernAPI) {
    throw new Error('kernAPI unavailable (preload not loaded)')
  }
  return window.kernAPI
}
