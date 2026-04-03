/**
 * Kern IDE extension (ESM). Loaded dynamically from the main process.
 * @param {{ registerCommand: (id: string, handler: () => void | Promise<void>) => void }} api
 */
export function activate(api) {
  api.registerCommand('sample.hello', () => {
    console.info('[kern extension:sample] Hello from Kern IDE extensions.')
  })
}
