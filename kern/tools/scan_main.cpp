/* Thin CLI wrapper: same behavior as `kern --scan` (cross-layer static analysis). */

#include "scanner/scan_driver.hpp"
#include "import_resolution.hpp"
#include "vm/builtins.hpp"
#include "vm/vm.hpp"
#include "vm/permissions.hpp"

int main(int argc, char** argv) {
    const char* prog = argc >= 1 ? argv[0] : "kern-scan";
    kern::VM vm;
    kern::registerAllBuiltins(vm);
    kern::registerImportBuiltin(vm);
    kern::RuntimeGuardPolicy guards;
    guards.debugMode = true;
    kern::registerAllStandardPermissions(guards);
    vm.setRuntimeGuards(guards);
    vm.setStepLimit(5'000'000);
    vm.setMaxCallDepth(2048);
    vm.setCallbackStepGuard(250'000);
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
    vm.setCliArgs(std::move(args));
    return kern::runScanFromArgv(argc, argv, 1, vm, prog);
}
