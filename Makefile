# Minimal developer install: build kern and copy into PREFIX/bin (default: ~/.local/bin).
# Windows: use install.ps1 instead, or: cmake --build build --config Release && cmake --install build --prefix "%LOCALAPPDATA%\Programs\Kern" --config Release

.PHONY: build install

BUILD_DIR ?= build
PREFIX ?= $(HOME)/.local
CMAKE_BUILD_TYPE ?= Release

build:
	cmake -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
	cmake --build "$(BUILD_DIR)" --config Release --target kern

install: build
	cmake --install "$(BUILD_DIR)" --prefix "$(PREFIX)" --config Release
	@echo "Installed. Ensure $(PREFIX)/bin is on your PATH (e.g. export PATH=\"$(PREFIX)/bin:\$$PATH\")."
