"""
VM built-in stdlib module exports — keep in sync with createStdlibModule() in src/stdlib_modules.cpp.
Used for completion, hovers, and synthesized "source" views for embedded modules.
"""

from __future__ import annotations

# keys must match isStdlibModuleName() + MODULES map in stdlib_modules.cpp
STDLIB_EXPORTS: dict[str, list[str]] = {
    "math": [
        "sqrt", "pow", "sin", "cos", "tan", "floor", "ceil", "round", "round_to", "abs", "min", "max",
        "clamp", "lerp", "log", "atan2", "sign", "deg_to_rad", "rad_to_deg", "PI", "E", "TAU",
    ],
    "string": [
        "upper", "lower", "replace", "join", "split", "trim", "starts_with", "ends_with",
        "repeat", "pad_left", "pad_right", "split_lines", "format", "len", "regex_match", "regex_replace",
        "chr", "ord", "hex", "bin", "hash_fnv", "escape_regex",
    ],
    "json": ["json_parse", "json_stringify"],
    "net": ["url_encode", "url_decode", "http_get", "http_request", "http_post", "url_parse", "parse_query"],
    "data": ["toml_parse", "toml_stringify", "csv_parse", "csv_stringify"],
    "random": ["random", "random_int", "random_choice", "random_shuffle"],
    "sys": [
        "cli_args", "print", "panic", "error_message", "Error", "stack_trace", "stack_trace_array", "assertType",
        "error_name", "error_cause", "ValueError", "TypeError", "RuntimeError", "OSError", "KeyError", "is_error_type",
        "format_exception", "error_traceback", "invoke", "extend_array", "with_cleanup",
        "repr", "kern_version", "platform", "os_name", "arch", "exit_code", "uuid", "env_all", "env_get",
        "cwd", "chdir", "hostname", "cpu_count", "getpid", "monotonic_time", "env_set",
        "set_step_limit", "set_max_call_depth", "set_callback_guard", "deterministic_mode", "runtime_info",
    ],
    "io": [
        "read_file", "write_file", "readFile", "writeFile", "readline", "base64_encode", "base64_decode",
        "csv_parse", "csv_stringify", "fileExists", "listDir", "file_size", "glob",
        "listDirRecursive", "create_dir", "is_file", "is_dir", "copy_file", "delete_file", "move_file",
    ],
    "array": [
        "array", "len", "push", "push_front", "slice", "map", "filter", "reduce", "reverse", "find",
        "sort", "flatten", "flat_map", "zip", "chunk", "unique", "first", "last", "take", "drop",
        "sort_by", "copy", "cartesian", "window", "deep_equal", "insert_at", "remove_at",
    ],
    "env": ["env_get", "env_all", "env_set"],
    "map": ["keys", "values", "has", "merge", "deep_equal", "copy"],
    "types": [
        "str", "int", "float", "parse_int", "parse_float", "is_nan", "is_inf",
        "is_string", "is_array", "is_map", "is_number", "is_function", "type",
    ],
    "debug": ["inspect", "type", "dir", "stack_trace", "assert_eq"],
    "log": ["log_info", "log_warn", "log_error", "log_debug"],
    "time": ["time", "sleep", "time_format", "monotonic_time"],
    "memory": [
        "alloc", "free", "ptr_address", "ptr_from_address", "ptr_offset",
        "peek8", "peek16", "peek32", "peek64", "peek8s", "peek16s", "peek32s", "peek64s",
        "poke8", "poke16", "poke32", "poke64",
        "peek_float", "poke_float", "peek_double", "poke_double",
        "mem_copy", "mem_set", "mem_cmp", "mem_move", "mem_swap", "realloc",
        "align_up", "align_down", "ptr_align_up", "ptr_align_down",
        "memory_barrier",
        "volatile_load8", "volatile_store8", "volatile_load16", "volatile_store16",
        "volatile_load32", "volatile_store32", "volatile_load64", "volatile_store64",
        "bytes_read", "bytes_write", "ptr_is_null", "size_of_ptr",
        "ptr_add", "ptr_sub", "is_aligned", "mem_set_zero", "ptr_tag", "ptr_untag", "ptr_get_tag",
        "struct_define", "offsetof_struct", "sizeof_struct",
        "pool_create", "pool_alloc", "pool_free", "pool_destroy",
        "read_be16", "read_be32", "read_be64", "write_be16", "write_be32", "write_be64",
        "dump_memory", "alloc_tracked", "free_tracked", "get_tracked_allocations",
        "atomic_load32", "atomic_store32", "atomic_add32", "atomic_cmpxchg32",
        "map_file", "unmap_file", "memory_protect",
        "read_le16", "read_le32", "read_le64", "write_le16", "write_le32", "write_le64", "alloc_zeroed",
        "ptr_eq", "alloc_aligned", "string_to_bytes", "bytes_to_string",
        "memory_page_size", "mem_find", "mem_fill_pattern",
        "ptr_compare", "mem_reverse",
        "mem_scan", "mem_overlaps", "get_endianness",
        "mem_is_zero", "read_le_float", "write_le_float",
        "read_le_double", "write_le_double",
        "mem_count", "ptr_min", "ptr_max", "ptr_diff",
        "read_be_float", "write_be_float", "read_be_double", "write_be_double", "ptr_in_range",
        "mem_xor", "mem_zero",
    ],
    "util": ["range", "default", "merge", "all", "any", "vec2", "vec3", "rand_vec2", "rand_vec3"],
    "profiling": ["profile_cycles", "profile_fn"],
    "path": [
        "basename", "dirname", "path_join", "cwd", "chdir", "realpath", "temp_dir",
        "read_file", "write_file", "fileExists", "listDir", "listDirRecursive",
        "create_dir", "is_file", "is_dir", "copy_file", "delete_file", "move_file", "file_size", "glob",
        "path_normalize",
    ],
    "errors": [
        "Error", "panic", "error_message", "error_name", "error_cause",
        "ValueError", "TypeError", "RuntimeError", "OSError", "KeyError", "is_error_type",
        "stack_trace", "stack_trace_array", "format_exception", "error_traceback",
    ],
    "iter": ["range", "map", "filter", "reduce", "all", "any", "cartesian", "window"],
    "collections": [
        "array", "len", "push", "push_front", "slice", "keys", "values", "has",
        "map", "filter", "reduce", "reverse", "find", "sort", "flatten", "flat_map",
        "zip", "chunk", "unique", "first", "last", "take", "drop", "sort_by",
        "copy", "merge", "deep_equal", "cartesian", "window",
    ],
    "fs": [
        "read_file", "write_file", "readFile", "writeFile", "fileExists", "listDir",
        "listDirRecursive", "create_dir", "is_file", "is_dir", "copy_file", "delete_file", "move_file",
    ],
    "regex": [
        "regex_match", "regex_replace", "regex_split", "regex_find_all", "regex_compile",
        "regex_match_pattern", "regex_replace_pattern", "escape_regex",
    ],
    "csv": ["csv_parse", "csv_stringify"],
    "b64": ["base64_encode", "base64_decode"],
    "logging": ["log_info", "log_warn", "log_error", "log_debug"],
    "hash": ["hash_fnv", "sha1", "sha256"],
    "uuid": ["uuid"],
    "os": [
        "cwd", "chdir", "getpid", "hostname", "cpu_count", "env_get", "env_all", "env_set",
        "listDir", "create_dir", "is_file", "is_dir", "temp_dir", "realpath", "which",
        "exec_args", "spawn", "wait_process", "kill_process",
    ],
    "copy": ["copy", "deep_equal"],
    "datetime": ["time", "sleep", "time_format", "monotonic_time"],
    "secrets": ["random", "random_int", "random_choice", "random_shuffle", "uuid"],
    "itools": ["range", "map", "filter", "reduce", "all", "any", "cartesian", "window"],
    "cli": ["cli_args"],
    "encoding": ["base64_encode", "base64_decode", "string_to_bytes", "bytes_to_string"],
    "run": ["cli_args", "exit_code"],
}

EMBEDDED_STDLIB_NAMES: frozenset[str] = frozenset(STDLIB_EXPORTS.keys())

# native / special modules (no .kn in repo; implemented in C++)
NATIVE_MODULE_NAMES: frozenset[str] = frozenset(
    {"g2d", "g3d", "game", "process", "input", "vision", "render", "2dgraphics"}
)


def stdlib_stub_text(module_name: str) -> str:
    """Read-only buffer content for embedded stdlib modules."""
    exports = STDLIB_EXPORTS.get(module_name, [])
    lines = [
        f'// Kern built-in module "{module_name}" (implemented in the VM — no .kn file on disk).',
        "// Python-style analogues: e.g. `types` for typing-style helpers, `math`/`json`/`re`+regex.",
        "// Exported names (from stdlib_modules.cpp):",
        "",
    ]
    if exports:
        chunk = ", ".join(exports)
        lines.append(f"//   {chunk}")
    else:
        lines.append("//   (unknown module — check src/stdlib_modules.cpp)")
    lines.extend(
        [
            "",
            f'// Usage: let m = import("{module_name}")',
            "// Reference: src/stdlib_modules.cpp → createStdlibModule()",
            "",
        ]
    )
    return "\n".join(lines)
