#ifndef KERN_STDLIB_STDV1_EXPORTS_HPP
#define KERN_STDLIB_STDV1_EXPORTS_HPP

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kern {

/* * User-facing export name -> global builtin name (std_math_* or legacy read_file, etc.).*/
inline const std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>>& stdV1NamedExports() {
    static const std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> m = {
        {"std.v1.math",
            {{"fmod", "std_math_fmod"},
             {"hypot", "std_math_hypot"},
             {"log10", "std_math_log10"},
             {"log2", "std_math_log2"},
             {"exp", "std_math_exp"},
             {"expm1", "std_math_expm1"},
             {"log1p", "std_math_log1p"},
             {"cbrt", "std_math_cbrt"},
             {"asin", "std_math_asin"},
             {"acos", "std_math_acos"},
             {"atan", "std_math_atan"},
             {"sinh", "std_math_sinh"},
             {"cosh", "std_math_cosh"},
             {"tanh", "std_math_tanh"},
             {"copysign", "std_math_copysign"},
             {"nextafter", "std_math_nextafter"},
             {"trunc", "std_math_trunc"},
             {"round_to_int", "std_math_round_to_int"}}},
        {"std.v1.string",
            {{"index_of", "std_string_index_of"},
             {"last_index_of", "std_string_last_index_of"},
             {"count_substr", "std_string_count_substr"},
             {"substr", "std_string_substr"},
             {"equals_ignore_case", "std_string_equals_ignore_case"},
             {"compare", "std_string_compare"},
             {"trim_left", "std_string_trim_left"},
             {"trim_right", "std_string_trim_right"},
             {"split_first", "std_string_split_first"},
             {"char_at", "std_string_char_at"},
             {"is_empty", "std_string_is_empty"},
             {"remove_prefix", "std_string_remove_prefix"},
             {"remove_suffix", "std_string_remove_suffix"},
             {"pad_center", "std_string_pad_center"}}},
        {"std.v1.bytes",
            {{"crc32", "std_bytes_crc32"},
             {"to_hex", "std_bytes_to_hex"},
             {"from_hex", "std_bytes_from_hex"},
             {"xor", "std_bytes_xor"},
             {"concat", "std_bytes_concat"},
             {"slice", "std_bytes_slice"},
             {"equal", "std_bytes_equal"},
             {"get_u16_le", "std_bytes_get_u16_le"},
             {"get_u32_le", "std_bytes_get_u32_le"},
             {"set_u32_le", "std_bytes_set_u32_le"},
             {"get_u8", "std_bytes_get_u8"},
             {"from_string", "std_bytes_from_string"},
             {"len", "std_bytes_len"}}},
        {"std.v1.collections",
            {{"array_index_of", "std_col_array_index_of"},
             {"array_last_index_of", "std_col_array_last_index_of"},
             {"swap", "std_col_swap"},
             {"rotate_left", "std_col_rotate_left"},
             {"dict_merge_shallow", "std_col_dict_merge_shallow"},
             {"array_zip_pairs", "std_col_array_zip_pairs"},
             {"array_unique", "std_col_array_unique"},
             {"array_fill", "std_col_array_fill"},
             {"map_invert", "std_col_map_invert"},
             {"array_intersect", "std_col_array_intersect"}}},
        {"std.v1.fs",
            {{"read_text", "read_file"},
             {"write_text", "write_file"},
             {"file_exists", "fileExists"},
             {"list_dir", "listDir"},
             {"list_recursive", "listDirRecursive"},
             {"is_file", "is_file"},
             {"is_dir", "is_dir"},
             {"create_dir", "create_dir"},
             {"copy_file", "copy_file"},
             {"delete_file", "delete_file"},
             {"move_file", "move_file"},
             {"file_size", "file_size"},
             {"glob", "glob"},
             {"path_join", "path_join"},
             {"basename", "basename"},
             {"dirname", "dirname"},
             {"normalize", "path_normalize"},
             {"realpath", "realpath"}}},
        {"std.v1.process",
            {{"cli_args", "cli_args"},
             {"cwd", "cwd"},
             {"chdir", "chdir"},
             {"env_get", "env_get"},
             {"env_all", "env_all"},
             {"env_set", "env_set"},
             {"getpid", "getpid"},
             {"hostname", "hostname"},
             {"cpu_count", "cpu_count"},
             {"which", "which"},
             {"spawn", "spawn"},
             {"wait_process", "wait_process"},
             {"kill_process", "kill_process"},
             {"exec_args", "exec_args"},
             {"exec_capture", "exec_capture"},
             {"sleep_ms", "sleep_ms"}}},
        {"std.v1.time",
            {{"now", "time"},
             {"monotonic", "monotonic_time"},
             {"sleep", "sleep"},
             {"sleep_ms", "sleep_ms"},
             {"time_format", "time_format"}}},
    };
    return m;
}

inline std::string resolveStdlibModuleAlias(std::string name) {
    static const std::unordered_map<std::string, std::string> aliases = {
        {"std.math", "std.v1.math"},
        {"std.string", "std.v1.string"},
        {"std.bytes", "std.v1.bytes"},
        {"std.collections", "std.v1.collections"},
        {"std.fs", "std.v1.fs"},
        {"std.process", "std.v1.process"},
        {"std.time", "std.v1.time"},
    };
    auto it = aliases.find(name);
    if (it != aliases.end()) return it->second;
    return name;
}

} // namespace kern

#endif
