import type { CSSProperties } from 'react'
import { useWorkbench } from '../state/WorkbenchContext'

export function ActivityBar() {
  const { sidebarVisible, setSidebarVisible, panelVisible, setPanelVisible } = useWorkbench()

  const btn = (active: boolean): CSSProperties => ({
    width: 48,
    height: 48,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    borderLeft: active ? '2px solid var(--kern-accent)' : '2px solid transparent',
    opacity: active ? 1 : 0.75
  })

  return (
    <aside
      style={{
        width: 48,
        flexShrink: 0,
        background: 'var(--kern-activity)',
        borderRight: '1px solid var(--kern-border)',
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'stretch'
      }}
    >
      <button
        type="button"
        title="Explorer"
        style={btn(sidebarVisible)}
        onClick={() => setSidebarVisible(!sidebarVisible)}
      >
        📁
      </button>
      <button
        type="button"
        title="Toggle bottom panel"
        style={btn(panelVisible)}
        onClick={() => setPanelVisible(!panelVisible)}
      >
        ⬍
      </button>
    </aside>
  )
}
