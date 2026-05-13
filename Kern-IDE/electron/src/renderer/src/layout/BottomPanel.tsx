import { Suspense, lazy, useMemo } from 'react'
import { useWorkbench } from '../state/WorkbenchContext'

const ProblemsPanel = lazy(() => import('../panels/ProblemsPanel'))

export function BottomPanel() {
  const { panelTab, setPanelTab, outputLines } = useWorkbench()

  const tabBtn = useMemo(
    () =>
      (id: 'output' | 'problems', label: string) => {
        const active = panelTab === id
        return (
          <button
            key={id}
            type="button"
            onClick={() => setPanelTab(id)}
            style={{
              padding: '6px 14px',
              border: 'none',
              borderBottom: active ? '2px solid var(--kern-accent)' : '2px solid transparent',
              background: 'transparent',
              color: active ? 'var(--kern-fg)' : 'var(--kern-fg-muted)',
              fontWeight: active ? 600 : 400
            }}
          >
            {label}
          </button>
        )
      },
    [panelTab, setPanelTab]
  )

  return (
    <div
      style={{
        flexShrink: 0,
        height: 200,
        borderTop: '1px solid var(--kern-border)',
        display: 'flex',
        flexDirection: 'column',
        background: 'var(--kern-bg)'
      }}
    >
      <div style={{ display: 'flex', borderBottom: '1px solid var(--kern-border)' }}>
        {tabBtn('output', 'Output')}
        {tabBtn('problems', 'Problems')}
      </div>
      <div style={{ flex: 1, minHeight: 0, overflow: 'hidden' }}>
        {panelTab === 'output' ? (
          <pre
            className="kern-scrollbar"
            style={{
              margin: 0,
              padding: 8,
              height: '100%',
              overflow: 'auto',
              fontSize: 12,
              fontFamily: 'var(--kern-mono)',
              color: 'var(--kern-fg-muted)'
            }}
          >
            {outputLines.length === 0
              ? 'Output from Kern tooling will appear here.\n'
              : outputLines.join('\n')}
          </pre>
        ) : (
          <Suspense
            fallback={<div style={{ padding: 8, color: 'var(--kern-fg-muted)' }}>Loading…</div>}
          >
            <ProblemsPanel />
          </Suspense>
        )}
      </div>
    </div>
  )
}
