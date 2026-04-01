# PyInstaller spec for Kern Build GUI (PySide6).
# Run from repo:  python -m PyInstaller --noconfirm kern-build-gui/packaging/kern-gui.spec
# Or use:         build\build_ecosystem.ps1

import os

from PyInstaller.utils.hooks import collect_all

block_cipher = None
spec_dir = os.path.dirname(os.path.abspath(SPEC))
gui_root = os.path.normpath(os.path.join(spec_dir, ".."))

datas, binaries, hiddenimports = collect_all("PySide6")

a = Analysis(
    [os.path.join(gui_root, "kern_build_gui", "__main__.py")],
    pathex=[gui_root],
    binaries=binaries,
    datas=datas,
    hiddenimports=hiddenimports,
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
    [],
    exclude_binaries=True,
    name="kern-gui",
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

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name="kern-gui",
)
