#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION="$(sed -n 's/^project(LumaTerm VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt)"
ARCH="$(dpkg --print-architecture)"
DIST="dist"
STAGE="luma-terminal-${VERSION}-linux-${ARCH}"

echo "==> Building Luma Terminal ${VERSION} (${ARCH})"
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

rm -rf "$DIST"
mkdir -p "$DIST"

echo "==> Portable bundle"
cmake --install build --prefix "$DIST/$STAGE"
tar -C "$DIST" -czf "$DIST/${STAGE}.tar.gz" "$STAGE"

echo "==> Debian package"
DEBROOT="$DIST/deb-root"
cmake --install build --prefix "$DEBROOT/usr"
mkdir -p "$DEBROOT/DEBIAN"
cat > "$DEBROOT/DEBIAN/control" <<EOF
Package: luma-terminal
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Depends: libc6, libstdc++6, libqt6core6t64, libqt6gui6, libqt6qml6, libqt6network6, libqt6quick6, libqt6quickcontrols2-6, libqt6svg6, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-window, qml6-module-qtquick-effects, qml6-module-qtcore
Maintainer: Luma Terminal <luma@localhost>
Description: Luma Terminal - modern macOS-style terminal
 A fast Linux terminal built with Qt Quick and the libghostty-vt core.
 It features tabs, nested split panes, scrollback search, selection,
 themes, IME support, a command palette and a settings panel.
EOF
cat > "$DEBROOT/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database -q /usr/share/applications || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true
fi
exit 0
EOF
chmod 755 "$DEBROOT/DEBIAN/postinst"
dpkg-deb --build --root-owner-group "$DEBROOT" "$DIST/luma-terminal_${VERSION}_${ARCH}.deb" >/dev/null
rm -rf "$DEBROOT"

(
  cd "$DIST"
  sha256sum "${STAGE}.tar.gz" "luma-terminal_${VERSION}_${ARCH}.deb" > SHA256SUMS
)

echo "==> Artifacts"
ls -lh "$DIST"/*.deb "$DIST"/*.tar.gz "$DIST/SHA256SUMS"
