import { app, BrowserWindow, ipcMain, dialog, nativeTheme } from 'electron'
import { existsSync } from 'node:fs'
import { mkdir, readFile, writeFile, readdir, stat } from 'node:fs/promises'
import { join, resolve, relative, isAbsolute, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'
import { IPC, type DirEntry, type CommandDescriptor } from '../shared/ipc'
import { startWatch, stopWatch } from './services/watch-service'
import { scanWorkspaceFiles } from './services/fs-scan'
import { executeExtensionCommand, loadExtensions, type LoadedExtension } from './services/plugin-host'

const __dirname = dirname(fileURLToPath(import.meta.url))

let mainWindow: BrowserWindow | null = null
let workspaceRoot: string | null = null
let extensions: LoadedExtension[] = []

const coreCommands: CommandDescriptor[] = [
  { id: 'workbench.action.openFolder', title: 'Open Folder…', category: 'File', source: 'core' },
  { id: 'workbench.action.toggleSidebar', title: 'Toggle Sidebar', category: 'View', source: 'core' },
  { id: 'workbench.action.togglePanel', title: 'Toggle Panel', category: 'View', source: 'core' },
  { id: 'workbench.action.showCommands', title: 'Show Command Palette', category: 'View', source: 'core' },
  { id: 'workbench.action.toggleTheme', title: 'Toggle Light/Dark Theme', category: 'Preferences', source: 'core' },
  { id: 'workbench.action.quickOpen', title: 'Quick Open File', category: 'Go', source: 'core' }
]

function isUnderWorkspace(file: string): boolean {
  if (!workspaceRoot) return false
  const root = resolve(workspaceRoot)
  const target = resolve(file)
  const rel = relative(root, target)
  return rel === '' || (!rel.startsWith('..') && !isAbsolute(rel))
}

function isUnderDir(file: string, rootDir: string): boolean {
  const root = resolve(rootDir)
  const target = resolve(file)
  const rel = relative(root, target)
  return rel === '' || (!rel.startsWith('..') && !isAbsolute(rel))
}

function settingsPath(): string {
  return join(app.getPath('userData'), 'kern-ide-settings.json')
}

async function readSettings(): Promise<{ theme?: string; customThemePath?: string }> {
  try {
    const raw = await readFile(settingsPath(), 'utf8')
    return JSON.parse(raw) as { theme?: string; customThemePath?: string }
  } catch {
    return {}
  }
}

async function writeSettings(partial: Record<string, unknown>): Promise<void> {
  await mkdir(app.getPath('userData'), { recursive: true })
  const cur = await readSettings()
  await writeFile(settingsPath(), JSON.stringify({ ...cur, ...partial }, null, 2), 'utf8')
}

function extensionScanDirs(): string[] {
  const user = join(app.getPath('userData'), 'extensions')
  if (app.isPackaged) {
    const bundled = join(process.resourcesPath, 'extensions')
    return existsSync(bundled) ? [bundled, user] : [user]
  }
  return [join(app.getAppPath(), 'extensions'), user]
}

async function reloadExtensions(): Promise<void> {
  extensions = await loadExtensions(extensionScanDirs())
}

function createWindow(): void {
  mainWindow = new BrowserWindow({
    width: 1280,
    height: 800,
    minWidth: 800,
    minHeight: 500,
    show: false,
    title: 'Kern IDE',
    webPreferences: {
      preload: join(__dirname, '../preload/index.mjs'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true
    }
  })

  mainWindow.on('ready-to-show', () => mainWindow?.show())

  if (process.env['ELECTRON_RENDERER_URL']) {
    void mainWindow.loadURL(process.env['ELECTRON_RENDERER_URL'])
  } else {
    void mainWindow.loadFile(join(__dirname, '../renderer/index.html'))
  }

  mainWindow.on('closed', () => {
    mainWindow = null
    stopWatch()
  })
}

function broadcastTheme(theme: string): void {
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send('kern:theme:updated', theme)
  }
}

app.whenReady().then(async () => {
  await reloadExtensions()
  createWindow()

  ipcMain.handle(IPC.DIALOG_OPEN_FOLDER, async () => {
    const { canceled, filePaths } = await dialog.showOpenDialog(mainWindow!, {
      properties: ['openDirectory']
    })
    if (canceled || !filePaths[0]) return null
    return filePaths[0]
  })

  ipcMain.handle(IPC.FS_READ_DIR, async (_e, dir: string) => {
    if (!workspaceRoot) throw new Error('No workspace open')
    const target = resolve(dir)
    if (!isUnderWorkspace(target)) {
      throw new Error('Access denied')
    }
    const dirents = await readdir(target, { withFileTypes: true })
    const entries: DirEntry[] = dirents
      .filter((d) => !d.name.startsWith('.'))
      .map((d) => ({
        name: d.name,
        path: join(target, d.name),
        isDirectory: d.isDirectory()
      }))
      .sort((a, b) => {
        if (a.isDirectory !== b.isDirectory) return a.isDirectory ? -1 : 1
        return a.name.localeCompare(b.name)
      })
    return entries
  })

  ipcMain.handle(IPC.FS_READ_FILE, async (_e, filePath: string) => {
    const target = resolve(filePath)
    if (!isUnderWorkspace(target)) throw new Error('Access denied')
    return readFile(target, 'utf8')
  })

  ipcMain.handle(IPC.FS_WRITE_FILE, async (_e, filePath: string, content: string) => {
    const target = resolve(filePath)
    if (!isUnderWorkspace(target)) throw new Error('Access denied')
    await mkdir(dirname(target), { recursive: true })
    await writeFile(target, content, 'utf8')
  })

  ipcMain.handle(IPC.FS_STAT, async (_e, filePath: string) => {
    if (!workspaceRoot) throw new Error('No workspace open')
    const target = resolve(filePath)
    if (!isUnderWorkspace(target)) throw new Error('Access denied')
    const s = await stat(target)
    return { isDirectory: s.isDirectory(), mtime: s.mtimeMs }
  })

  ipcMain.handle(IPC.FS_SCAN, async (_e, root: string) => {
    const r = resolve(root)
    if (!isUnderWorkspace(r)) throw new Error('Access denied')
    return scanWorkspaceFiles(r, 80_000)
  })

  ipcMain.handle(IPC.WATCH_START, (_e, root: string) => {
    const r = resolve(root)
    workspaceRoot = r
    if (mainWindow) startWatch(r, mainWindow)
  })

  ipcMain.handle(IPC.WATCH_STOP, () => {
    stopWatch()
    workspaceRoot = null
  })

  ipcMain.handle(IPC.COMMANDS_LIST, async () => {
    const extCmds = extensions.flatMap((x) => x.commands)
    return [...coreCommands, ...extCmds]
  })

  ipcMain.handle(IPC.COMMANDS_EXECUTE, async (_e, id: string) => {
    if (id.includes(':')) {
      await executeExtensionCommand(id)
      return
    }
    if (mainWindow?.isDestroyed()) return
    switch (id) {
      case 'workbench.action.showCommands':
        mainWindow?.webContents.send('kern:command:showPalette')
        break
      case 'workbench.action.toggleTheme': {
        const s = await readSettings()
        const effective =
          s.theme === 'light' || s.theme === 'dark'
            ? s.theme
            : nativeTheme.shouldUseDarkColors
              ? 'dark'
              : 'light'
        const next = effective === 'dark' ? 'light' : 'dark'
        await writeSettings({ theme: next, customThemePath: undefined })
        broadcastTheme(next)
        break
      }
      default:
        mainWindow?.webContents.send('kern:command:invoke', id)
    }
  })

  ipcMain.handle(IPC.THEME_GET, async () => {
    const s = await readSettings()
    if (s.theme === 'light' || s.theme === 'dark') return s.theme
    if (s.customThemePath) return `custom:${s.customThemePath}`
    return nativeTheme.shouldUseDarkColors ? 'dark' : 'light'
  })

  ipcMain.handle(IPC.THEME_SET, async (_e, theme: string) => {
    if (theme.startsWith('custom:')) {
      await writeSettings({ theme: 'custom', customThemePath: theme.slice('custom:'.length) })
    } else if (theme === 'light' || theme === 'dark') {
      await writeSettings({ theme, customThemePath: undefined })
    }
    broadcastTheme(theme)
  })

  ipcMain.handle('kern:theme:readCustom', async (_e, themePath: string) => {
    const themesRoot = join(app.getPath('userData'), 'themes')
    const target = resolve(themePath)
    if (!isUnderDir(target, themesRoot)) throw new Error('Invalid theme path')
    return readFile(target, 'utf8')
  })

  ipcMain.handle(IPC.THEME_LIST, async () => {
    const base = [
      { id: 'light', label: 'Light' },
      { id: 'dark', label: 'Dark' }
    ]
    const dir = join(app.getPath('userData'), 'themes')
    let custom: { id: string; label: string }[] = []
    try {
      const files = await readdir(dir)
      custom = files
        .filter((f) => f.endsWith('.json'))
        .map((f) => ({
          id: `custom:${join(dir, f)}`,
          label: f.replace(/\.json$/i, '')
        }))
    } catch {
      /* none */
    }
    return [...base, ...custom]
  })

  ipcMain.handle('kern:extensions:reload', async () => {
    await reloadExtensions()
  })

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow()
  })
})

app.on('window-all-closed', () => {
  stopWatch()
  if (process.platform !== 'darwin') app.quit()
})
