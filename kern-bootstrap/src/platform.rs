pub fn kern_executable_name() -> &'static str {
    if cfg!(windows) {
        "kern.exe"
    } else {
        "kern"
    }
}
