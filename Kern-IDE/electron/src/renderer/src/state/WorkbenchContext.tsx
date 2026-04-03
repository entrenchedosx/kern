import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
  type Dispatch,
  type ReactNode,
  type SetStateAction
} from 'react'
import * as monaco from 'monaco-editor'
import { getKernApi } from '../kern/kern-api'

export type EditorTab = {
  path: string
  name: string
  dirty: boolean
}

type WorkbenchCtx = {
  workspaceRoot: string | null
  setWorkspaceRoot: (p: string | null) => void
  sidebarVisible: boolean
  setSidebarVisible: Dispatch<SetStateAction<boolean>>
  panelVisible: boolean
  setPanelVisible: Dispatch<SetStateAction<boolean>>
  panelTab: 'output' | 'problems'
  setPanelTab: (t: 'output' | 'problems') => void
  tabs: EditorTab[]
  activePath: string | null
  openFile: (path: string) => Promise<void>
  closeTab: (path: string) => void
  selectTab: (path: string) => void
  markDirty: (path: string, dirty: boolean) => void
  saveActive: () => Promise<void>
  theme: 'light' | 'dark' | string
  setThemeId: (id: string) => Promise<void>
  outputLines: string[]
  appendOutput: (line: string) => void
  problems: { path: string; message: string }[]
  setProblems: (p: { path: string; message: string }[]) => void
  refreshExplorer: () => void
  explorerEpoch: number
}

const Ctx = createContext<WorkbenchCtx | null>(null)

export function WorkbenchProvider({ children }: { children: ReactNode }) {
  const api = getKernApi()
  const [workspaceRoot, setWorkspaceRoot] = useState<string | null>(null)
  const [sidebarVisible, setSidebarVisible] = useState(true)
  const [panelVisible, setPanelVisible] = useState(true)
  const [panelTab, setPanelTab] = useState<'output' | 'problems'>('output')
  const [tabs, setTabs] = useState<EditorTab[]>([])
  const [activePath, setActivePath] = useState<string | null>(null)
  const [theme, setTheme] = useState<string>('dark')
  const [outputLines, setOutputLines] = useState<string[]>([])
  const [problems, setProblems] = useState<{ path: string; message: string }[]>([])
  const [explorerEpoch, setExplorerEpoch] = useState(0)

  const refreshExplorer = useCallback(() => setExplorerEpoch((e) => e + 1), [])

  useEffect(() => {
    void api.getTheme().then((t) => {
      setTheme(t)
      applyDomTheme(t)
      applyMonacoTheme(t)
    })
    const off = api.onThemeUpdated((t) => {
      setTheme(t)
      applyDomTheme(t)
      applyMonacoTheme(t)
    })
    return off
  }, [api])

  useEffect(() => {
    const offWatch = api.onWatchEvent(() => {
      refreshExplorer()
    })
    return offWatch
  }, [api, refreshExplorer])

  const openFile = useCallback(async (path: string) => {
    const name = path.split(/[/\\]/).pop() ?? path
    setTabs((prev) => {
      if (prev.some((t) => t.path === path)) return prev
      return [...prev, { path, name, dirty: false }]
    })
    setActivePath(path)
    setProblems([])
  }, [])

  const closeTab = useCallback((path: string) => {
    setTabs((prev) => prev.filter((t) => t.path !== path))
  }, [])

  useEffect(() => {
    if (activePath && !tabs.some((t) => t.path === activePath)) {
      setActivePath(tabs[tabs.length - 1]?.path ?? null)
    }
  }, [tabs, activePath])

  const selectTab = useCallback((path: string) => setActivePath(path), [])

  const markDirty = useCallback((path: string, dirty: boolean) => {
    setTabs((prev) => prev.map((t) => (t.path === path ? { ...t, dirty } : t)))
  }, [])

  const saveActive = useCallback(async () => {
    if (!activePath) return
    window.dispatchEvent(new CustomEvent('kern-save-active', { detail: { path: activePath } }))
  }, [activePath])

  const setThemeId = useCallback(
    async (id: string) => {
      await api.setTheme(id)
      setTheme(id)
      applyDomTheme(id)
      applyMonacoTheme(id)
    },
    [api]
  )

  const appendOutput = useCallback((line: string) => {
    setOutputLines((prev) => [...prev.slice(-500), line])
  }, [])

  const value = useMemo(
    () => ({
      workspaceRoot,
      setWorkspaceRoot,
      sidebarVisible,
      setSidebarVisible,
      panelVisible,
      setPanelVisible,
      panelTab,
      setPanelTab,
      tabs,
      activePath,
      openFile,
      closeTab,
      selectTab,
      markDirty,
      saveActive,
      theme,
      setThemeId,
      outputLines,
      appendOutput,
      problems,
      setProblems,
      refreshExplorer,
      explorerEpoch
    }),
    [
      workspaceRoot,
      sidebarVisible,
      panelVisible,
      panelTab,
      tabs,
      activePath,
      openFile,
      closeTab,
      selectTab,
      markDirty,
      saveActive,
      theme,
      setThemeId,
      outputLines,
      appendOutput,
      problems,
      refreshExplorer,
      explorerEpoch
    ]
  )

  return <Ctx.Provider value={value}>{children}</Ctx.Provider>
}

export function useWorkbench() {
  const v = useContext(Ctx)
  if (!v) throw new Error('useWorkbench outside provider')
  return v
}

function applyDomTheme(t: string) {
  const root = document.documentElement
  if (t.startsWith('custom:')) {
    root.setAttribute('data-theme', 'dark')
    void applyCustomTheme(t.slice('custom:'.length))
    return
  }
  root.setAttribute('data-theme', t === 'light' ? 'light' : 'dark')
}

async function applyCustomTheme(themePath: string) {
  try {
    const raw = await getKernApi().readCustomTheme(themePath)
    const data = JSON.parse(raw) as { colors?: Record<string, string> }
    const root = document.documentElement
    if (data.colors) {
      for (const [k, v] of Object.entries(data.colors)) {
        root.style.setProperty(k, v)
      }
    }
  } catch {
    /* ignore */
  }
}

function applyMonacoTheme(t: string) {
  const m = t.startsWith('custom:') ? 'kern-dark' : t === 'light' ? 'kern-light' : 'kern-dark'
  monaco.editor.setTheme(m)
}
