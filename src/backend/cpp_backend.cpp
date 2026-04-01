#include "backend/cpp_backend.hpp"

#include "packager/bundle_manifest.hpp"
#include "packager/bundle_writer.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

namespace kern {
namespace fs = std::filesystem;

namespace {

static std::vector<unsigned char> toBytes(const std::string& s) {
    return std::vector<unsigned char>(s.begin(), s.end());
}

static std::vector<unsigned char> readBinary(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static int runShell(const std::string& cmd) {
    return std::system(cmd.c_str());
}

static std::string toCMakePath(const std::string& p) {
    std::string out = p;
    for (char& c : out) if (c == '\\') c = '/';
    return out;
}

static std::string readText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool writeText(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

static std::string shellQuote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
}

/* * virtual paths in embedded bundles must be stable when the project is moved (relative to entry's directory).*/
static std::string portablePathRelativeToWorkspace(const fs::path& path, const fs::path& workspaceRoot) {
    std::error_code ec;
    fs::path can = fs::weakly_canonical(path, ec);
    if (ec) return path.generic_string();
    fs::path root = fs::weakly_canonical(workspaceRoot, ec);
    if (ec) return can.generic_string();
    fs::path rel = fs::relative(can, root, ec);
    if (ec) return can.generic_string();
    std::string s = rel.generic_string();
    for (char& c : s)
        if (c == '\\') c = '/';
    return s;
}

} // namespace

bool generateCppBundle(const IRProgram& program, const SplConfig& config, CppBackendResult& out, std::string& error) {
    if (program.modules.empty()) {
        error = "no modules available for backend generation";
        return false;
    }

    BundleManifest m;
    std::error_code ec;
    fs::path entryCan = fs::weakly_canonical(fs::path(config.entry), ec);
    if (ec) {
        error = "invalid entry path for bundle";
        return false;
    }
    fs::path workspaceRoot = entryCan.has_parent_path() ? entryCan.parent_path() : fs::current_path();
    m.entryModulePath = portablePathRelativeToWorkspace(entryCan, workspaceRoot);

    for (const auto& mod : program.modules) {
        EmbeddedFile f;
        f.virtualPath = portablePathRelativeToWorkspace(fs::path(mod.path), workspaceRoot);
        f.bytes = toBytes(mod.source);
        m.modules.push_back(std::move(f));
    }

    for (const auto& assetPath : config.assets) {
        fs::path p(assetPath);
        std::error_code ec2;
        fs::path can = fs::weakly_canonical(p, ec2);
        if (ec2 || !fs::exists(can)) continue;
        EmbeddedFile a;
        a.virtualPath = portablePathRelativeToWorkspace(can, workspaceRoot);
        a.bytes = readBinary(can);
        m.assets.push_back(std::move(a));
    }

    fs::path outPath = fs::path(config.output).parent_path() / "kernc_generated_main.cpp";
    ec.clear();
    fs::create_directories(outPath.parent_path(), ec);
    std::string writeErr;
    if (!writeBundleAsCppSource(m, outPath.string(), writeErr)) {
        error = writeErr;
        return false;
    }
    out.generatedCpp = outPath.string();
    out.entryPath = m.entryModulePath;
    return true;
}

bool buildStandaloneExe(const CppBackendResult& input, const SplConfig& config, const std::string& workspaceRoot, std::string& error) {
    fs::path output = fs::path(config.output);
    std::error_code ec;
    fs::create_directories(output.parent_path(), ec);

    fs::path tempRoot = output.parent_path() / ".kern-build";
    fs::path tempSrc = tempRoot / "src";
    fs::path tempBuild = tempRoot / "build";
    fs::create_directories(tempSrc, ec);
    fs::create_directories(tempBuild, ec);

    std::string optFlag = "/O2";
    if (!config.release || config.optimizationLevel <= 0) optFlag = "/Od";
    else if (config.optimizationLevel == 1) optFlag = "/O1";
    else if (config.optimizationLevel >= 3) optFlag = "/Ox";

    bool embedWinIcon = false;
#ifdef _WIN32
    if (!config.icon.empty()) {
        fs::path iconSrc(config.icon);
        ec.clear();
        iconSrc = fs::weakly_canonical(iconSrc, ec);
        if (!ec && fs::exists(iconSrc) && fs::is_regular_file(iconSrc)) {
            fs::path iconDest = tempSrc / "kern_app_icon.ico";
            fs::copy_file(iconSrc, iconDest, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                fs::path rcPath = tempSrc / "kern_app_icon.rc";
                if (writeText(rcPath, "IDI_ICON1 ICON \"kern_app_icon.ico\"\n"))
                    embedWinIcon = true;
            }
        }
    }
#endif

    fs::path cmakeFile = tempSrc / "CMakeLists.txt";
    std::ofstream cm(cmakeFile, std::ios::trunc);
    if (!cm) {
        error = "failed to create temporary CMakeLists for standalone build";
        return false;
    }
    cm << "cmake_minimum_required(VERSION 3.15)\n";
    cm << "project(kernc_standalone LANGUAGES CXX)\n";
    cm << "if(MSVC)\n";
    cm << "  cmake_policy(SET CMP0091 NEW)\n";
    cm << "  set(CMAKE_MSVC_RUNTIME_LIBRARY \"MultiThreaded$<$<CONFIG:Debug>:Debug>\")\n";
    cm << "endif()\n";
    if (embedWinIcon) {
        cm << "if(MSVC)\n";
        cm << "  enable_language(RC)\n";
        cm << "endif()\n";
    }
    cm << "set(CMAKE_CXX_STANDARD 17)\n";
    cm << "set(CMAKE_CXX_FLAGS_RELEASE \"${CMAKE_CXX_FLAGS_RELEASE} " << optFlag << "\")\n";
    const std::string w = toCMakePath(workspaceRoot);
    cm << "add_executable(kernc_standalone\n";
    cm << "  \"" << toCMakePath(input.generatedCpp) << "\"\n";
    cm << "  \"" << w << "/src/compiler/lexer.cpp\"\n";
    cm << "  \"" << w << "/src/compiler/parser.cpp\"\n";
    cm << "  \"" << w << "/src/compiler/codegen.cpp\"\n";
    cm << "  \"" << w << "/src/vm/value.cpp\"\n";
    cm << "  \"" << w << "/src/vm/vm.cpp\"\n";
    cm << "  \"" << w << "/src/errors.cpp\"\n";
    cm << "  \"" << w << "/src/stdlib_modules.cpp\"\n";
    cm << "  \"" << w << "/src/import_resolution.cpp\"\n";
    cm << "  \"" << w << "/src/process/process_module.cpp\"\n";
    cm << "  \"" << w << "/src/system/event_bus.cpp\"\n";
    cm << "  \"" << w << "/src/modules/system/input_module.cpp\"\n";
    cm << "  \"" << w << "/src/modules/system/vision_module.cpp\"\n";
    cm << "  \"" << w << "/src/modules/system/render_module.cpp\"\n";
    if (embedWinIcon)
        cm << "  \"" << toCMakePath((tempSrc / "kern_app_icon.rc").string()) << "\"\n";
    cm << ")\n";
    cm << "target_include_directories(kernc_standalone PRIVATE \"" << w << "/src\" \"" << w << "\")\n";
    cm << "if(WIN32)\n";
    cm << "  target_sources(kernc_standalone PRIVATE \"" << w << "/src/vm/http_get_winhttp.cpp\")\n";
    cm << "  target_link_libraries(kernc_standalone PRIVATE psapi winhttp wininet)\n";
    cm << "endif()\n";
    if (!config.console) {
        cm << "if(MSVC)\n";
        cm << "  target_link_options(kernc_standalone PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)\n";
        cm << "endif()\n";
    }
    cm << "set(KERN_STANDALONE_REPO \"" << w << "\")\n";
    cm << "include(\"" << w << "/cmake/kern_standalone_graphics.cmake\")\n";
    cm.close();

    std::ostringstream cfgCmd;
    cfgCmd << "cmake -S \"" << tempSrc.string() << "\" -B \"" << tempBuild.string() << "\"";
    fs::path vcpkgToolchain = fs::path(workspaceRoot) / "tools" / "vcpkg" / "scripts" / "buildsystems" / "vcpkg.cmake";
    if (fs::exists(vcpkgToolchain)) {
        cfgCmd << " -DCMAKE_TOOLCHAIN_FILE=\"" << toCMakePath(vcpkgToolchain.string()) << "\"";
#ifdef _WIN32
        cfgCmd << " -DVCPKG_TARGET_TRIPLET=x64-windows-static";
#endif
    }
    if (runShell(cfgCmd.str()) != 0) {
        error = "cmake configure failed for standalone target";
        return false;
    }
    std::ostringstream bldCmd;
    bldCmd << "cmake --build \"" << tempBuild.string() << "\" --config " << (config.release ? "Release" : "Debug");
    if (runShell(bldCmd.str()) != 0) {
        error = "cmake build failed for standalone target";
        return false;
    }

    fs::path built = tempBuild / (config.release ? "Release" : "Debug") / "kernc_standalone.exe";
    if (!fs::exists(built)) built = tempBuild / "kernc_standalone.exe";
    if (!fs::exists(built)) {
        error = "standalone executable not produced by build";
        return false;
    }

    fs::copy_file(built, output, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        error = "failed to copy built exe to output: " + output.string();
        return false;
    }
    return true;
}

bool buildKernelArtifacts(const SplConfig& config, const std::string& workspaceRoot, std::string& kernelBinOut, std::string& isoOut, std::string& error) {
    fs::path entry = fs::path(config.entry);
    std::error_code ec;
    entry = fs::weakly_canonical(entry, ec);
    if (ec || !fs::exists(entry)) {
        error = "kernel entry file not found: " + config.entry;
        return false;
    }

    fs::path outPath = fs::path(config.output);
    if (outPath.empty()) outPath = fs::path("dist/kernel.bin");
    if (!outPath.is_absolute()) outPath = fs::current_path() / outPath;
    fs::create_directories(outPath.parent_path(), ec);

    fs::path tempRoot = outPath.parent_path() / ".kern-kernel-build";
    fs::path tempSrc = tempRoot / "src";
    fs::path tempObj = tempRoot / "obj";
    fs::path isoRoot = tempRoot / "iso";
    fs::create_directories(tempSrc, ec);
    fs::create_directories(tempObj, ec);
    fs::create_directories(isoRoot / "boot" / "grub", ec);

    std::string entrySource = readText(entry);
    if (entrySource.empty()) {
        error = "failed to read kernel entry source";
        return false;
    }

    std::vector<std::string> bootLines;
    std::regex printRe(R"re(\bscreen\.print\s*\(\s*"([^"]*)"\s*\))re");
    for (std::sregex_iterator it(entrySource.begin(), entrySource.end(), printRe), end; it != end; ++it) {
        bootLines.push_back((*it)[1].str());
    }
    if (bootLines.empty()) bootLines.push_back("Kern kernel booted");

    std::ostringstream kernelCpp;
    kernelCpp
        << "#include <stdint.h>\n"
        << "#include <stddef.h>\n\n"
        << "extern \"C\" void* memset(void* dst, int c, size_t n) { unsigned char* p=(unsigned char*)dst; for(size_t i=0;i<n;++i) p[i]=(unsigned char)c; return dst; }\n"
        << "extern \"C\" void* memcpy(void* d, const void* s, size_t n) { unsigned char* dd=(unsigned char*)d; const unsigned char* ss=(const unsigned char*)s; for(size_t i=0;i<n;++i) dd[i]=ss[i]; return d; }\n\n"
        << "namespace splk {\n"
        << "static volatile uint16_t* const VGA = (volatile uint16_t*)0xB8000;\n"
        << "static uint16_t row = 0, col = 0;\n"
        << "static inline uint16_t vga_entry(char c, uint8_t color){ return (uint16_t)c | ((uint16_t)color << 8); }\n"
        << "static inline void port_write8(uint16_t port, uint8_t value){ __asm__ __volatile__(\"outb %0, %1\" : : \"a\"(value), \"Nd\"(port)); }\n"
        << "static inline uint8_t port_read8(uint16_t port){ uint8_t ret; __asm__ __volatile__(\"inb %1, %0\" : \"=a\"(ret) : \"Nd\"(port)); return ret; }\n"
        << "static inline void cpu_hlt(){ __asm__ __volatile__(\"hlt\"); }\n"
        << "static inline void cpu_sti(){ __asm__ __volatile__(\"sti\"); }\n"
        << "static inline void cpu_cli(){ __asm__ __volatile__(\"cli\"); }\n"
        << "static inline void memory_barrier(){ __asm__ __volatile__(\"\" ::: \"memory\"); }\n"
        << "struct Thread { void (*entry)(); int priority; bool runnable; };\n"
        << "static Thread g_threads[16]; static int g_thread_count = 0; static int g_current = 0;\n"
        << "void thread_create(void (*fn)(), int pri){ if(g_thread_count < 16){ g_threads[g_thread_count++] = {fn, pri, true}; } }\n"
        << "void schedule_round_robin(){ if(g_thread_count==0) return; g_current = (g_current + 1) % g_thread_count; if(g_threads[g_current].runnable && g_threads[g_current].entry) g_threads[g_current].entry(); }\n"
        << "void vga_putc(char c, uint8_t color){ if(c=='\\n'){ col=0; ++row; return; } VGA[row*80+col] = vga_entry(c, color); if(++col >= 80){ col=0; ++row; } }\n"
        << "void screen_print(const char* s){ for(size_t i=0; s[i]; ++i) vga_putc(s[i], 0x0F); }\n"
        << "void serial_init(){ port_write8(0x3F8 + 1, 0x00); port_write8(0x3F8 + 3, 0x80); port_write8(0x3F8 + 0, 0x03); port_write8(0x3F8 + 1, 0x00); port_write8(0x3F8 + 3, 0x03); port_write8(0x3F8 + 2, 0xC7); port_write8(0x3F8 + 4, 0x0B); }\n"
        << "int serial_ready(){ return port_read8(0x3F8 + 5) & 0x20; }\n"
        << "void serial_write(char c){ while(serial_ready()==0){} port_write8(0x3F8, (uint8_t)c); }\n"
        << "void serial_log(const char* s){ for(size_t i=0; s[i]; ++i) serial_write(s[i]); serial_write('\\n'); }\n"
        << "void panic(const char* msg){ screen_print(\"KERNEL PANIC: \"); screen_print(msg); serial_log(msg); for(;;) cpu_hlt(); }\n"
        << "void idt_init(){}\n"
        << "void paging_init(){}\n"
        << "void heap_init(){}\n"
        << "}\n\n"
        << "extern \"C\" void kernel_main(){\n"
        << "  splk::serial_init();\n"
        << "  splk::idt_init();\n"
        << "  splk::paging_init();\n"
        << "  splk::heap_init();\n";
    for (const auto& line : bootLines) {
        std::string esc = line;
        for (size_t i = 0; i < esc.size(); ++i) if (esc[i] == '"') esc.insert(i++, "\\");
        kernelCpp << "  splk::screen_print(\"" << esc << "\\n\");\n";
        kernelCpp << "  splk::serial_log(\"" << esc << "\");\n";
    }
    kernelCpp
        << "  for(;;){ splk::schedule_round_robin(); splk::cpu_hlt(); }\n"
        << "}\n";

    fs::path kernelCppPath = tempSrc / "kernel_entry.cpp";
    if (!writeText(kernelCppPath, kernelCpp.str())) {
        error = "failed writing generated kernel source";
        return false;
    }

    fs::path linkerPath = tempSrc / "linker.ld";
    std::string linkerText =
        "ENTRY(kernel_main)\n"
        "SECTIONS {\n"
        "  . = 1M;\n"
        "  .text : { *(.text*) }\n"
        "  .rodata : { *(.rodata*) }\n"
        "  .data : { *(.data*) }\n"
        "  .bss : { *(.bss*) *(COMMON) }\n"
        "}\n";
    if (!config.kernelLinkerScript.empty()) {
        fs::path custom = config.kernelLinkerScript;
        if (!custom.is_absolute()) custom = fs::path(workspaceRoot) / custom;
        std::string customText = readText(custom);
        if (!customText.empty()) linkerText = customText;
    }
    if (!writeText(linkerPath, linkerText)) {
        error = "failed writing linker script";
        return false;
    }

    fs::path grubCfg = isoRoot / "boot" / "grub" / "grub.cfg";
    std::string grubText =
        "set timeout=0\n"
        "set default=0\n"
        "menuentry \"Kern Kernel\" {\n"
        "  multiboot2 /boot/kernel.bin\n"
        "  boot\n"
        "}\n";
    if (!writeText(grubCfg, grubText)) {
        error = "failed writing grub config";
        return false;
    }

    const std::string prefix = config.kernelCrossPrefix.empty() ? "" : config.kernelCrossPrefix;
    const std::string cxx = prefix + "g++";
    const std::string ld = prefix + "ld";
    fs::path objPath = tempObj / "kernel.o";
    kernelBinOut = outPath.string();

    auto mkCompile = [&](const std::string& cc) {
        std::ostringstream cmd;
        cmd << cc << " -c " << shellQuote(kernelCppPath.string())
            << " -o " << shellQuote(objPath.string())
            << " -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -nostdlib -m64 "
            << (config.release ? "-O2" : "-O0");
        return cmd.str();
    };
    auto mkLink = [&](const std::string& linker) {
        std::ostringstream cmd;
        cmd << linker << " -n -T " << shellQuote(linkerPath.string())
            << " -o " << shellQuote(kernelBinOut)
            << " " << shellQuote(objPath.string());
        return cmd.str();
    };

    bool compileOk = (runShell(mkCompile(cxx)) == 0);
    if (!compileOk) {
        compileOk = (runShell(mkCompile("g++")) == 0);
    }
    if (!compileOk) {
        compileOk = (runShell(mkCompile("clang++")) == 0);
    }
    if (!compileOk) {
        error = "kernel compile failed; install x86_64-elf-g++ or provide g++/clang++";
        return false;
    }

    bool linkOk = (runShell(mkLink(ld)) == 0);
    if (!linkOk) {
        linkOk = (runShell(mkLink("ld")) == 0);
    }
    if (!linkOk) {
        linkOk = (runShell(mkLink("ld.lld")) == 0);
    }
    if (!linkOk) {
        error = "kernel link failed; install x86_64-elf-ld/ld.lld";
        return false;
    }

    if (config.kernelBuildIso) {
        fs::path isoKernel = isoRoot / "boot" / "kernel.bin";
        fs::copy_file(kernelBinOut, isoKernel, fs::copy_options::overwrite_existing, ec);
        fs::path isoPath = outPath.parent_path() / "bootable.iso";
        isoOut = isoPath.string();
        std::ostringstream isoCmd;
        isoCmd << "grub-mkrescue -o " << shellQuote(isoOut) << " " << shellQuote(isoRoot.string());
        if (runShell(isoCmd.str()) != 0) {
            error = "ISO build failed; install grub-mkrescue and xorriso";
            return false;
        }
    }

    if (config.kernelRunQemu) {
        std::ostringstream qemuCmd;
        qemuCmd << "qemu-system-x86_64 ";
        if (!isoOut.empty()) qemuCmd << "-cdrom " << shellQuote(isoOut);
        else qemuCmd << "-kernel " << shellQuote(kernelBinOut);
        qemuCmd << " -serial stdio";
        if (runShell(qemuCmd.str()) != 0) {
            error = "qemu run failed; install qemu-system-x86_64";
            return false;
        }
    }

    return true;
}

} // namespace kern
