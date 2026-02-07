#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
cc="${CC:-cc}"

if ! command -v pkg-config >/dev/null 2>&1; then
  echo "pkg-config is required to generate compile_commands.json" >&2
  exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "jq is required to generate compile_commands.json" >&2
  exit 1
fi

read -r -a pkg_cflags <<<"$(pkg-config --cflags gtk4 gtk4-layer-shell-0)"

common_flags=(
  -D_FORTIFY_SOURCE=3
  -Isrc
  -std=c11
  -O2
  -g
  -fno-omit-frame-pointer
  -fstack-protector-strong
  -Wall
  -Wextra
  -Wformat=2
  -Wconversion
  -Wshadow
  -Wpedantic
)

mapfile -t src_files < <(find "$repo_root/src" -type f -name '*.c' | sort)
if [[ ${#src_files[@]} -eq 0 ]]; then
  echo "No C source files found in $repo_root/src" >&2
  exit 1
fi

tmp_entries="$(mktemp)"
trap 'rm -f "$tmp_entries"' EXIT

for src in "${src_files[@]}"; do
  rel_src="${src#"$repo_root"/}"
  rel_obj="/tmp/hyprvolume-compdb/${rel_src#src/}"
  rel_obj="${rel_obj%.c}.o"

  jq -n \
    --arg directory "$repo_root" \
    --arg file "$rel_src" \
    --arg cc "$cc" \
    '{directory: $directory, file: $file, arguments: ([$cc] + $ARGS.positional)}' \
    --args -- "${common_flags[@]}" "${pkg_cflags[@]}" -c "$rel_src" -o "$rel_obj" >> "$tmp_entries"
done

jq -s '.' "$tmp_entries" > "$repo_root/compile_commands.json"

echo "Wrote $repo_root/compile_commands.json"
