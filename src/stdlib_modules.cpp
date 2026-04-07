#include "stdlib_modules.hpp"
#include "stdlib_stdv1_exports.hpp"
#include "vm/builtins.hpp"
#include "bytecode/value.hpp"
#include <unordered_map>
#include <vector>
#include <string>

namespace kern {

namespace {
const std::unordered_map<std::string, std::string>& stdlibBuiltinAliasTarget() {
    static const std::unordered_map<std::string, std::string> m = {
        {"readFile", "read_file"},
        {"writeFile", "write_file"},
        {"appendFile", "append_file"},
        {"file_exists", "fileExists"},
        {"list_dir", "listDir"},
    };
    return m;
}
} // namespace

ValuePtr createStdlibModule(VM& vm, const std::string& nameIn) {
    const std::string name = resolveStdlibModuleAlias(nameIn);
    const std::vector<std::string>& builtinNames = getBuiltinNames();
    auto bindExport = [&](std::unordered_map<std::string, ValuePtr>& out, const std::string& exportKey, const std::string& lookupKey) {
        std::string lookup = lookupKey;
        auto ait = stdlibBuiltinAliasTarget().find(lookupKey);
        if (ait != stdlibBuiltinAliasTarget().end()) lookup = ait->second;
        ValuePtr gv = vm.getGlobal(lookup);
        if (gv && gv->type == Value::Type::FUNCTION) {
            out[exportKey] = gv;
            return;
        }
        for (size_t i = 0; i < builtinNames.size(); ++i) {
            if (builtinNames[i] == lookup) {
                auto fn = std::make_shared<FunctionObject>();
                fn->isBuiltin = true;
                fn->builtinIndex = static_cast<int>(i);
                out[exportKey] = std::make_shared<Value>(Value::fromFunction(fn));
                return;
            }
        }
    };

    if (name == "std") {
        std::unordered_map<std::string, ValuePtr> out;
        out["v1"] = createStdlibModule(vm, "std.v1");
        return std::make_shared<Value>(Value::fromMap(std::move(out)));
    }
    if (name == "std.v1") {
        std::unordered_map<std::string, ValuePtr> out;
        const char* subs[] = {"math", "string", "bytes", "collections", "fs", "process", "net", "os", "signal", "memory",
                              "task", "sync", "time"};
        for (const char* s : subs) {
            std::string full = std::string("std.v1.") + s;
            out[s] = createStdlibModule(vm, full);
        }
        return std::make_shared<Value>(Value::fromMap(std::move(out)));
    }

    auto itV1 = stdV1NamedExports().find(name);
    if (itV1 != stdV1NamedExports().end()) {
        std::unordered_map<std::string, ValuePtr> out;
        for (const auto& pr : itV1->second) bindExport(out, pr.first, pr.second);
        if (name == "std.v1.math") {
            out["PI"] = std::make_shared<Value>(Value::fromFloat(3.14159265358979323846));
            out["E"] = std::make_shared<Value>(Value::fromFloat(2.71828182845904523536));
            out["TAU"] = std::make_shared<Value>(Value::fromFloat(6.28318530717958647692));
        }
        return std::make_shared<Value>(Value::fromMap(std::move(out)));
    }

    static const std::unordered_map<std::string, std::vector<std::string>> MODULES = {
        // math: full set including constants (PI, E set below)
        { "math", {
            "sqrt", "pow", "sin", "cos", "tan", "floor", "ceil", "round", "round_to", "abs", "min", "max",
            "clamp", "lerp", "log", "atan2", "sign", "deg_to_rad", "rad_to_deg", "PI", "E"
        }},
        // string: all string builtins including regex
        { "string", {
            "upper", "lower", "replace", "join", "split", "partition", "trim", "starts_with", "ends_with",
            "repeat", "pad_left", "pad_right", "split_lines", "format", "len", "regex_match", "regex_replace",
            "chr", "ord", "hex", "bin", "hash_fnv", "escape_regex"
        }},
        { "json", { "json_parse", "json_stringify" }},
        { "net", { "url_encode", "url_decode", "http_get", "http_request", "http_post", "url_parse", "parse_query", "build_query", "url_resolve", "mime_type_guess", "http_parse_response",
            "parse_cookie_header", "content_type_charset", "is_safe_http_redirect", "http_parse_request", "parse_link_header",
            "parse_content_disposition", "url_normalize", "http_build_response", "url_path_join", "merge_query", "parse_accept_language",
            "parse_authorization_basic" }},
        { "web", {
            "html_escape", "html_unescape", "strip_html", "xml_escape", "css_escape", "js_escape", "css_url_escape", "html_nl2br",
            "build_query", "url_encode", "url_decode", "url_parse", "parse_query", "url_resolve", "merge_query", "url_path_join",
            "mime_type_guess", "parse_data_url", "http_parse_response", "http_parse_request", "http_build_response",
            "parse_cookie_header", "set_cookie_fields", "content_type_charset", "is_safe_http_redirect",
            "parse_link_header", "parse_content_disposition", "url_normalize", "html_sanitize_strict",
            "parse_authorization_basic", "parse_accept_language",
        }},
        { "data", { "toml_parse", "toml_stringify", "csv_parse", "csv_stringify" }},
        { "random", { "random", "random_int", "random_choice", "random_shuffle" }},
        // system: process, error, and debugging
        { "sys", {
            "cli_args", "print", "panic", "error_message", "Error", "stack_trace", "stack_trace_array", "assertType",
            "error_name", "error_cause", "ValueError", "TypeError", "RuntimeError", "OSError", "KeyError", "is_error_type",
            "format_exception", "error_traceback", "invoke", "extend_array", "with_cleanup",
            "repr", "kern_version", "platform", "os_name", "arch", "exit_code", "uuid", "env_all", "env_get",
            "cwd", "chdir", "hostname", "cpu_count", "getpid", "monotonic_time", "env_set",
            "set_step_limit", "set_max_call_depth", "set_callback_guard", "deterministic_mode", "runtime_info"
        }},
        { "io", {
            "read_file", "write_file", "append_file", "require", "readFile", "writeFile", "appendFile", "readline", "base64_encode", "base64_decode", "csv_parse", "csv_stringify", "fileExists", "listDir", "file_size", "glob",
            "listDirRecursive", "create_dir", "is_file", "is_dir", "copy_file", "delete_file", "move_file"
        }},
        // array: full combinator set
        { "array", {
            "array", "len", "push", "push_front", "slice", "map", "filter", "reduce", "reverse", "find",
            "sort", "sorted", "enumerate", "flatten", "flat_map", "zip", "chunk", "unique", "first", "last", "take", "drop",
            "sort_by", "copy", "cartesian", "window", "deep_equal", "insert_at", "remove_at"
        }},
        { "env", { "env_get", "env_all", "env_set" }},
        // map/object utilities
        { "map", {
            "keys", "values", "has", "merge", "deep_equal", "copy"
        }},
        // type conversion
        { "types", {
            "str", "int", "float", "parse_int", "parse_float", "is_nan", "is_inf",
            "is_string", "is_array", "is_map", "is_number", "is_int", "is_float", "is_function", "type"
        }},
        // debug and reflection
        { "debug", {
            "inspect", "type", "dir", "stack_trace", "assert_eq"
        }},
        // logging
        { "log", {
            "log_info", "log_warn", "log_error", "log_debug"
        }},
        // time
        { "time", {
            "time", "sleep", "time_format", "monotonic_time"
        }},
        // low-level memory (see LOW_LEVEL_AND_MEMORY.md)
        { "memory", {
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
            "mem_xor", "mem_zero"
        }},
        // general utilities
        { "util", {
            "range", "default", "merge", "all", "any", "vec2", "vec3", "rand_vec2", "rand_vec3"
        }},
        // profiling
        { "profiling", {
            "profile_cycles", "profile_fn"
        }},
        // path and file system (path helpers + file I/O)
        { "path", {
            "basename", "dirname", "path_join", "cwd", "chdir", "realpath", "temp_dir",
            "read_file", "write_file", "append_file", "fileExists", "listDir", "listDirRecursive",
            "create_dir", "is_file", "is_dir", "copy_file", "delete_file", "move_file", "file_size", "glob",
            "path_normalize"
        }},
        // errors and exceptions
        { "errors", {
            "Error", "panic", "error_message", "error_name", "error_cause",
            "ValueError", "TypeError", "RuntimeError", "OSError", "KeyError", "is_error_type",
            "stack_trace", "stack_trace_array", "format_exception", "error_traceback", "error_structured"
        }},
        // iteration and sequences
        { "iter", {
            "range", "map", "filter", "reduce", "all", "any", "cartesian", "window", "enumerate", "zip"
        }},
        // collections (arrays and maps)
        { "collections", {
            "array", "len", "push", "push_front", "slice", "keys", "values", "has",
            "map", "filter", "reduce", "reverse", "find", "sort", "sorted", "enumerate", "flatten", "flat_map",
            "zip", "chunk", "unique", "first", "last", "take", "drop", "sort_by",
            "copy", "merge", "deep_equal", "cartesian", "window"
        }},
        // file system (alias for io)
        { "fs", {
            "read_file", "write_file", "append_file", "readFile", "writeFile", "appendFile", "fileExists", "listDir",
            "listDirRecursive", "create_dir", "is_file", "is_dir", "copy_file", "delete_file", "move_file"
        }},
        // python-inspired: re -> regex
        { "regex", { "regex_match", "regex_replace", "regex_split", "regex_find_all", "regex_compile", "regex_match_pattern", "regex_replace_pattern", "escape_regex" }},
        // python csv
        { "csv", { "csv_parse", "csv_stringify" }},
        // python base64 -> b64
        { "b64", { "base64_encode", "base64_decode" }},
        // python logging
        { "logging", { "log_info", "log_warn", "log_error", "log_debug" }},
        // python hashlib -> hash
        { "hash", { "hash_fnv", "sha1", "sha256" }},
        // python uuid
        { "uuid", { "uuid" }},
        // python os (env + path + process info)
        { "os", { "cwd", "chdir", "getpid", "hostname", "cpu_count", "env_get", "env_all", "env_set", "listDir", "create_dir", "is_file", "is_dir", "temp_dir", "realpath", "which", "exec_args", "spawn", "wait_process", "kill_process" }},
        // python copy
        { "copy", { "copy", "deep_equal" }},
        // python datetime (time/format)
        { "datetime", { "time", "sleep", "time_format", "monotonic_time" }},
        // python secrets (random + uuid)
        { "secrets", { "random", "random_int", "random_choice", "random_shuffle", "uuid" }},
        // python itertools -> itools
        { "itools", { "range", "map", "filter", "reduce", "all", "any", "cartesian", "window", "enumerate", "zip", "sorted" }},
        // python argparse -> cli
        { "cli", { "cli_args" }},
        // encoding (bytes/string)
        { "encoding", { "base64_encode", "base64_decode", "string_to_bytes", "bytes_to_string" }},
        // python run/exit
        { "run", { "cli_args", "exit_code" }},
        // advanced interop and native boundaries
        { "interop", {
            "ffi_allow_library", "ffi_call", "ffi_call_typed",
            "ptr_address", "ptr_from_address", "ptr_offset", "ptr_add", "ptr_sub", "ptr_diff", "ptr_eq",
            "type", "runtime_info"
        }},
        // process-level concurrency primitives (multi-process orchestration)
        { "concurrency", {
            "spawn", "wait_process", "kill_process", "exec_args", "which",
            "sleep_ms", "sleep", "cpu_count", "monotonic_time", "getpid"
        }},
        // deep runtime visibility and profiling
        { "observability", {
            "profile_cycles", "profile_fn",
            "stack_trace", "stack_trace_array", "runtime_info",
            "log_info", "log_warn", "log_error", "log_debug",
            "format_exception", "error_traceback"
        }},
        // security and integrity toolkit
        { "security", {
            "sha1", "sha256", "hash_fnv", "uuid",
            "random_int", "random_choice", "random_shuffle",
            "base64_encode", "base64_decode",
            "is_safe_http_redirect", "parse_authorization_basic"
        }},
        // automation and CI-style task control
        { "automation", {
            "exec", "exec_capture", "exec_args", "spawn", "wait_process", "kill_process", "which", "retry_call",
            "env_get", "env_set", "env_all", "cwd", "chdir", "time", "sleep_ms"
        }},
        // binary data and endian-safe operations
        { "binary", {
            "string_to_bytes", "bytes_to_string",
            "read_le16", "read_le32", "read_le64", "write_le16", "write_le32", "write_le64",
            "read_be16", "read_be32", "read_be64", "write_be16", "write_be32", "write_be64",
            "read_le_float", "write_le_float", "read_le_double", "write_le_double",
            "read_be_float", "write_be_float", "read_be_double", "write_be_double",
            "get_endianness", "bytes_read", "bytes_write"
        }},
        // secure web sanitation and header parsing
        { "websec", {
            "html_escape", "html_unescape", "strip_html", "html_sanitize_strict", "html_nl2br",
            "css_escape", "css_url_escape", "js_escape", "xml_escape",
            "parse_cookie_header", "set_cookie_fields", "content_type_charset",
            "parse_link_header", "parse_content_disposition", "parse_accept_language"
        }},
        // network protocol and HTTP tooling
        { "netops", {
            "http_get", "http_request", "http_post",
            "http_parse_response", "http_parse_request", "http_build_response",
            "url_parse", "parse_query", "build_query", "merge_query", "url_resolve", "url_normalize", "url_path_join",
            "mime_type_guess", "parse_data_url"
        }},
        // data engineering and config serialization
        { "datatools", {
            "json_parse", "json_stringify",
            "toml_parse", "toml_stringify",
            "csv_parse", "csv_stringify",
            "parse_int", "parse_float", "default", "merge", "deep_equal", "format"
        }},
        // runtime safety and guard controls
        { "runtime_controls", {
            "set_step_limit", "set_max_call_depth", "set_callback_guard", "deterministic_mode",
            "runtime_info", "ffi_allow_library", "path_normalize", "retry_call"
        }},
    };
    auto it = MODULES.find(nameIn);
    if (it == MODULES.end()) it = MODULES.find(name);
    if (it == MODULES.end()) return nullptr;
    std::unordered_map<std::string, ValuePtr> out;
    for (const std::string& key : it->second) {
        bindExport(out, key, key);
    }
    if (name == "math") {
        out["PI"] = std::make_shared<Value>(Value::fromFloat(3.14159265358979323846));
        out["E"] = std::make_shared<Value>(Value::fromFloat(2.71828182845904523536));
        out["TAU"] = std::make_shared<Value>(Value::fromFloat(6.28318530717958647692));  // 2*pI
    }
    return std::make_shared<Value>(Value::fromMap(std::move(out)));
}

bool isStdlibModuleName(const std::string& name) {
    std::string r = resolveStdlibModuleAlias(name);
    if (r == "std" || r == "std.v1") return true;
    if (stdV1NamedExports().find(r) != stdV1NamedExports().end()) return true;
    return name == "math" || name == "string" || name == "json" || name == "random"
        || name == "sys" || name == "io" || name == "net" || name == "web" || name == "data" || name == "array" || name == "env"
        || name == "map" || name == "types" || name == "debug" || name == "log"
        || name == "time" || name == "memory" || name == "util" || name == "profiling"
        || name == "path" || name == "errors" || name == "iter" || name == "collections" || name == "fs"
        || name == "regex" || name == "csv" || name == "b64" || name == "logging" || name == "hash"
        || name == "uuid" || name == "os" || name == "copy" || name == "datetime" || name == "secrets"
        || name == "itools" || name == "cli" || name == "encoding" || name == "run"
        || name == "interop" || name == "concurrency" || name == "observability" || name == "security"
        || name == "automation" || name == "binary" || name == "websec" || name == "netops"
        || name == "datatools" || name == "runtime_controls";
}

} // namespace kern
