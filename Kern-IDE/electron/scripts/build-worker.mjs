import * as esbuild from 'esbuild'
import { mkdir } from 'node:fs/promises'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const outFile = join(root, 'out/main/workers/scan-worker.mjs')
await mkdir(dirname(outFile), { recursive: true })

await esbuild.build({
  entryPoints: [join(root, 'src/main/workers/scan-worker.ts')],
  bundle: true,
  platform: 'node',
  format: 'esm',
  outfile: outFile,
  target: 'node20'
})

console.log('built scan-worker ->', outFile)
