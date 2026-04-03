import { useVirtualizer } from '@tanstack/react-virtual'
import { useRef } from 'react'
import { useWorkbench } from '../state/WorkbenchContext'

export default function ProblemsPanel() {
  const { problems, openFile } = useWorkbench()
  const parentRef = useRef<HTMLDivElement>(null)

  const virtualizer = useVirtualizer({
    count: problems.length,
    getScrollElement: () => parentRef.current,
    estimateSize: () => 40,
    overscan: 10
  })

  if (problems.length === 0) {
    return <div style={{ padding: 12, color: 'var(--kern-fg-muted)' }}>No problems</div>
  }

  return (
    <div ref={parentRef} className="kern-scrollbar" style={{ height: '100%', overflow: 'auto' }}>
      <div style={{ height: virtualizer.getTotalSize(), position: 'relative' }}>
        {virtualizer.getVirtualItems().map((vi) => {
          const pr = problems[vi.index]
          if (!pr) return null
          return (
            <button
              key={`${pr.path}-${vi.index}`}
              type="button"
              style={{
                position: 'absolute',
                top: 0,
                left: 0,
                width: '100%',
                height: vi.size,
                transform: `translateY(${vi.start}px)`,
                textAlign: 'left',
                padding: '6px 10px',
                border: 'none',
                borderBottom: '1px solid var(--kern-border)',
                background: 'transparent',
                color: 'var(--kern-fg)',
                cursor: 'pointer',
                fontSize: 12
              }}
              onClick={() => void openFile(pr.path)}
            >
              <div style={{ fontFamily: 'var(--kern-mono)', opacity: 0.85 }}>{pr.path}</div>
              <div>{pr.message}</div>
            </button>
          )
        })}
      </div>
    </div>
  )
}
