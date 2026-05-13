/*
 * windows: small front-end named kernc.exe that runs an incremental CMake build for target
 * "kern_launcher" (produces kern-impl.exe), then execs kern-impl.exe with the same arguments.
 * ensures CLI users who invoke build\Release\kernc.exe get a fresh compiler without a separate rebuild step.
 */
#define WIN32_LEAN_AND_MEAN
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static void dirname_inplace(char* path) {
    char* p = strrchr(path, '\\');
    if (!p) p = strrchr(path, '/');
    if (p) *p = '\0';
}

int main(int argc, char** argv) {
    char self[MAX_PATH];
    if (GetModuleFileNameA(NULL, self, MAX_PATH) == 0) return 127;

    char dir[MAX_PATH];
    size_t n = strlen(self);
    if (n >= sizeof(dir)) n = sizeof(dir) - 1U;
    memcpy(dir, self, n);
    dir[n] = '\0';
    dirname_inplace(dir);

    char root[MAX_PATH];
    n = strlen(dir);
    if (n >= sizeof(root)) n = sizeof(root) - 1U;
    memcpy(root, dir, n);
    root[n] = '\0';
    dirname_inplace(root); /* .../build*/
    dirname_inplace(root); /* repo root*/

    char buildDir[MAX_PATH];
    snprintf(buildDir, sizeof(buildDir), "%s\\build", root);

    const char* cfg =
        (strstr(self, "\\Debug\\") != NULL || strstr(self, "/Debug/") != NULL) ? "Debug" : "Release";

    char cmakeCmd[6144];
    snprintf(cmakeCmd, sizeof(cmakeCmd), "cmake --build \"%s\" --config %s --target kern_launcher", buildDir, cfg);
    int cr = system(cmakeCmd);
    if (cr != 0) {
        fprintf(stderr, "kern: cmake --build failed (%d)\n", cr);
        char msg[2048];
        snprintf(msg, sizeof(msg),
                 "cmake --build failed (exit %d).\n\n"
                 "Common causes: CMake not on PATH, or build\\ is missing/misconfigured.\n\n"
                 "Command:\n%s",
                 cr, cmakeCmd);
        MessageBoxA(NULL, msg, "kernc launcher", MB_OK | MB_ICONERROR);
        return cr > 255 ? 255 : cr;
    }

    char impl[MAX_PATH];
    snprintf(impl, sizeof(impl), "%s\\kern-impl.exe", dir);

    char** av = (char**)calloc((size_t)argc + 1, sizeof(char*));
    if (!av) return 1;
    av[0] = impl;
    for (int i = 1; i < argc; ++i) av[i] = argv[i];
    av[argc] = NULL;

    /* use wait-mode spawn so launcher returns the same exit code as kern-impl.*/
    intptr_t rc = _spawnv(_P_WAIT, impl, (const char* const*)av);
    free(av);
    if (rc == -1) {
        fprintf(stderr, "kern: could not run %s\n", impl);
        perror("kern");
        char msg[1024];
        snprintf(msg, sizeof(msg), "Could not start:\n%s\n\n(Is kern-impl.exe missing?)", impl);
        MessageBoxA(NULL, msg, "kernc launcher", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (rc > 255) return 255;
    return (int)rc;
}
