import type { CSSProperties } from 'react'
import { useEffect, useMemo, useRef, useState } from 'react'
import { useVirtualizer } from '@tanstack/react-virtual'
import type { CommandDescriptor } from '../../../shared/ipc'
import { getKernApi } from '../kern/kern-api'

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
    score += 100 - (idx - ti)
    ti = idx + 1
  }
  return score
}

export function CommandPalette({ open, onClose }: Props) {
  const [query, setQuery] = useState('')
  const [all, setAll] = useState<CommandDescriptor[]>([])
  const inputRef = useRef<HTMLInputElement>(null)
  const listRef = useRef<HTMLDivElement>(null)
  const api = getKernApi()

  useEffect(() => {
    if (!open) return
    setQuery('')
    void api.listCommands().then(setAll)
    setTimeout(() => inputRef.current?.focus(), 0)
  }, [open, api])

  const filtered = useMemo(() => {
    const scored = all
      .map((c) => ({
        c,
        s: fuzzyScore(query, `${c.title} ${c.id} ${c.category ?? ''}`)
      }))
      .filter((x) => x.s > 0)
      .sort((a, b) => b.s - a.s)
    return scored.map((x) => x.c)
  }, [all, query])

  const virtualizer = useVirtualizer({
    count: filtered.length,
    getScrollElement: () => listRef.current,
    estimateSize: () => 32,
    overscan: 8
  })

  const [sel, setSel] = useState(0)
  useEffect(() => {
    setSel(0)
  }, [query, open])

  const run = async (id: string) => {
    await api.executeCommand(id)
    onClose()
  }

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
        const c = filtered[sel]
        if (c) void run(c.id)
      }
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [open, filtered, sel, onClose, api])

  if (!open) return null

  return (
    <div
      className="palette-backdrop"
      style={backdrop}
      onMouseDown={(e) => {
        if (e.target === e.currentTarget) onClose()
      }}
    >
      <div className="palette-dialog" style={dialog} onMouseDown={(e) => e.stopPropagation()}>
        <input
          ref={inputRef}
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder="Type a command name…"
          style={input}
          autoComplete="off"
          spellCheck={false}
        />
        <div ref={listRef} className="kern-scrollbar" style={{ maxHeight: 320, overflow: 'auto' }}>
          <div style={{ height: virtualizer.getTotalSize(), position: 'relative' }}>
            {virtualizer.getVirtualItems().map((vi) => {
              const c = filtered[vi.index]
              if (!c) return null
              const active = vi.index === sel
              return (
                <button
                  key={c.id}
                  type="button"
                  className="palette-item"
                  style={{
                    ...itemBase,
                    position: 'absolute',
                    top: 0,
                    left: 0,
                    width: '100%',
                    height: vi.size,
                    transform: `translateY(${vi.start}px)`,
                    background: active ? 'var(--kern-accent)' : 'transparent',
                    color: active ? '#fff' : 'var(--kern-fg)',
                    textAlign: 'left',
                    padding: '0 12px'
                  }}
                  onMouseEnter={() => setSel(vi.index)}
                  onClick={() => void run(c.id)}
                >
                  <span style={{ fontWeight: 500 }}>{c.title}</span>
                  <span style={{ marginLeft: 8, opacity: 0.65, fontSize: 11 }}>{c.category}</span>
                </button>
              )
            })}
          </div>
        </div>
      </div>
    </div>
  )
}

const backdrop: CSSProperties = {
  position: 'fixed',
  inset: 0,
  background: 'rgba(0,0,0,0.45)',
  zIndex: 10000,
  display: 'flex',
  alignItems: 'flex-start',
  justifyContent: 'center',
  paddingTop: 80
}

const dialog: React.CSSProperties = {
  width: 560,
  maxWidth: '90vw',
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

const itemBase: CSSProperties = {
  display: 'flex',
  alignItems: 'center',
  border: 'none',
  cursor: 'pointer'
}
