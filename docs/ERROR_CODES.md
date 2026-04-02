# Kern error codes

Diagnostics use stable string **codes** (e.g. `LEX-TOKENIZE`, `VM-DIV-ZERO`) for tooling and JSON output (`kern --check --json`).

## Compiler and driver

| Code | Meaning (typical) |
|------|-------------------|
| `LEX-TOKENIZE` | Lexer could not tokenize the source |
| `PARSE-SYNTAX` | Parser could not build an AST |
| `CODEGEN-UNSUPPORTED` | Codegen hit an unsupported AST node |
| `FILE-OPEN` | Could not read a source file |
| `ANAL-LOAD-GLOBAL` | Heuristic: possible undefined global (warning) |

Additional codes appear in `src/errors.hpp` / `src/main.cpp` for import scopes, internal failures, etc.

## VM registry (`src/vm/vm_error_registry.hpp`)

Registered VM failures include stable codes such as:

| Stable code | Area |
|-------------|------|
| `IMPORT-*` | Module resolution and loading |
| `BROWSERKIT-*` | BrowserKit bridge (when enabled) |
| `VM-STACK-UNDERFLOW` | Internal stack underflow |
| `VM-STEP-LIMIT` | Instruction budget exceeded |
| `VM-RETURN-OUTSIDE-FN` | Invalid `return` at runtime |
| `VM-INVALID-JUMP` | Bad control-flow target |
| `VM-INVALID-BYTECODE` | Malformed bytecode operand |
| `VM-INVALID-CALL` | Call to non-callable |
| `VM-INVALID-OP` | Invalid operation for operand types |
| `VM-DIV-ZERO` | Division or modulo by zero |

Each registry entry provides **hint** and **detail** text used in human-readable reports and JSON `hint` / `detail` fields.

## JSON runtime stack

For runtime errors, `kern --check --json` (when running semantic + compile) and runtime reporting include optional `stack` arrays. Each frame includes `function`, `line`, `column`, and **`filename`** (the source file associated with that diagnostic item).

See also: `docs/MEMORY_MODEL.md`, `docs/LANGUAGE_ROADMAP.md`.
