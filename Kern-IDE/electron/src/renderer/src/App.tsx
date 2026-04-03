import { useCallback, useEffect, useState } from 'react'
import { Workbench } from './layout/Workbench'
import { CommandPalette } from './components/CommandPalette'
import { QuickOpen } from './components/QuickOpen'
import { getKernApi } from './kern/kern-api'
import { useWorkbench } from './state/WorkbenchContext'

export function App() {
  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <Shell />
    </div>
  )
}

function Shell() {
  const {
    setWorkspaceRoot,
    setSidebarVisible,
    setPanelVisible,
    saveActive,
    setThemeId,
    appendOutput
  } = useWorkbench()
  const [paletteOpen, setPaletteOpen] = useState(false)
  const [quickOpen, setQuickOpen] = useState(false)

  const openFolder = useCallback(async () => {
    const api = getKernApi()
    const p = await api.openFolder()
    if (!p) return
    setWorkspaceRoot(p)
    await api.watchStart(p)
    appendOutput(`Opened workspace: ${p}`)
  }, [setWorkspaceRoot, appendOutput])

  useEffect(() => {
    const api = getKernApi()
    const offCmd = api.onWorkbenchCommand((id) => {
      switch (id) {
        case 'workbench.action.openFolder':
          void openFolder()
          break
        case 'workbench.action.toggleSidebar':
          setSidebarVisible((v) => !v)
          break
        case 'workbench.action.togglePanel':
          setPanelVisible((v) => !v)
          break
        case 'workbench.action.quickOpen':
          setQuickOpen(true)
          break
        default:
          appendOutput(`Command: ${id}`)
      }
    })
    const offPal = api.onShowCommandPalette(() => setPaletteOpen(true))
    return () => {
      offCmd()
      offPal()
    }
  }, [openFolder, setSidebarVisible, setPanelVisible, appendOutput])

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const mod = e.ctrlKey || e.metaKey
      if (mod && e.shiftKey && e.key.toLowerCase() === 'p') {
        e.preventDefault()
        setPaletteOpen(true)
        return
      }
      if (mod && e.key.toLowerCase() === 'p' && !e.shiftKey) {
        e.preventDefault()
        setQuickOpen(true)
        return
      }
      if (mod && e.key.toLowerCase() === 's') {
        e.preventDefault()
        void saveActive()
      }
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [saveActive])

  return (
    <>
      <div style={{ flex: 1, minHeight: 0 }}>
        <Workbench />
      </div>
      <StatusBar onOpenFolder={() => void openFolder()} onThemeChange={(id) => void setThemeId(id)} />
      <CommandPalette open={paletteOpen} onClose={() => setPaletteOpen(false)} />
      <QuickOpen open={quickOpen} onClose={() => setQuickOpen(false)} />
    </>
  )
}

function StatusBar({
  onOpenFolder,
  onThemeChange
}: {
  onOpenFolder: () => void
  onThemeChange: (id: string) => void
}) {
  const { workspaceRoot, theme } = useWorkbench()
  const [options, setOptions] = useState<{ id: string; label: string }[]>([])

  useEffect(() => {
    void getKernApi()
      .listThemes()
      .then(setOptions)
      .catch(() => setOptions([]))
  }, [theme])

  return (
    <footer
      style={{
        flexShrink: 0,
        height: 22,
        display: 'flex',
        alignItems: 'center',
        gap: 12,
        padding: '0 10px',
        background: 'var(--kern-status)',
        color: '#fff',
        fontSize: 12
      }}
    >
      <button type="button" style={{ color: '#fff', textDecoration: 'underline' }} onClick={onOpenFolder}>
        Open folder
      </button>
      {workspaceRoot && (
        <span style={{ opacity: 0.9, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
          {workspaceRoot}
        </span>
      )}
      <span style={{ flex: 1 }} />
      <label style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
        Theme
        <select
          value={theme.startsWith('custom:') ? theme : theme === 'light' ? 'light' : 'dark'}
          onChange={(e) => onThemeChange(e.target.value)}
          style={{ fontSize: 11, maxWidth: 160 }}
        >
          {options.map((o) => (
            <option key={o.id} value={o.id}>
              {o.label}
            </option>
          ))}
        </select>
      </label>
    </footer>
  )
}
