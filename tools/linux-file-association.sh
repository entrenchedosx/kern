#!/usr/bin/env sh
# Optional: register .kn as text/x-kern for your user (Freedesktop).
# Usage: ./tools/linux-file-association.sh [/path/to/kern]
# Requires: xdg-mime, desktop-file-utils (xdg-mime), and a .desktop file.

set -e
MIME="text/x-kern"
KERN_BIN="${1:-kern}"
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
DESKTOP_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
MIME_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/mime"
mkdir -p "$DESKTOP_DIR" "$MIME_DIR/packages"

cp "$ROOT/tools/linux-kern-mime.xml" "$MIME_DIR/packages/kern-kn.xml"
update-mime-database "$MIME_DIR" 2>/dev/null || true

cat > "$DESKTOP_DIR/kern.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Kern
Exec=$KERN_BIN %f
MimeType=$MIME;
NoDisplay=true
EOF

xdg-mime default kern.desktop $MIME 2>/dev/null || echo "xdg-mime not available; install desktop-file-utils."

echo "Registered $MIME for *.kn (user). Default app for .kn: try xdg-open file.kn after setting PATH to kern."
