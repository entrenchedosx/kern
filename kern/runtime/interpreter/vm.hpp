/* *
 * kern Virtual Machine - Executes bytecode
 */

#ifndef KERN_VM_HPP
#define KERN_VM_HPP

#include "bytecode.hpp"
#include "value.hpp"
#include "script_code.hpp"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <stack>
#include <tuple>
#include <functional>
#include <stdexcept>

namespace kern {

struct VMStackFrame {
    std::string functionName;
    int line = 0;
    int column = 0;
};

class VMError : public std::runtime_error {
public:
    int line = 0;
    int column = 0;
    int category = 0;  // 0 = other, 1 = Runtime, 2 = Type, 3 = Value, 4 = Division, 5 = Argument, 6 = Index, etc.
    VMError(const std::string& msg, int ln = 0, int col = 0, int cat = 0)
        : std::runtime_error(msg), line(ln), column(col), category(cat) {}
};

class VM {
public:
    VM();
    void setBytecode(Bytecode code);
    void setStringConstants(std::vector<std::string> constants);
    void setValueConstants(std::vector<Value> constants);
    void run();
    using BuiltinFn = std::function<Value(VM*, std::vector<ValuePtr>)>;
    void registerBuiltin(size_t index, BuiltinFn fn);
    void setGlobal(const std::string& name, ValuePtr value);
    /* * get global by name (for building stdlib modules). Returns nullptr if not set.*/
    ValuePtr getGlobal(const std::string& name) const;
    /* * snapshot all globals (used by import/export capture).*/
    std::unordered_map<std::string, ValuePtr> getGlobalsSnapshot() const;
    ValuePtr popStack();
    bool hasResult() const { return !stack_.empty(); }
    ValuePtr getResult();
    uint64_t getCycleCount() const { return cycleCount_; }
    void resetCycleCount() { cycleCount_ = 0; }
    /* * set max instructions per run (0 = unlimited). Throws VMError when exceeded.*/
    void setStepLimit(uint64_t limit) { stepLimit_ = limit; }
    uint64_t getStepLimit() const { return stepLimit_; }
    /* * set CLI arguments (e.g. from main argv). Used by cli_args() builtin.*/
    void setCliArgs(std::vector<std::string> args) { cliArgs_ = std::move(args); }
    const std::vector<std::string>& getCliArgs() const { return cliArgs_; }
    /* * call a value (function or builtin) with args. Used by map/filter/reduce. Returns result.*/
    ValuePtr callValue(ValuePtr callee, std::vector<ValuePtr> args);
    /* * get current call stack (function name + line) for error reporting.*/
    std::vector<VMStackFrame> getCallStack() const { return callStack_; }
    /* * run another script's bytecode in this VM (for import). Saves/restores main script state.*/
    void runSubScript(Bytecode code, std::vector<std::string> stringConstants, std::vector<Value> valueConstants);
    /* * script-requested exit code (set by exit_code(n) builtin). -1 = not set.*/
    void setScriptExitCode(int c) { scriptExitCode_ = c; }
    int getScriptExitCode() const { return scriptExitCode_; }

private:
    Bytecode code_;
    std::vector<std::string> stringConstants_;
    std::vector<Value> valueConstants_;
    std::vector<ValuePtr> stack_;
    std::vector<ValuePtr> locals_;
    std::unordered_map<std::string, ValuePtr> globals_;
    size_t ip_;
    std::vector<size_t> callFrames_;
    std::vector<std::vector<ValuePtr>> frameLocals_;
    std::unordered_map<size_t, BuiltinFn> builtins_;
    std::vector<BuiltinFn> builtinsVec_;  // fast path for builtin indices 0..255
    std::vector<size_t> tryStack_;
    ValuePtr lastThrown_;  // set when THROW jumps to catch; used by RETHROW
    std::vector<std::pair<ValuePtr, size_t>> iterStack_;
    std::vector<std::vector<std::pair<ValuePtr, std::vector<ValuePtr>>>> deferStack_;
    std::shared_ptr<ScriptCode> currentScript_;  // set during runSubScript so BUILD_FUNC can attach it
    std::shared_ptr<ScriptCode> entryScriptCache_;
    std::vector<std::tuple<Bytecode, std::vector<std::string>, std::vector<Value>>> codeFrameStack_;  // when calling into script function, push caller code
    void runDeferredCalls();
    uint64_t cycleCount_ = 0;
    uint64_t stepLimit_ = 0;  // 0 = no limit
    std::vector<std::string> cliArgs_;
    std::vector<VMStackFrame> callStack_;
    int scriptExitCode_ = -1;  // set by exit_code(n); -1 = not set

    std::string getOperandStr(const Instruction& inst);
    size_t getOperandU64(const Instruction& inst);
    void push(ValuePtr v);
    ValuePtr peek();
    void runInstruction(const Instruction& inst);
    void initBuiltins();
};

} // namespace kern

#endif // kERN_VM_HPP
