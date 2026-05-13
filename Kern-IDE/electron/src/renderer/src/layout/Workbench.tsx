import { ActivityBar } from './ActivityBar'
import { TabBar } from './TabBar'
import { BottomPanel } from './BottomPanel'
import { FileTree } from '../components/FileTree'
import { EditorArea } from '../components/EditorArea'
import { useWorkbench } from '../state/WorkbenchContext'

export function Workbench() {
  const { sidebarVisible, panelVisible } = useWorkbench()

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      <ActivityBar />
      {sidebarVisible && (
        <aside
          style={{
            width: 260,
            flexShrink: 0,
            borderRight: '1px solid var(--kern-border)',
            display: 'flex',
            flexDirection: 'column',
            background: 'var(--kern-surface)',
            minWidth: 0
          }}
        >
          <div
            style={{
              padding: '8px 12px',
              fontSize: 11,
              fontWeight: 600,
              textTransform: 'uppercase',
              letterSpacing: '0.06em',
              color: 'var(--kern-fg-muted)',
              borderBottom: '1px solid var(--kern-border)'
            }}
          >
            Explorer
          </div>
          <div style={{ flex: 1, minHeight: 0 }}>
            <FileTree />
          </div>
        </aside>
      )}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0, minHeight: 0 }}>
        <TabBar />
        <div style={{ flex: 1, minHeight: 0 }}>
          <EditorArea />
        </div>
        {panelVisible && <BottomPanel />}
      </div>
    </div>
  )
}
