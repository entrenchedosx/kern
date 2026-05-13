/* *
 * Contract tests for humanizePathForDisplay-001 (see errors.hpp).
 * Built as a tiny executable; CI can run: kern_contract_humanize && echo OK
 */

#include "errors/errors.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void fail(const char* msg) {
    std::cerr << "humanize_path_contract_test: " << msg << "\n";
    std::exit(1);
}

void expectEq(const std::string& got, const std::string& want, const char* ctx) {
    if (got != want) {
        std::cerr << "humanize_path_contract_test: " << ctx << "\n  want: " << want << "\n  got:  " << got << "\n";
        std::exit(1);
    }
}

} // namespace

int main() {
    using kern::humanizePathForDisplay;

    // 1. empty → "<unknown>"
    expectEq(humanizePathForDisplay(""), "<unknown>", "empty path");

    // 2. "<repl>" → interactive label
    expectEq(humanizePathForDisplay("<repl>"), "<repl> (interactive)", "repl sentinel");

    // 3. otherwise unchanged (no reordering / extra rules)
    const std::string p = "D:/proj/foo.kn";
    expectEq(humanizePathForDisplay(p), p, "ordinary path passthrough");

    const std::string special = "<unknown>";
    expectEq(humanizePathForDisplay(special), special, "literal <unknown> string is not treated as empty");

    return 0;
}
