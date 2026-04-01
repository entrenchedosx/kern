/* *
 * kern Game - Entry point for Kern scripts with game builtins (window, draw, input, etc.).
 * run: kern_game script.kn
 */
#include "compiler/lexer.hpp"
#include "compiler/parser.hpp"
#include "compiler/codegen.hpp"
#include "vm/vm.hpp"
#include "vm/value.hpp"
#include "vm/builtins.hpp"
#include "vm/bytecode.hpp"
#include "game/game_builtins.hpp"
#include "errors.hpp"
#include <iostream>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <exception>

using namespace kern;

static bool runSource(VM& vm, const std::string& source, const std::string& filename = "") {
    g_errorReporter.setSource(source);
    g_errorReporter.setFilename(filename);
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
        for (const std::string& n : getGameBuiltinNames()) declaredGlobals.insert(n);
        for (const auto& inst : code) {
            if (inst.op != Opcode::STORE_GLOBAL) continue;
            if (inst.operand.index() != 4) continue;
            size_t idx = std::get<size_t>(inst.operand);
            if (idx < constants.size()) declaredGlobals.insert(constants[idx]);
        }
        for (const auto& inst : code) {
            if (inst.op != Opcode::LOAD_GLOBAL) continue;
            if (inst.operand.index() != 4) continue;
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
        vm.run();
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
    } catch (const VMError& e) {
        std::vector<StackFrame> stack;
        for (const auto& f : vm.getCallStackSlice()) {
            stack.push_back({f.functionName, f.line, f.column});
        }
        std::string hint(vmRuntimeErrorHint(e.category, e.code));
        g_errorReporter.reportRuntimeError(vmErrorCategory(e.category), e.line, e.column, e.what(), stack, hint,
            vmErrorCodeString(e.category, e.code), vmRuntimeErrorDetail(e.category, e.code),
            e.lineEnd, e.columnEnd);
        return false;
    } catch (const std::exception& e) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, std::string("Kern stopped: ") + e.what(),
            "This may be an unexpected error from the runtime or compiler; try reducing the program to a minimal case.",
            "KERN-STOP-EXC", internalFailureDetail("kern_game script execution (std::exception)"));
        return false;
    } catch (...) {
        g_errorReporter.reportCompileError(ErrorCategory::Other, 0, 0, "Kern stopped: unknown exception",
            "This may be an internal error; try simplifying the code or reporting a minimal reproducer.",
            "KERN-STOP-UNKNOWN",
            internalFailureDetail("kern_game script execution (non-typed throw)") + "\n"
            "No exception message was available; enable sanitizers or a debugger if this persists.");
        return false;
    }
}

int main(int argc, char** argv) {
    VM vm;
    RuntimeGuardPolicy guards;
    guards.debugMode = false;
    guards.allowUnsafe = false;
    guards.enforcePointerBounds = true;
    guards.ffiEnabled = false;
    guards.sandboxEnabled = true;
    vm.setRuntimeGuards(guards);
    vm.setStepLimit(0);
    vm.setMaxCallDepth(8192);
    vm.setCallbackStepGuard(0);
    registerAllBuiltins(vm);
    registerGameBuiltins(vm);
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) args.push_back(argv[i]);
    vm.setCliArgs(std::move(args));

    if (argc <= 1) {
        std::cout << "Kern Game - Run a game script: kern_game game.kn" << std::endl;
        return 0;
    }

    std::string path = argv[1];
    std::ifstream f(path);
    if (!f) {
        g_errorReporter.setFilename(path);
        g_errorReporter.reportCompileError(ErrorCategory::FileError, 0, 0,
            "Could not open file: " + path, "Check that the file exists and is readable.", "FILE-OPEN",
            fileOpenErrorDetail());
        g_errorReporter.printSummary();
        return 1;
    }
    std::stringstream buf;
    buf << f.rdbuf();
    bool ok = runSource(vm, buf.str(), path);
    g_errorReporter.printSummary();
    return ok ? 0 : 1;
}
