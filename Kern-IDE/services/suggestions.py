"""Workspace-aware path and symbol data for IDE code suggestions."""

from __future__ import annotations

from pathlib import Path

# kit entrypoint facades — keep in sync with:
# lib/kern/gamekit/module.kn
# lib/kern/browserkit/module.kn
# lib/kern/oskit/module.kn
# lib/kern/runtime/module.kn
EXTRA_MODULE_MEMBERS: dict[str, list[str]] = {
    "gamekit": [
        "use_ecs",
        "use_ecs_query",
        "use_input",
        "use_actions",
        "use_draw2d",
        "use_draw3d",
        "use_camera",
        "use_audio",
        "use_mixer",
        "use_ui",
        "use_assets",
        "use_scene",
        "use_math",
        "use_random",
        "use_physics",
        "use_network",
        "use_tasks",
        "use_debug",
        "use_loop",
        "use_core",
    ],
    "browserkit": [
        "use_http",
        "use_ws",
        "use_loader",
        "use_protocols",
        "use_cache",
        "use_dom_tree",
        "use_dom_query",
        "use_dom_events",
        "use_html",
        "use_css_parser",
        "use_style_engine",
        "use_js_runtime",
        "use_plugins",
        "use_layout",
        "use_painter",
        "use_incremental_renderer",
        "use_window_tabs",
        "use_events",
        "use_storage",
        "use_security",
        "use_csp",
    ],
    "oskit": [
        "use_kernel",
        "use_asm",
        "use_port",
        "use_serial",
        "use_heap",
        "use_paging",
        "use_interrupts",
        "use_irq_controller",
        "use_threads",
        "use_sync",
        "use_screen",
        "use_keyboard",
        "use_timer",
        "use_disk",
        "use_fs",
        "use_path",
        "use_boot",
    ],
    "runtime": [
        "use_memory",
        "use_sync",
        "use_time",
    ],
}

# common lib/kern/*.kn single-file imports (alias = file stem). Curated public-style names.
STDLIB_MODULE_MEMBERS: dict[str, list[str]] = {
    "string_utils": [
        "contains",
        "lines",
        "words",
        "trim_each",
        "count_sub",
        "remove_prefix",
        "remove_suffix",
        "split_n",
        "pad_center",
        "is_empty",
        "first_line",
        "last_line",
        "replace_all",
        "repeat_str",
    ],
    "algo": [
        "binary_search",
        "gcd",
        "lcm",
        "merge",
        "merge_sort",
        "quicksort",
        "is_prime",
        "clamp_val",
        "sum_arr",
        "product_arr",
        "min_arr",
        "max_arr",
        "factorial",
        "fibonacci",
        "is_palindrome",
        "reverse_string",
        "shuffle",
        "lerp",
    ],
    "list_utils": [
        "last_index_of",
        "rotate_left",
        "rotate_right",
        "take_while",
        "drop_while",
        "intersperse",
        "all_indexes",
        "zip_with",
        "flatten_one",
        "unzip",
        "group_by",
        "partition",
        "find_index",
        "split_at",
    ],
    "dict_utils": [
        "get_default",
        "set_default",
        "invert",
        "from_pairs",
        "pick",
        "omit",
        "keys_sorted",
        "merge_shallow",
        "values_list",
        "filter_keys",
        "map_values",
    ],
    "path_utils": [
        "path_split_ext",
        "path_has_ext",
        "path_stem",
        "path_join_all",
    ],
    "html_template": [
        "render",
    ],
    "http_shortcuts": [
        "json_response",
        "text_response",
        "html_response",
    ],
    "testing": [
        "run_test",
        "assert_throws",
        "assert_throws_msg",
        "run_tests",
        "assert_true",
        "assert_true_msg",
        "assert_near",
        "assert_near_eps",
        "skip_test",
        "test_suite",
        "expect_true",
        "expect_false",
        "expect_null",
        "expect_not_null",
    ],
}

# common Kern prelude-adjacent names for 1-char / fuzzy-style prefix completion.
EXTRA_BUILTINS: dict[str, str] = {
    "html_escape": "Escape string for safe HTML text (XSS-safe embedding)",
    "html_unescape": "Decode common HTML entities (&amp;, &#…;, &nbsp;, etc.)",
    "strip_html": "Remove HTML/XML tags and <!-- comments --> (rough text extract)",
    "xml_escape": "Escape &, <, >, quotes for XML/text nodes",
    "css_escape": "Escape string for safe embedding in CSS quoted values",
    "js_escape": "Escape string for JavaScript/JSON double-quoted literals",
    "build_query": "Build application/x-www-form-urlencoded query from string map (sorted keys)",
    "url_resolve": "Resolve a relative URL against an absolute base (RFC-style)",
    "mime_type_guess": "Guess Content-Type from file path extension",
    "parse_data_url": "Parse data: URL → ok, mime, is_base64, data",
    "parse_cookie_header": "Parse Cookie header → map of name → value (split on ;)",
    "set_cookie_fields": "Build one Set-Cookie line from map (name, value, path, …)",
    "content_type_charset": "Extract charset from Content-Type (default utf-8)",
    "is_safe_http_redirect": "True if redirect URL stays same host as base (or relative path)",
    "http_parse_request": "Parse raw HTTP/1.x request → method, path, version, headers, body, ok",
    "http_parse_response": "Parse raw HTTP response → status, headers map, body, ok",
    "parse_link_header": "Parse HTTP Link header → array of {url, rel, type, title, …}",
    "parse_content_disposition": "Parse Content-Disposition → disposition, filename, filename_star, ok",
    "url_normalize": "Normalize absolute http(s) URL (host/scheme case, default port, path . and ..)",
    "html_sanitize_strict": "Strip dangerous blocks; keep only allowlisted tags without attributes",
    "css_url_escape": "Percent-encode string for safe use inside CSS url(...)",
    "http_build_response": "Build raw HTTP/1.1 response string (status, headers map, body)",
    "html_nl2br": "HTML-escape text and turn newlines into <br> (SSR paragraphs)",
    "url_path_join": "Join two URL path segments with a single slash",
    "parse_authorization_basic": "Decode Authorization: Basic base64 → username, password, ok",
    "merge_query": "Merge a string map into a URL query (sorted keys, URL-encoded)",
    "parse_accept_language": "Parse Accept-Language → array of {language, q} by preference",
    "assert": "Assert a condition at runtime",
    "typeof": "Inspect value type name",
    "panic": "Abort with message",
    "min": "Minimum of two numbers",
    "max": "Maximum of two numbers",
    "clamp": "Clamp value to range",
    "abs": "Absolute value",
}


def scan_workspace_module_paths(workspace_root: Path, *, max_files: int = 8000) -> list[str]:
    """Return paths relative to workspace without .kn suffix (for import(\"...\") strings)."""
    out: set[str] = set()
    n = 0
    try:
        for p in workspace_root.rglob("*.kn"):
            n += 1
            if n > max_files:
                break
            try:
                rel = p.relative_to(workspace_root)
            except ValueError:
                continue
            s = rel.as_posix()
            if s.endswith(".kn"):
                s = s[:-3]
            if s:
                out.add(s)
    except OSError:
        return sorted(out)
    return sorted(out)


def _subsequence_match(query: str, target: str) -> bool:
    if not query:
        return True
    ti = 0
    for ch in query.lower():
        found = False
        while ti < len(target):
            if target[ti].lower() == ch:
                ti += 1
                found = True
                break
            ti += 1
        if not found:
            return False
    return True


def completion_rank(prefix: str, name: str) -> tuple[int, int, int, str]:
    """
    Sort key for completions: lower tuple is better.
    (tier, secondary, tertiary, name) — tier 0 = prefix match, 1 = substring, 2 = subsequence, 99 = no match.
    """
    if not prefix:
        return (3, len(name), 0, name)
    pl, nl = prefix.lower(), name.lower()
    if nl.startswith(pl):
        return (0, len(nl), 0, name)
    if pl in nl:
        return (1, nl.index(pl), len(nl), name)
    if _subsequence_match(pl, nl):
        return (2, len(nl), 0, name)
    return (99, 0, 0, name)


def rank_named_candidates(prefix: str, names: list[str], *, limit: int = 80) -> list[str]:
    """Dynamic ordering: prefix > contains > subsequence; shorter names win within tier."""
    pairs = [(completion_rank(prefix, n), n) for n in names]
    pairs.sort(key=lambda x: x[0])
    return [n for r, n in pairs if r[0] < 99][:limit]


def filter_module_paths(paths: list[str], prefix: str, limit: int = 60) -> list[str]:
    """Prefer prefix matches, then substring, then subsequence (VS Code–style fuzzy path)."""
    if not prefix:
        return paths[:limit]
    pl = prefix.lower()
    scored: list[tuple[tuple[int, int, int, str], str]] = []
    for p in paths:
        pline = p.lower()
        tier: int
        sec: int
        ter: int
        if pline.startswith(pl):
            tier, sec, ter = 0, len(pline), 0
        elif pl in pline:
            tier, sec, ter = 1, pline.index(pl), len(pline)
        elif _subsequence_match(pl, pline):
            tier, sec, ter = 2, len(pline), 0
        else:
            continue
        scored.append(((tier, sec, ter, pline), p))
    scored.sort(key=lambda x: x[0])
    return [p for _, p in scored[:limit]]
