# PyInstaller: portable IDE as ide.exe with sibling compiler/ populated by build_shareable_ide.ps1.
#   cd kern-ide && python -m PyInstaller --noconfirm packaging/shareable-ide.spec

import os

block_cipher = None
spec_dir = os.path.dirname(os.path.abspath(SPEC))
ide_root = os.path.normpath(os.path.join(spec_dir, ".."))
repo_root = os.path.normpath(os.path.join(ide_root, ".."))

a = Analysis(
    [os.path.join(ide_root, "main.py")],
    pathex=[ide_root, repo_root],
    binaries=[],
    datas=[],
    hiddenimports=[
        "app",
        "app.ide",
        "app.editor",
        "app.filesystem",
        "app.runner",
        "app.theme",
        "app.state",
        "services.process_runner",
        "services.repl_session",
        "services.suggestions",
        "services.module_resolution",
        "services.stdlib_exports",
        "services.errors",
        "services.diagnostics",
        "ui.layout",
        "ui.theme",
        "models.events",
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name="ide",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
