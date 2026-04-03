import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import { useVirtualizer } from '@tanstack/react-virtual'
import type { DirEntry } from '../../../shared/ipc'
import { getKernApi } from '../kern/kern-api'
import { useWorkbench } from '../state/WorkbenchContext'

type Row =
  | { kind: 'dir'; path: string; name: string; depth: number; expanded: boolean; loaded: boolean }
  | { kind: 'file'; path: string; name: string; depth: number }

export function FileTree() {
  const { workspaceRoot, openFile, explorerEpoch } = useWorkbench()
  const [expanded, setExpanded] = useState<Set<string>>(() => new Set())
  const [childrenCache, setChildrenCache] = useState<Map<string, DirEntry[]>>(new Map())
  const api = getKernApi()
  const parentRef = useRef<HTMLDivElement>(null)

  const loadChildren = useCallback(
    async (dir: string) => {
      const entries = await api.readDir(dir)
      setChildrenCache((m) => new Map(m).set(dir, entries))
    },
    [api]
  )

  useEffect(() => {
    if (!workspaceRoot) {
      setExpanded(new Set())
      setChildrenCache(new Map())
      return
    }
    void loadChildren(workspaceRoot)
    setExpanded(new Set([workspaceRoot]))
  }, [workspaceRoot, explorerEpoch, loadChildren])

  const rows = useMemo(() => {
    if (!workspaceRoot) return [] as Row[]
    const out: Row[] = []
    const walk = (dir: string, depth: number) => {
      const list = childrenCache.get(dir)
      const isEx = expanded.has(dir)
      const name = dir === workspaceRoot ? dir.split(/[/\\]/).filter(Boolean).pop() ?? dir : dir.split(/[/\\]/).pop() ?? dir
      out.push({
        kind: 'dir',
        path: dir,
        name,
        depth,
        expanded: isEx,
        loaded: !!list
      })
      if (!isEx || !list) return
      for (const e of list) {
        if (e.isDirectory) walk(e.path, depth + 1)
        else out.push({ kind: 'file', path: e.path, name: e.name, depth: depth + 1 })
      }
    }
    walk(workspaceRoot, 0)
    return out
  }, [workspaceRoot, childrenCache, expanded])

  const virtualizer = useVirtualizer({
    count: rows.length,
    getScrollElement: () => parentRef.current,
    estimateSize: () => 22,
    overscan: 12
  })

  const toggle = async (path: string) => {
    setExpanded((prev) => {
      const next = new Set(prev)
      if (next.has(path)) next.delete(path)
      else next.add(path)
      return next
    })
    if (!childrenCache.has(path)) await loadChildren(path)
  }

  if (!workspaceRoot) {
    return <div style={{ padding: 8, color: 'var(--kern-fg-muted)' }}>No folder open</div>
  }

  return (
    <div
      ref={parentRef}
      className="kern-scrollbar"
      style={{ height: '100%', overflow: 'auto', padding: '4px 0' }}
    >
      <div style={{ height: virtualizer.getTotalSize(), position: 'relative' }}>
        {virtualizer.getVirtualItems().map((vi) => {
          const row = rows[vi.index]
          if (!row) return null
          const pad = row.depth * 12 + 4
          if (row.kind === 'dir') {
            return (
              <div
                key={row.path}
                className="palette-item"
                role="treeitem"
                aria-expanded={row.expanded}
                style={{
                  position: 'absolute',
                  top: 0,
                  left: 0,
                  width: '100%',
                  height: vi.size,
                  transform: `translateY(${vi.start}px)`,
                  paddingLeft: pad,
                  display: 'flex',
                  alignItems: 'center',
                  gap: 4,
                  cursor: 'pointer',
                  userSelect: 'none',
                  fontSize: 13
                }}
                onClick={() => void toggle(row.path)}
              >
                <span style={{ width: 12, opacity: 0.7 }}>{row.expanded ? '▼' : '▶'}</span>
                <span>{row.name}</span>
              </div>
            )
          }
          return (
            <div
              key={row.path}
              className="palette-item"
              role="treeitem"
              style={{
                position: 'absolute',
                top: 0,
                left: 0,
                width: '100%',
                height: vi.size,
                transform: `translateY(${vi.start}px)`,
                paddingLeft: pad + 16,
                display: 'flex',
                alignItems: 'center',
                cursor: 'pointer',
                fontSize: 13
              }}
              onClick={() => void openFile(row.path)}
              onDoubleClick={() => void openFile(row.path)}
            >
              {row.name}
            </div>
          )
        })}
      </div>
    </div>
  )
}
