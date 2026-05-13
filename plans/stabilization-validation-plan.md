# Stabilization & Validation Plan

## Objective
Prove that the hardened `CallExpr` logic in [`codegen.cpp`](../kern/core/compiler/codegen.cpp:657) correctly throws `CodegenError("Missing required argument for parameter 'x'", ...)` when named arguments are omitted.

## Current State

### Compilation Pipeline (manual, no Compiler class)
```
Lexer(source)  -->  tokenize()  -->  vector<Token>
Parser(tokens)  -->  parse()  -->  unique_ptr<Program>
CodeGenerator()  -->  generate(program)  -->  Bytecode
```

### Relevant Error Throw
At [`codegen.cpp:656-658`](../kern/core/compiler/codegen.cpp:656):
```cpp
if (ordered[i] == nullptr) {
    throw CodegenError("Missing required argument for parameter '" + pnames[i] + "'", currentLine_);
}
```

### Existing `.o` Files (reusable)
`main.o`, `lexer.o`, `vm.o`, `bytecode_verifier.o`, `scheduler.o`, `value.o`, `http_get_winhttp.o`, `native_bindings.o`, `module_registry.o`, `codegen.o`, `bytecode_peephole.o`

---

## Step 1: Modify `kern/cli/main.cpp`

Replace the current hardcoded bytecode arithmetic test with an integration test that:
1. Defines a test source string: `"func test(x: int, y: int) = x + y\n test(y: 42)"`
2. Runs the full Lexer → Parser → CodeGenerator pipeline
3. Wraps the compilation in `try/catch` for `CodegenError` (or `std::exception`)
4. Prints the error message to `std::cerr` on catch

### New `main.cpp` content:
```cpp
#include <iostream>
#include <vector>
#include "../core/compiler/lexer.hpp"
#include "../core/compiler/parser.hpp"
#include "../core/compiler/codegen.hpp"

int main() {
    // Source: define a function with two required params, then call with only y
    std::string source = "func test(x: int, y: int) = x + y\n test(y: 42)";

    try {
        // Step 1: Lex
        kern::Lexer lexer(source);
        auto tokens = lexer.tokenize();

        // Step 2: Parse
        kern::Parser parser(tokens);
        auto program = parser.parse();

        // Step 3: Codegen (this should throw)
        kern::CodeGenerator cg;
        auto bytecode = cg.generate(std::move(program));

        // If we reach here, the bug is NOT fixed
        std::cerr << "ERROR: Expected CodegenError for missing argument, but compilation succeeded." << std::endl;
        return 1;

    } catch (const kern::CodegenError& e) {
        std::cerr << e.what() << std::endl;
        return 0;  // Success — missing argument was caught
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### Key changes from current file:
- Removed `#include "../runtime/vm/vm.hpp"`, `#include "../core/bytecode/bytecode.hpp"`, `#include "../core/value.hpp"` (no longer needed)
- Added `#include "../core/compiler/parser.hpp"` and `#include "../core/compiler/codegen.hpp"`
- Replaced hardcoded `"10 + 32"` lexer test with full pipeline test
- Added explicit catch for `kern::CodegenError` to verify the exact error

---

## Step 2: Build & Execute Commands

Run these four commands **in order** from the project root (`e:/kerncode`):

### 2a. Fix DLL search path
```powershell
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
```

### 2b. Recompile only `main.cpp`
```powershell
C:\msys64\mingw64\bin\g++.exe -std=c++17 -Wall -Wextra -Wpedantic -c -I. -Ikern/core kern/cli/main.cpp -o main.o
```

### 2c. Link all object files into executable
```powershell
C:\msys64\mingw64\bin\g++.exe -std=c++17 main.o lexer.o vm.o bytecode_verifier.o scheduler.o value.o http_get_winhttp.o native_bindings.o module_registry.o codegen.o bytecode_peephole.o -o kern_runtime.exe -lwinhttp
```

### 2d. Execute
```powershell
.\kern_runtime.exe
```

---

## Success Criteria

The terminal output **must** contain exactly:
```
Missing required argument for parameter 'x'
```

This proves the `CodegenError` was thrown by the named-argument validation logic at [`codegen.cpp:657`](../kern/core/compiler/codegen.cpp:657).

---

## What to Report Back

Copy and paste the **raw terminal output** from step 2d. If the output is `Missing required argument for parameter 'x'`, the fix is validated. If the output differs, capture the full output for debugging.
