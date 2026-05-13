# ai.prompts

Prompt tooling for template rendering and guarded model output handling.

## API

- `render(templateText, vars)`
- `build_messages(systemPrompt, userPrompt)`
- `extract_json(text)`
- `ensure_max_chars(text, maxChars)`
- `run_template(client, templateText, vars, options)`

## Dependency

- `ai.client`

## Quick use

```kn
let prompts = import("ai.prompts")
let txt = prompts["render"]("Write release notes for {{version}}", { "version": "1.0.0" })
print(txt)
```
