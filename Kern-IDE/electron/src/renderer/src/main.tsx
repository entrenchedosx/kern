import './monaco/setupMonaco'
import './styles/global.css'
import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import { WorkbenchProvider } from './state/WorkbenchContext'
import { App } from './App'

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <WorkbenchProvider>
      <App />
    </WorkbenchProvider>
  </StrictMode>
)
