/* *
 * kern REPL - Interactive read-eval-print loop
 * multi-line input support and advanced error display.
 */

#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/codegen.hpp"
#include "compiler/ast.hpp"
#include "vm/vm.hpp"
#include "vm/permissions.hpp"
#include "bytecode/value.hpp"
#include "vm/builtins.hpp"
#include "bytecode/bytecode.hpp"
#include "errors/errors.hpp"
#include "import_resolution.hpp"
#include "platform/kern_env.hpp"
#ifdef KERN_BUILD_GAME
#include "game/game_builtins.hpp"
#endif
#include <iostream>
#include <unordered_set>
#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace kern;

static bool runSource(VM& vm, const std::string& source) {
    replClearImportLoadingAtReplLineStart();
    g_errorReporter.setSource(source);
    g_errorReporter.setFilename("<repl>");  // virtual path for diagnostics / stack traces
    try {
        Lexer lexer(source);
        std::vector<Token> tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        std::unique_ptr<Program> program = parser.parse();
        CodeGenerator gen;
        Bytecode code = gen.generate(std::move(program));
        const std::vector<std::string>& constants = gen.getConstants();
        std::unordered_set<std::string> declaredGlobals;
        insertAllBuiltinNamesForAnalysis(declaredGlobals);
#ifdef KERN_BUILD_GAME
        for (const std::string& n : getGameBuiltinNames()) declaredGlobals.insert(n);
#endif
        declaredGlobals.insert("__import");
        for (const auto& inst : code) {
            if (inst.op != Opcode::STORE_GLOBAL || inst.operand.index() != 4) continue;
            size_t idx = std::get<size_t>(inst.operand);
            if (idx < constants.size()) declaredGlobals.insert(constants[idx]);
        }
        for (const auto& inst : code) {
            if (inst.op != Opcode::LOAD_GLOBAL || inst.operand.index() != 4) continue;
            size_t idx = std::get<size_t>(inst.operand);
            if (idx >= constants.size()) continue;
            const std::string& name = constants[idx];
            if (declaredGlobals.find(name) == declaredGlobals.end())
                g_errorReporter.reportWarning(inst.line, 0,
                    "Possible undefined variable '" + name + "'. Did you mean to define it first?",
                    undefinedGlobalLoadWarningHint(), "ANAL-LOAD-GLOBAL", undefinedGlobalLoadWarningDetail());
        }
        vm.setBytecode(code);
        vm.setStringConstants(gen.getConstants());
        vm.setValueConstants(gen.getValueConstants());
        vm.setActiveSourcePath("<repl>");
        vm.run();
        if (vm.hasResult()) {
            ValuePtr r = vm.getResult();
            if (r && r->type != Value::Type::NIL)
                std::cout << "=> " << r->toString() << std::endl;
        }
        return true;
    } catch (const LexerError& e) {
        g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, e.what(), lexerCompileErrorHint(),
            "LEX-TOKENIZE", lexerCompileErrorDetail());
        return false;
    } catch (const ParserError& e) {
        std::string msg(e.what());
        g_errorReporter.reportCompileError(ErrorCategory::SyntaxError, e.line, e.column, msg, parserCompileErrorHint(msg),
            "PARSE-SYNTAX", parserCompileErrorDetail(msg));
        return false;
    } catch (const CodegenError& e) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, e.line, 0,
            std::string("Code generation failed: ") + e.what(),
            "The compiler hit an unsupported AST/operator case while lowering to bytecode.",
            "CODEGEN-UNSUPPORTED",
            internalFailureDetail("code generation"));
        return false;
    } catch (const VMError& e) {
        std::vector<StackFrame> stack;
        for (const auto& f : vm.getCallStackSlice()) {
            stack.push_back({f.functionName, f.filePath, f.line, f.column});
        }
        std::string hint(vmRuntimeErrorHint(e.category, e.code));
        g_errorReporter.reportRuntimeError(vmErrorCategory(e.category), e.line, e.column, e.what(), stack, hint,
            vmErrorCodeString(e.category, e.code), vmRuntimeErrorDetail(e.category, e.code),
            e.lineEnd, e.columnEnd);
        return false;
    } catch (const std::exception& e) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, std::string("Kern stopped: ") + e.what(),
            "This may be an unexpected error; try reducing the input to a minimal case.", "KERN-STOP-EXC",
            internalFailureDetail("REPL evaluation (std::exception)"));
        return false;
    } catch (...) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, "Kern stopped: unknown exception",
            "This may be an internal error; try simplifying the code.", "KERN-STOP-UNKNOWN",
            internalFailureDetail("REPL evaluation (non-typed throw)") + "\n"
            "No exception message was available; enable sanitizers or a debugger if this persists.");
        return false;
    }
}

static const size_t IMPORT_BUILTIN_INDEX = 200;

int main(int argc, char** argv) {
    kern::initKernEnvironmentFromArgv(argc, argv);
    VM vm;
    RuntimeGuardPolicy guards;
    guards.debugMode = true;
    guards.allowUnsafe = false;
    guards.enforcePointerBounds = true;
    guards.ffiEnabled = false;
    guards.sandboxEnabled = true;
    registerAllStandardPermissions(guards);
    vm.setRuntimeGuards(guards);
    vm.setStepLimit(5'000'000);
    vm.setMaxCallDepth(2048);
    vm.setCallbackStepGuard(250'000);
    registerAllBuiltins(vm);
    registerImportBuiltin(vm);
    vm.setCliArgs({"kern", "repl"});
    std::cout << "Kern REPL. Type 'exit' to quit. Commands: .help .clear" << std::endl;
    std::string input;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, input)) break;
        if (input.empty()) continue;
        if (input == "exit" || input == "quit") break;
        if (input == ".help") {
            std::cout << "Commands: .help (this), .clear (clear screen), exit/quit (exit).\n"
                         "Examples: let x = 5   print(x)   print(2+3)" << std::endl;
            continue;
        }
        if (input == ".clear") {
#ifdef _WIN32
            std::system("cls");
#else
            std::system("clear");
#endif
            continue;
        }
        g_errorReporter.resetCounts();
        runSource(vm, input);
        g_errorReporter.printSummary();
    }
    return 0;
}
