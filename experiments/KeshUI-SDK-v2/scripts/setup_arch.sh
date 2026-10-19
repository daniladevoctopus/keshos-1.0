#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

printf '\n[KeshUI] Installing Arch/CachyOS dependencies...\n'
sudo pacman -S --needed --noconfirm base-devel cmake ninja pkgconf libx11 cairo pango

printf '\n[KeshUI] Configuring...\n'
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

printf '\n[KeshUI] Building runtime, KXamlC and gallery...\n'
cmake --build build --parallel

printf '\n[KeshUI] Validating XAML and tests...\n'
./build/kxc --check demo/MainWindow.xaml
ctest --test-dir build --output-on-failure

cat <<'EOF'

Build complete.
Run the gallery with:

  ./build/keshui_gallery

Inspect generated XAML C++ with:

  sed -n '1,220p' build/generated/MainWindow.g.hpp

EOF
