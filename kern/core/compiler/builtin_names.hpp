/* *
 * Stable builtin name lists for static analysis (compiler / scanner).
 * Lives in the core layer — must not include VM execution headers.
 */

#ifndef KERN_BUILTIN_NAMES_HPP
#define KERN_BUILTIN_NAMES_HPP

#include <string>
#include <unordered_set>
#include <vector>

namespace kern {

inline std::vector<std::string> getBuiltinNames() {
    static std::vector<std::string> names;
    if (!names.empty()) return names;
    names = {"print","sqrt","pow","sin","cos","random","floor","ceil","str","int","float","array","len",
        "read_file","write_file","time","inspect","alloc","free","type","dir","profile_cycles","push","slice",
        "keys","values","has","upper","lower","replace","join","split","round","abs","log","fileExists","listDir",
        "sleep","Instance","json_parse","json_stringify","cli_args","range","copy","freeze",
        "deep_equal","random_choice","random_int","random_shuffle","map","filter","reduce","Error","panic","error_message",
        "clamp","lerp","min","max",
        "listDirRecursive","copy_file","delete_file",
        "trim","starts_with","ends_with",
        "flat_map","zip","chunk","unique",
        "log_info","log_warn","log_error",
        "move_file","format","tan","atan2",
        "reverse","find","sort","flatten",
        "repeat","pad_left","pad_right",
        "env_get","all","any",
        "create_dir","is_file","is_dir",
        "sort_by","first","last","take","drop",
        "split_lines","sign","deg_to_rad","rad_to_deg",
        "parse_int","parse_float","default","merge",
        "log_debug","stack_trace","assertType","profile_fn","cartesian","window",
        "push_front","vec2","vec3","rand_vec2","rand_vec3","regex_match","regex_replace",
        "mem_copy","mem_set","ptr_offset","ptr_address","ptr_from_address",
        "peek8","peek16","peek32","peek64","poke8","poke16","poke32","poke64",
        "align_up","align_down","memory_barrier","volatile_load8","volatile_store8",
        "mem_cmp","mem_move","realloc","ptr_align_up","ptr_align_down",
        "peek_float","poke_float","peek_double","poke_double",
        "volatile_load16","volatile_store16","volatile_load32","volatile_store32","volatile_load64","volatile_store64",
        "peek8s","peek16s","peek32s","peek64s",
        "mem_swap","bytes_read","bytes_write","ptr_is_null","size_of_ptr",
        "ptr_add","ptr_sub","is_aligned","mem_set_zero","ptr_tag","ptr_untag","ptr_get_tag",
        "struct_define","offsetof_struct","sizeof_struct",
        "pool_create","pool_alloc","pool_free","pool_destroy",
        "read_be16","read_be32","read_be64","write_be16","write_be32","write_be64",
        "dump_memory","alloc_tracked","free_tracked","get_tracked_allocations",
        "atomic_load32","atomic_store32","atomic_add32","atomic_cmpxchg32",
        "map_file","unmap_file","memory_protect",
        "error_name","error_cause","ValueError","TypeError","RuntimeError","OSError","KeyError","is_error_type",
        "read_le16","read_le32","read_le64","write_le16","write_le32","write_le64","alloc_zeroed",
        "basename","dirname","path_join",
        "ptr_eq","alloc_aligned","string_to_bytes","bytes_to_string",
        "memory_page_size","mem_find","mem_fill_pattern",
        "ptr_compare","mem_reverse",
        "mem_scan","mem_overlaps","get_endianness",
        "mem_is_zero","read_le_float","write_le_float",
        "read_le_double","write_le_double",
        "mem_count","ptr_min","ptr_max","ptr_diff",
        "read_be_float","write_be_float","read_be_double","write_be_double","ptr_in_range",
        "mem_xor","mem_zero",
        "repr","kern_version","platform","os_name","arch","exit_code",
        "readline","chr","ord","hex","bin","assert_eq",
        "base64_encode","base64_decode","uuid","hash_fnv",
        "csv_parse","csv_stringify","time_format","stack_trace_array",
        "is_nan","is_inf","env_all","escape_regex",
        "cwd","chdir","hostname","cpu_count","temp_dir","realpath","getpid","monotonic_time","file_size","env_set","glob",
        "is_string","is_array","is_map","is_number","is_function","round_to","insert_at","remove_at",
        "sleep_ms","exec","exec_capture","which",
        "set_step_limit","set_max_call_depth","set_callback_guard","deterministic_mode","runtime_info","permissions_active",
        "path_normalize","retry_call","sha1","sha256",
        "exec_args","spawn","wait_process","kill_process",
        "url_encode","url_decode","http_get","toml_parse",
        "http_request","http_post","regex_split","regex_find_all","url_parse","parse_query","html_escape","http_parse_response","toml_stringify",
        "regex_compile","regex_match_pattern","regex_replace_pattern",
        "format_exception","error_traceback","error_structured",
        "invoke","extend_array","with_cleanup","__apply_decorator","__spawn_task","__await_task","__runtime_apply_decorators","__safe_invoke2",
        "sorted","enumerate","partition","is_int","is_float",
        "html_unescape","strip_html","css_escape","js_escape","xml_escape","build_query","url_resolve","mime_type_guess","parse_data_url",
        "parse_cookie_header","set_cookie_fields","content_type_charset","is_safe_http_redirect",
        "http_parse_request","parse_link_header","parse_content_disposition","url_normalize","html_sanitize_strict","css_url_escape",
        "http_build_response","html_nl2br","url_path_join","parse_authorization_basic","merge_query","parse_accept_language",
        "ffi_allow_library","ffi_call","ffi_call_typed",
        "std_math_fmod","std_math_hypot","std_math_log10","std_math_log2","std_math_exp","std_math_expm1","std_math_log1p",
        "std_math_cbrt","std_math_asin","std_math_acos","std_math_atan","std_math_sinh","std_math_cosh","std_math_tanh",
        "std_math_copysign","std_math_nextafter","std_math_trunc","std_math_round_to_int",
        "std_string_index_of","std_string_last_index_of","std_string_count_substr","std_string_substr",
        "std_string_equals_ignore_case","std_string_compare","std_string_trim_left","std_string_trim_right",
        "std_string_split_first","std_string_char_at","std_string_is_empty","std_string_remove_prefix","std_string_remove_suffix",
        "std_string_pad_center",
        "std_bytes_crc32","std_bytes_to_hex","std_bytes_from_hex","std_bytes_xor","std_bytes_concat","std_bytes_slice",
        "std_bytes_equal","std_bytes_get_u16_le","std_bytes_get_u32_le","std_bytes_set_u32_le","std_bytes_get_u8",
        "std_bytes_from_string","std_bytes_len",
        "std_col_array_index_of","std_col_array_last_index_of","std_col_swap","std_col_rotate_left","std_col_dict_merge_shallow",
        "std_col_array_zip_pairs","std_col_array_unique","std_col_array_fill","std_col_map_invert",        "std_col_array_intersect",
        "append_file",
        "require",
        "tcp_connect",
        "tcp_connect_start",
        "tcp_connect_check",
        "tcp_listen",
        "tcp_accept",
        "tcp_send",
        "tcp_recv",
        "tcp_close",
        "udp_open",
        "udp_bind",
        "udp_send",
        "udp_recv",
        "udp_close",
        "socket_set_nonblocking",
        "socket_select_read",
        "socket_select_write",
        "fs_fd_open","fs_fd_close","fs_fd_read","fs_fd_write","fs_fd_pread","fs_fd_pwrite","fs_flock",
        "fs_statx","fs_atomic_write","fs_watch","fs_space","fs_mounts",
        "process_spawn_v2","process_wait","process_kill_tree","process_list","process_job_create","process_job_add","process_job_kill",
        "os_signal_trap","os_signal_untrap","os_runtime_limits","os_features",
        "net_tcp_server","net_tcp_poll","net_dns_lookup","net_tls_connect","net_ws_connect","net_ws_listen"};
    return names;
}

/* * predefined globals and alternate spellings that share a builtin index (not listed in getBuiltinNames order).*/
inline std::vector<std::string> getBuiltinExtraGlobalNames() {
    return {"PI", "E", "readFile", "writeFile", "appendFile", "file_exists", "list_dir"};
}

inline void insertAllBuiltinNamesForAnalysis(std::unordered_set<std::string>& out) {
    for (const auto& n : getBuiltinNames()) out.insert(n);
    for (const auto& n : getBuiltinExtraGlobalNames()) out.insert(n);
}

} // namespace kern

#endif
