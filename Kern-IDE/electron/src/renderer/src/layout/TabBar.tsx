import { useWorkbench } from '../state/WorkbenchContext'

export function TabBar() {
  const { tabs, activePath, selectTab, closeTab } = useWorkbench()

  if (tabs.length === 0) return null

  return (
    <div
      style={{
        display: 'flex',
        flexShrink: 0,
        minHeight: 35,
        background: 'var(--kern-tab-inactive)',
        borderBottom: '1px solid var(--kern-border)',
        overflowX: 'auto',
        alignItems: 'flex-end'
      }}
      className="kern-scrollbar"
    >
      {tabs.map((t) => {
        const active = t.path === activePath
        return (
          <div
            key={t.path}
            role="tab"
            aria-selected={active}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: 6,
              padding: '8px 10px',
              minWidth: 120,
              maxWidth: 220,
              background: active ? 'var(--kern-tab-active)' : 'transparent',
              borderRight: '1px solid var(--kern-border)',
              borderTop: active ? '2px solid var(--kern-accent)' : '2px solid transparent',
              cursor: 'pointer',
              userSelect: 'none'
            }}
            onClick={() => selectTab(t.path)}
          >
            <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
              {t.dirty ? '● ' : ''}
              {t.name}
            </span>
            <button
              type="button"
              title="Close"
              style={{ padding: '0 4px', opacity: 0.7 }}
              onClick={(e) => {
                e.stopPropagation()
                closeTab(t.path)
              }}
            >
              ×
            </button>
          </div>
        )
      })}
    </div>
  )
}
