#include "packager/bundle_writer.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace kern {
namespace {

static std::string escapeForCpp(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else out += c;
    }
    return out;
}

static void writeByteArray(std::ostream& os, const std::vector<unsigned char>& bytes, const std::string& varName) {
    os << "static const unsigned char " << varName << "[] = {";
    for (size_t i = 0; i < bytes.size(); ++i) {
        if ((i % 20) == 0) os << "\n  ";
        os << static_cast<int>(bytes[i]);
        if (i + 1 < bytes.size()) os << ", ";
    }
    os << "\n};\n";
}

} // namespace

bool writeBundleAsCppSource(const BundleManifest& manifest, const std::string& outCppPath, std::string& error) {
    std::ofstream out(outCppPath, std::ios::out | std::ios::trunc);
    if (!out) {
        error = "failed to create generated source: " + outCppPath;
        return false;
    }

    out << "#ifdef _MSC_VER\n#define _CRT_SECURE_NO_WARNINGS 1\n#endif\n";
    out << "#include \"compiler/lexer.hpp\"\n";
    out << "#include \"compiler/parser.hpp\"\n";
    out << "#include \"compiler/codegen.hpp\"\n";
    out << "#include \"vm/vm.hpp\"\n";
    out << "#include \"vm/builtins.hpp\"\n";
    out << "#include \"import_resolution.hpp\"\n";
    out << "#ifdef KERN_BUILD_GAME\n#include \"game/game_builtins.hpp\"\n#endif\n";
    out << "#include <unordered_map>\n#include <string>\n#include <vector>\n#include <iostream>\n";
    out << "#include <cctype>\n";
    out << "#include <cstdlib>\n";
    out << "#ifdef _WIN32\n";
    out << "#ifndef WIN32_LEAN_AND_MEAN\n#define WIN32_LEAN_AND_MEAN\n#endif\n";
    out << "#include <windows.h>\n#endif\n";
    out << "using namespace kern;\n";

    for (size_t i = 0; i < manifest.modules.size(); ++i) {
        writeByteArray(out, manifest.modules[i].bytes, "g_mod_" + std::to_string(i));
    }
    for (size_t i = 0; i < manifest.assets.size(); ++i) {
        writeByteArray(out, manifest.assets[i].bytes, "g_asset_" + std::to_string(i));
    }

    out << "static std::unordered_map<std::string, std::string> g_modules;\n";
    out << "static void initModules() {\n";
    for (size_t i = 0; i < manifest.modules.size(); ++i) {
        out << "  g_modules[\"" << escapeForCpp(manifest.modules[i].virtualPath) << "\"] = std::string((const char*)g_mod_" << i
            << ", (const char*)g_mod_" << i << " + sizeof(g_mod_" << i << "));\n";
    }
    out << "}\n";

    out << "static bool runSource(VM& vm, const std::string& source, const std::string& filename) {\n";
    out << "  try {\n";
    out << "    Lexer lx(source);\n";
    out << "    auto tokens = lx.tokenize();\n";
    out << "    Parser p(std::move(tokens));\n";
    out << "    auto prog = p.parse();\n";
    out << "    CodeGenerator g; auto code = g.generate(std::move(prog));\n";
    out << "    vm.setBytecode(code); vm.setStringConstants(g.getConstants()); vm.setValueConstants(g.getValueConstants());\n";
    out << "    vm.run(); return true;\n";
    out << "  } catch (const std::exception& e) { std::cerr << filename << \": \" << e.what() << std::endl; return false; }\n";
    out << "  catch (...) { std::cerr << filename << \": unknown exception\" << std::endl; return false; }\n";
    out << "}\n";

    out << "int main(int argc, char** argv) {\n";
    out << "  (void)argc; (void)argv;\n";
    out << "  int exitCode = 1;\n";
    out << "  try {\n";
    out << "  initModules();\n";
    out << "  VM vm; registerAllBuiltins(vm);\n";
    out << "#ifdef KERN_BUILD_GAME\n  registerGameBuiltins(vm);\n#endif\n";
    out << "  setEmbeddedModuleProvider([](const std::string& req, std::string& src, std::string* logicalPath) {\n";
    out << "    const bool trace = std::getenv(\"KERNC_TRACE_IMPORTS\") != nullptr;\n";
    out << "    std::string key = req;\n";
    out << "    for (char& c : key) if (c == '\\\\') c = '/';\n";
    out << "    while (key.rfind(\"./\", 0) == 0) key.erase(0, 2);\n";
    out << "    if (key.size() < 4 || key.substr(key.size()-4) != \".kn\") key += \".kn\";\n";
    out << "    auto lower = [](std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; };\n";
    out << "    const std::string keyLower = lower(key);\n";
    out << "    if (trace) std::cerr << \"[kern-embed] request=\" << key << \" modules=\" << g_modules.size() << std::endl;\n";
    out << "    auto it = g_modules.find(key);\n";
    out << "    if (it == g_modules.end()) {\n";
    out << "      const std::string tail = std::string(\"/\") + key;\n";
    out << "      const std::string tailLower = lower(tail);\n";
    out << "      for (const auto& kv : g_modules) {\n";
    out << "        std::string norm = kv.first;\n";
    out << "        for (char& c : norm) if (c == '\\\\') c = '/';\n";
    out << "        std::string normLower = lower(norm);\n";
    out << "        if (normLower == keyLower || (normLower.size() >= tailLower.size() && normLower.compare(normLower.size() - tailLower.size(), tailLower.size(), tailLower) == 0) || normLower.find(keyLower) != std::string::npos) {\n";
    out << "          src = kv.second;\n";
    out << "          if (logicalPath) *logicalPath = norm;\n";
    out << "          return true;\n";
    out << "        }\n";
    out << "      }\n";
    out << "      return false;\n";
    out << "    }\n";
    out << "    src = it->second;\n";
    out << "    if (logicalPath) *logicalPath = key;\n";
    out << "    return true;\n";
    out << "  });\n";
    out << "  registerImportBuiltin(vm);\n";
    out << "  auto it = g_modules.find(\"" << escapeForCpp(manifest.entryModulePath) << "\");\n";
    out << "  if (it == g_modules.end()) { std::cerr << \"embedded entry missing\" << std::endl; exitCode = 1; }\n";
    out << "  else {\n";
    out << "    bool ok = runSource(vm, it->second, \"" << escapeForCpp(manifest.entryModulePath) << "\");\n";
    out << "    exitCode = ok ? 0 : 1;\n";
    out << "  }\n";
    out << "  clearEmbeddedModuleProvider();\n";
    out << "  } catch (const std::exception& e) {\n";
    out << "    std::cerr << \"fatal: \" << e.what() << std::endl;\n";
    out << "    exitCode = 1;\n";
    out << "  } catch (...) {\n";
    out << "    std::cerr << \"fatal: unknown exception\" << std::endl;\n";
    out << "    exitCode = 1;\n";
    out << "  }\n";
    out << "#ifdef _WIN32\n";
    out << "  std::cerr.flush();\n";
    out << "  if (exitCode != 0) {\n";
    out << "    if (GetConsoleWindow() != nullptr && std::getenv(\"KERN_STANDALONE_NO_PAUSE\") == nullptr) {\n";
    out << "      std::cerr << \"\\nPress Enter to close...\" << std::endl;\n";
    out << "      std::string __kern_standalone_pause;\n";
    out << "      std::getline(std::cin, __kern_standalone_pause);\n";
    out << "    } else if (GetConsoleWindow() == nullptr) {\n";
    out << "      MessageBoxA(nullptr, \"This program exited with an error. In kern-to-exe, enable Console based (terminal) and rebuild, or run this .exe from cmd.exe to see details.\", \"Kern standalone\", MB_OK | MB_ICONERROR);\n";
    out << "    }\n";
    out << "  }\n";
    out << "#endif\n";
    out << "  return exitCode;\n";
    out << "}\n";

    return true;
}

} // namespace kern
