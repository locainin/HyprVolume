#!/usr/bin/env bash
set -euo pipefail

bin_dir="$HOME/.local/bin"
config_dir="$HOME/.config/hyprvolume"
default_config_dir="$HOME/.config/hyprvolume"
hypr_conf="$HOME/.config/hypr/hyprland.conf"
hypr_snippet="$HOME/.config/hypr/hyprvolume.conf"
purge_config=0
stop_running=1

# Rejects control characters that can break generated config lines.
validate_cli_path() {
  local option_name="$1"
  local option_value="$2"

  if [[ "$option_value" =~ [[:cntrl:]] ]]; then
    echo "Invalid path for $option_name: control characters are not allowed" >&2
    exit 1
  fi
}

while (($# > 0)); do
  case "$1" in
    --bin-dir)
      if (($# < 2)); then
        echo "Missing value after $1" >&2
        exit 1
      fi
      bin_dir="$2"
      shift 2
      ;;
    --config-dir)
      if (($# < 2)); then
        echo "Missing value after $1" >&2
        exit 1
      fi
      config_dir="$2"
      shift 2
      ;;
    --hypr-conf)
      if (($# < 2)); then
        echo "Missing value after $1" >&2
        exit 1
      fi
      hypr_conf="$2"
      shift 2
      ;;
    --hypr-snippet)
      if (($# < 2)); then
        echo "Missing value after $1" >&2
        exit 1
      fi
      hypr_snippet="$2"
      shift 2
      ;;
    --purge)
      purge_config=1
      shift
      ;;
    --no-stop)
      stop_running=0
      shift
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
done

validate_cli_path "--bin-dir" "$bin_dir"
validate_cli_path "--config-dir" "$config_dir"
validate_cli_path "--hypr-conf" "$hypr_conf"
validate_cli_path "--hypr-snippet" "$hypr_snippet"

legacy_config_dir="$HOME/.config/hypr-volume-osd"
legacy_snippet_dir="$(dirname "$hypr_snippet")"
legacy_snippet="$legacy_snippet_dir/hypr-volume-osd.conf"
remove_legacy_config_dir=0
hypr_root_dir="$(dirname "$hypr_conf")"

if [[ "$config_dir" == "$default_config_dir" ]]; then
  remove_legacy_config_dir=1
fi

if [[ "$stop_running" -eq 1 ]]; then
  pkill -x hyprvolume >/dev/null 2>&1 || true
  pkill -x hypr-volume-osd >/dev/null 2>&1 || true
fi

rm -f "$bin_dir/hyprvolume"
rm -f "$bin_dir/hypr-volume-osd"

cleanup_hypr_config_file() {
  local conf_path="$1"
  local tmp_file=""

  if [[ ! -f "$conf_path" ]]; then
    return 0
  fi

  start_marker="# >>> hyprvolume >>>"
  end_marker="# <<< hyprvolume <<<"
  legacy_start_marker="# >>> hypr-volume-osd >>>"
  legacy_end_marker="# <<< hypr-volume-osd <<<"
  tmp_file="$(mktemp)"
  awk \
    -v start="$start_marker" \
    -v end="$end_marker" \
    -v legacy_start="$legacy_start_marker" \
    -v legacy_end="$legacy_end_marker" '
    $0 ~ "^[[:space:]]*" start "[[:space:]]*$" || $0 ~ "^[[:space:]]*" legacy_start "[[:space:]]*$" { inside = 1; next }
    $0 ~ "^[[:space:]]*" end "[[:space:]]*$" || $0 ~ "^[[:space:]]*" legacy_end "[[:space:]]*$" { inside = 0; next }
    $0 ~ /^[[:space:]]*source[[:space:]]*=/ && ($0 ~ /hyprvolume/ || $0 ~ /hypr-volume-osd/) { next }
    inside == 0 { print }
  ' "$conf_path" > "$tmp_file"
  mv "$tmp_file" "$conf_path"
}

cleanup_hypr_config_file "$hypr_conf"
if [[ -d "$hypr_root_dir" ]]; then
  while IFS= read -r conf_file; do
    if [[ "$conf_file" == "$hypr_snippet" || "$conf_file" == "$legacy_snippet" ]]; then
      continue
    fi
    cleanup_hypr_config_file "$conf_file"
  done < <(find "$hypr_root_dir" -type f -name '*.conf')
fi

rm -f "$hypr_snippet"
rm -f "$legacy_snippet"

if [[ "$purge_config" -eq 1 ]]; then
  rm -rf "$config_dir"
  if [[ "$remove_legacy_config_dir" -eq 1 ]]; then
    rm -rf "$legacy_config_dir"
  fi
fi

echo "Uninstalled hyprvolume from $bin_dir/hyprvolume"
if [[ "$purge_config" -eq 1 ]]; then
  if [[ "$remove_legacy_config_dir" -eq 1 ]]; then
    echo "Removed config directories: $config_dir and $legacy_config_dir"
  else
    echo "Removed config directory: $config_dir"
  fi
else
  echo "Config directory retained: $config_dir"
fi
