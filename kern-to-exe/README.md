# kern-to-exe

**kern-to-exe** is a small graphical tool (tkinter, no extra pip deps) for packaging **Kern** `.kn` programs into a **single native `.exe`**: script path, single-file output, console vs windowed mode, optional icon, embedded assets, pre/post build hooks, and a live build log.

## Requirements

- Python 3.10+ with tkinter (standard on Windows “full” Python).
- **kernc** — the Kern standalone compiler (`build/Release/kernc.exe` after building the repo), or set **`KERNC_EXE`** to its full path.

## Run

From the repository root:

```bat
cd kern-to-exe
python -m kern_to_exe
```

Or double-click **`kern-to-exe.bat`** (same as above).

### Headless / CI build

From the repo root (or any cwd), with `kernc` on `PATH` or **`KERNC_EXE`** set:

```bat
python -m kern_to_exe --recipe path\to\project.kern2exe.json
```

Or pass explicit fields instead of a recipe:

```bat
python -m kern_to_exe --entry app.kn --output out\app.exe --project-root .
```

Optional flags: `--no-release`, `--opt 2`, `--no-console`, `--force`, `--machine-json`, `--diagnostics-json PATH`, `--kern-repo-root PATH`, repeatable `--extra-kn`, `--asset`, `--pre-build`, `--post-build`. Same entry point: `python -m kern_to_exe.cli …`.

## What it does

1. Writes **`<project_root>/.kern-to-exe/kernconfig.json`** from your UI choices.
2. Runs: `kernc --config ...` (optional `--build-diagnostics-json`, `--json`).
3. Optional **force rebuild** deletes the previous output `.exe` and **`.kern-cache`** beside it.

## Recipe files

Save/load **`.kern2exe.json`** from the File menu to store all fields (entry, output, assets, icon, commands, etc.).

## Kern language: web-oriented helpers

Globals and **`import("web")`** / **`import("net")`** include helpers for HTML, CSS, JS literals, URLs, and HTTP:

- **`html_escape` / `html_unescape` / `strip_html` / `xml_escape`** — text safety and rough tag stripping.
- **`css_escape` / `js_escape`** — safe embedding in CSS quoted values and JS/JSON string literals.
- **`build_query(map)`** — `application/x-www-form-urlencoded` query string (keys sorted); pass a real map (e.g. from `json_parse("{...}")`), not the `map()` array function.
- **`url_resolve(base, relative)`**, **`mime_type_guess(path)`**, **`parse_data_url(s)`** (`ok`, `mime`, `is_base64`, `data`).
- **`http_parse_response(raw)`**, **`url_parse`**, **`parse_query`**, **`url_encode` / `url_decode`**, **`http_get`**, etc.
- **`parse_cookie_header`**, **`set_cookie_fields`**, **`content_type_charset`**, **`is_safe_http_redirect`** — cookies, charset from `Content-Type`, and same-host redirect checks for `Location`-style URLs.
- **`http_parse_request`**, **`parse_link_header`**, **`parse_content_disposition`** — raw HTTP/1.x request line + headers, RFC 5988 `Link`, and `Content-Disposition` (quoted `filename` with `;` supported).
- **`url_normalize`**, **`css_url_escape`**, **`html_sanitize_strict(html, allow_tags)`** — URL cleanup for http(s), percent-encoding for `url(...)`, and allowlist-only tags (attributes stripped; `script`/`style`/`iframe` blocks removed).
- **`http_build_response`**, **`html_nl2br`**, **`url_path_join`**, **`merge_query`**, **`parse_accept_language`**, **`parse_authorization_basic`** — raw response lines, line breaks in HTML, path joining, query merging, `Accept-Language`, and HTTP Basic auth decoding.

Standalone **Windows icon**: set **`icon`** in kernconfig (absolute path to `.ico`); kernc embeds it via a generated resource file when building the exe.
