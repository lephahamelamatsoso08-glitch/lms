#!/usr/bin/env bash
# Makes the Windows demo package: dist/lms-windows-demo.zip
# Usage:  ./tools/make_release_zip.sh              only make the zip
#         ./tools/make_release_zip.sh --publish    also publish it as GitHub pre-release v0.9-demo
set -euo pipefail
cd "$(dirname "$0")/.."

PKG=dist/lms
ZIP=dist/lms-windows-demo.zip
TAG=v0.9-demo

# 1. Make sure the package folder exists and is complete
if [ ! -f "$PKG/lms.exe" ]; then
  echo "Package not found, creating it with package_windows.sh ..."
  ./tools/package_windows.sh
fi
[ -f "$PKG/lms.exe" ]  || { echo "ERROR: $PKG/lms.exe is missing"; exit 1; }
[ -d "$PKG/platforms" ] || { echo "ERROR: $PKG/platforms is missing"; exit 1; }

# 2. Add the launcher and the licence note, and remove old data files
rm -f "$PKG"/*.db
printf '@echo off\r\nlms.exe --db demo.db --demo\r\n' > "$PKG/start_demo.bat"
cat > "$PKG/LICENSES.txt" <<'EOF'
Moderated LMS, CS4433 project.
This program uses Qt 6 under the GNU LGPL v3 (https://www.qt.io/licensing), linked
dynamically. SQLite is in the public domain. libsodium uses the ISC licence.
Demo accounts (start_demo.bat): admin, lecturer1, hod1, student1, student2.
All demo passwords: Passw0rd1
EOF

# 3. Make the zip (zip if it is installed, otherwise PowerShell)
rm -f "$ZIP"
if command -v zip >/dev/null 2>&1; then
  (cd dist && zip -qr lms-windows-demo.zip lms)
else
  echo "zip is not installed, using PowerShell instead"
  powershell.exe -NoProfile -Command \
    "Compress-Archive -Path '$(cygpath -w "$PKG")' -DestinationPath '$(cygpath -w "$ZIP")' -Force"
fi
echo "Created:"
ls -l "$ZIP"

# 4. Optional: publish on GitHub
if [ "${1:-}" = "--publish" ]; then
  if gh release view "$TAG" >/dev/null 2>&1; then
    gh release upload "$TAG" "$ZIP" --clobber
  else
    gh release create "$TAG" "$ZIP" --prerelease \
      --title "Windows demo package (not the final release)" \
      --notes "Unzip, then double-click start_demo.bat. Demo accounts: admin, lecturer1, hod1, student1, student2. Password: Passw0rd1."
  fi
  gh release view "$TAG" --web
fi
