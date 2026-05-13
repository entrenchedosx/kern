#ifndef KERN_SCANNER_SCAN_DRIVER_HPP
#define KERN_SCANNER_SCAN_DRIVER_HPP

#include "vm/vm.hpp"

namespace kern {

struct ScanCliOptions {
    bool json = false;
    bool verbose = false;
    bool strictTypes = false;
    bool registryOnly = false;
    bool runTestsAfter = false;
    std::string testDir; // empty => tests/coverage when runTestsAfter
};

/* * Parse argv after `--scan` (flags then paths). Default path is "." when none given.*/
int runScanFromArgv(int argc, char** argv, int argStart, VM& vm, const char* progName);

} // namespace kern

#endif
