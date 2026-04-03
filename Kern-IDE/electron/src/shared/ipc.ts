/** IPC channel names — keep in sync with preload + renderer. */
export const IPC = {
  /** Dialog: open folder → string | null */
  DIALOG_OPEN_FOLDER: 'kern:dialog:openFolder',
  /** Read single directory (non-recursive) */
  FS_READ_DIR: 'kern:fs:readDir',
  FS_READ_FILE: 'kern:fs:readFile',
  FS_WRITE_FILE: 'kern:fs:writeFile',
  FS_STAT: 'kern:fs:stat',
  /** Heavy recursive listing in worker thread */
  FS_SCAN: 'kern:fs:scan',
  WATCH_START: 'kern:watch:start',
  WATCH_STOP: 'kern:watch:stop',
  /** Main → renderer push */
  WATCH_EVENT: 'kern:watch:event',
  COMMANDS_LIST: 'kern:commands:list',
  COMMANDS_EXECUTE: 'kern:commands:execute',
  THEME_GET: 'kern:theme:get',
  THEME_SET: 'kern:theme:set',
  THEME_LIST: 'kern:theme:list',
  /** Optional: run kern binary (future) */
  KERN_RUN: 'kern:run:script'
} as const

export type WatchEventPayload = {
  type: 'add' | 'change' | 'unlink' | 'addDir' | 'unlinkDir'
  path: string
  root: string
}

export type DirEntry = {
  name: string
  path: string
  isDirectory: boolean
}

export type CommandDescriptor = {
  id: string
  title: string
  category?: string
  source?: 'core' | 'extension'
  extensionId?: string
}
