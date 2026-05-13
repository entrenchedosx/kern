import type { CSSProperties } from 'react'
import { useEffect, useMemo, useRef, useState } from 'react'
import { useVirtualizer } from '@tanstack/react-virtual'
import { getKernApi } from '../kern/kern-api'
import { useWorkbench } from '../state/WorkbenchContext'

type Props = {
  open: boolean
  onClose: () => void
}

function fuzzyScore(query: string, text: string): number {
  if (!query) return 1
  const q = query.toLowerCase()
  const t = text.toLowerCase()
  let ti = 0
  let score = 0
  for (let i = 0; i < q.length; i++) {
    const c = q[i]!
    const idx = t.indexOf(c, ti)
    if (idx < 0) return 0
    score += 80 - (idx - ti)
    ti = idx + 1
  }
  return score
}

export function QuickOpen({ open, onClose }: Props) {
  const { workspaceRoot, openFile } = useWorkbench()
  const [query, setQuery] = useState('')
  const [paths, setPaths] = useState<string[]>([])
  const [busy, setBusy] = useState(false)
  const inputRef = useRef<HTMLInputElement>(null)
  const listRef = useRef<HTMLDivElement>(null)
  const api = getKernApi()

  useEffect(() => {
    if (!open || !workspaceRoot) return
    setQuery('')
    setBusy(true)
    void api
      .scanWorkspace(workspaceRoot)
      .then((files) => {
        const kn = files.filter((p) => p.endsWith('.kn') || p.endsWith('.md') || p.endsWith('.txt'))
        setPaths(kn.length ? kn : files)
      })
      .finally(() => setBusy(false))
    setTimeout(() => inputRef.current?.focus(), 0)
  }, [open, workspaceRoot, api])

  const filtered = useMemo(() => {
    const rel = (p: string) => {
      if (!workspaceRoot) return p
      const norm = workspaceRoot.replace(/[/\\]+$/, '')
      if (p.startsWith(norm)) return p.slice(norm.length).replace(/^[/\\]/, '')
      return p
    }
    const scored = paths
      .map((p) => ({ p, s: fuzzyScore(query, rel(p)) }))
      .filter((x) => x.s > 0)
      .sort((a, b) => b.s - a.s)
      .slice(0, 5000)
    return scored.map((x) => x.p)
  }, [paths, query, workspaceRoot])

  const virtualizer = useVirtualizer({
    count: filtered.length,
    getScrollElement: () => listRef.current,
    estimateSize: () => 28,
    overscan: 15
  })

  const [sel, setSel] = useState(0)
  useEffect(() => {
    setSel(0)
  }, [query, open])

  useEffect(() => {
    if (!open) return
    const onKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        e.preventDefault()
        onClose()
        return
      }
      if (e.key === 'ArrowDown') {
        e.preventDefault()
        setSel((i) => Math.min(i + 1, filtered.length - 1))
        return
      }
      if (e.key === 'ArrowUp') {
        e.preventDefault()
        setSel((i) => Math.max(i - 1, 0))
        return
      }
      if (e.key === 'Enter') {
        e.preventDefault()
        const p = filtered[sel]
        if (p) {
          void openFile(p).then(onClose)
        }
      }
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [open, filtered, sel, onClose, openFile])

  if (!open) return null

  return (
    <div style={backdrop} onMouseDown={(e) => e.target === e.currentTarget && onClose()}>
      <div style={dialog} onMouseDown={(e) => e.stopPropagation()}>
        <input
          ref={inputRef}
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder={workspaceRoot ? 'Search files by name…' : 'Open a folder first'}
          style={input}
          disabled={!workspaceRoot || busy}
        />
        <div ref={listRef} className="kern-scrollbar" style={{ maxHeight: 400, overflow: 'auto' }}>
          {busy ? (
            <div style={{ padding: 16, color: 'var(--kern-fg-muted)' }}>Indexing workspace…</div>
          ) : (
            <div style={{ height: virtualizer.getTotalSize(), position: 'relative' }}>
              {virtualizer.getVirtualItems().map((vi) => {
                const p = filtered[vi.index]
                if (!p) return null
                const active = vi.index === sel
                const label = workspaceRoot
                  ? p.replace(workspaceRoot.replace(/[/\\]+$/, ''), '').replace(/^[/\\]/, '') || p
                  : p
                return (
                  <button
                    key={p}
                    type="button"
                    className="palette-item"
                    style={{
                      position: 'absolute',
                      top: 0,
                      left: 0,
                      width: '100%',
                      height: vi.size,
                      transform: `translateY(${vi.start}px)`,
                      textAlign: 'left',
                      padding: '4px 12px',
                      border: 'none',
                      cursor: 'pointer',
                      background: active ? 'var(--kern-accent)' : 'transparent',
                      color: active ? '#fff' : 'var(--kern-fg)',
                      fontSize: 13,
                      fontFamily: 'var(--kern-mono)'
                    }}
                    onMouseEnter={() => setSel(vi.index)}
                    onClick={() => void openFile(p).then(onClose)}
                  >
                    {label}
                  </button>
                )
              })}
            </div>
          )}
        </div>
      </div>
    </div>
  )
}

const backdrop: CSSProperties = {
  position: 'fixed',
  inset: 0,
  background: 'rgba(0,0,0,0.45)',
  zIndex: 9999,
  display: 'flex',
  alignItems: 'flex-start',
  justifyContent: 'center',
  paddingTop: 60
}

const dialog: React.CSSProperties = {
  width: 640,
  maxWidth: '92vw',
  background: 'var(--kern-surface)',
  border: '1px solid var(--kern-border)',
  borderRadius: 6,
  boxShadow: '0 16px 48px rgba(0,0,0,0.35)',
  overflow: 'hidden'
}

const input: CSSProperties = {
  width: '100%',
  padding: '12px 14px',
  border: 'none',
  borderBottom: '1px solid var(--kern-border)',
  background: 'var(--kern-bg)',
  color: 'var(--kern-fg)',
  fontSize: 14,
  outline: 'none'
}
