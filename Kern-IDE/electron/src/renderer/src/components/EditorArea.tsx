import Editor from '@monaco-editor/react'
import type { CSSProperties } from 'react'
import { useCallback, useEffect, useRef, useState } from 'react'
import type { editor } from 'monaco-editor'
import { getKernApi } from '../kern/kern-api'
import { useWorkbench } from '../state/WorkbenchContext'

export function EditorArea() {
  const { activePath, markDirty, theme } = useWorkbench()
  const [value, setValue] = useState('')
  const edRef = useRef<editor.IStandaloneCodeEditor | null>(null)
  const api = getKernApi()

  useEffect(() => {
    if (!activePath) {
      setValue('')
      return
    }
    let cancelled = false
    void api.readFile(activePath).then((c) => {
      if (!cancelled) setValue(c)
    })
    return () => {
      cancelled = true
    }
  }, [activePath, api])

  useEffect(() => {
    const fn = (e: Event) => {
      const ce = e as CustomEvent<{ path: string }>
      const path = ce.detail?.path
      if (!path || path !== activePath || !edRef.current) return
      void api.writeFile(path, edRef.current.getValue()).then(() => markDirty(path, false))
    }
    window.addEventListener('kern-save-active', fn)
    return () => window.removeEventListener('kern-save-active', fn)
  }, [activePath, api, markDirty])

  const onChange = useCallback(
    (v: string | undefined) => {
      setValue(v ?? '')
      if (activePath) markDirty(activePath, true)
    },
    [activePath, markDirty]
  )

  const monacoTheme = theme.startsWith('custom:') || theme === 'dark' ? 'kern-dark' : 'kern-light'

  if (!activePath) {
    return (
      <div className="editor-empty kern-scrollbar" style={emptyStyle}>
        <p style={{ margin: 0, color: 'var(--kern-fg-muted)' }}>
          Open a workspace folder, then pick a file from the explorer.
        </p>
        <p style={{ margin: '12px 0 0', fontSize: '12px', color: 'var(--kern-fg-muted)' }}>
          <kbd>Ctrl+Shift+P</kbd> command palette · <kbd>Ctrl+P</kbd> quick open · <kbd>Ctrl+S</kbd> save
        </p>
      </div>
    )
  }

  return (
    <Editor
      key={activePath}
      height="100%"
      path={activePath}
      language="kern"
      theme={monacoTheme}
      value={value}
      onChange={onChange}
      onMount={(ed) => {
        edRef.current = ed
      }}
      options={{
        fontFamily: 'var(--kern-mono)',
        fontSize: 14,
        minimap: { enabled: true },
        scrollBeyondLastLine: false,
        automaticLayout: true,
        tabSize: 2,
        wordWrap: 'on'
      }}
    />
  )
}

const emptyStyle: CSSProperties = {
  height: '100%',
  display: 'flex',
  flexDirection: 'column',
  alignItems: 'center',
  justifyContent: 'center',
  padding: 24,
  textAlign: 'center'
}
