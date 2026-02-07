#!/usr/bin/env bash
set -euo pipefail

bin_source="./hyprvolume"
bin_dir="$HOME/.local/bin"
config_dir="$HOME/.config/hyprvolume"
default_config_dir="$HOME/.config/hyprvolume"
config_file="$config_dir/config.json"
default_config="./assets/default-config.json"
style_source="./assets/default-style.css"
style_file="$config_dir/style.css"
hypr_conf="$HOME/.config/hypr/hyprland.conf"
hypr_snippet="$HOME/.config/hypr/hyprvolume.conf"
start_now=1
overwrite_config=0
overwrite_style=0
enable_slide_value="true"

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
    --bin-source)
      if (($# < 2)); then
        echo "Missing value after $1" >&2
        exit 1
      fi
      bin_source="$2"
      shift 2
      ;;
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
      config_file="$config_dir/config.json"
      style_file="$config_dir/style.css"
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
    --default-config)
      if (($# < 2)); then
        echo "Missing value after $1" >&2
        exit 1
      fi
      default_config="$2"
      shift 2
      ;;
    --style-source)
      if (($# < 2)); then
        echo "Missing value after $1" >&2
        exit 1
      fi
      style_source="$2"
      shift 2
      ;;
    --no-start)
      start_now=0
      shift
      ;;
    --overwrite-config)
      overwrite_config=1
      shift
      ;;
    --overwrite-style)
      overwrite_style=1
      shift
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
done

validate_cli_path "--bin-source" "$bin_source"
validate_cli_path "--bin-dir" "$bin_dir"
validate_cli_path "--config-dir" "$config_dir"
validate_cli_path "--hypr-conf" "$hypr_conf"
validate_cli_path "--hypr-snippet" "$hypr_snippet"
validate_cli_path "--default-config" "$default_config"
validate_cli_path "--style-source" "$style_source"

legacy_config_dir="$HOME/.config/hypr-volume-osd"
legacy_config_file="$legacy_config_dir/config.json"
legacy_snippet_dir="$(dirname "$hypr_snippet")"
legacy_snippet="$legacy_snippet_dir/hypr-volume-osd.conf"
allow_legacy_migration=0

if [[ "$config_dir" == "$default_config_dir" ]]; then
  allow_legacy_migration=1
fi

if [[ ! -x "$bin_source" ]]; then
  echo "Binary not found or not executable: $bin_source" >&2
  echo "Build first with: make" >&2
  exit 1
fi

if [[ ! -f "$default_config" ]]; then
  echo "Default config not found: $default_config" >&2
  exit 1
fi

if [[ ! -f "$style_source" ]]; then
  echo "Default style file not found: $style_source" >&2
  exit 1
fi

if [[ "$overwrite_config" -eq 1 ]]; then
  overwrite_style=1
fi

exec_bin_escaped="$(printf '%q' "$bin_dir/hyprvolume")"
config_file_escaped="$(printf '%q' "$config_file")"
hypr_snippet_escaped="${hypr_snippet//\\/\\\\}"
hypr_snippet_escaped="${hypr_snippet_escaped// /\\ }"
style_file_json_escaped="${style_file//\\/\\\\}"
style_file_json_escaped="${style_file_json_escaped//\"/\\\"}"
config_from_default=0

mkdir -p "$bin_dir"
install -m 0755 "$bin_source" "$bin_dir/hyprvolume"

mkdir -p "$config_dir"
if [[ "$overwrite_config" -eq 1 ]]; then
  install -m 0644 "$default_config" "$config_file"
  config_from_default=1
elif [[ -f "$config_file" ]]; then
  echo "Keeping existing config: $config_file"
elif [[ "$allow_legacy_migration" -eq 1 && -f "$legacy_config_file" ]]; then
  install -m 0644 "$legacy_config_file" "$config_file"
  echo "Migrated legacy config to: $config_file"
else
  install -m 0644 "$default_config" "$config_file"
  config_from_default=1
fi

if [[ "$overwrite_style" -eq 1 || ! -f "$style_file" ]]; then
  install -m 0644 "$style_source" "$style_file"
fi

if [[ "$config_from_default" -eq 1 ]]; then
  tmp_config="$(mktemp)"
  awk \
    -v css_path="$style_file_json_escaped" '
    $0 ~ /^[[:space:]]*"css_file"[[:space:]]*:/ {
      print "  \"css_file\": \"" css_path "\","
      next
    }
    $0 ~ /^[[:space:]]*"css_replace"[[:space:]]*:/ {
      print "  \"css_replace\": true,"
      next
    }
    { print }
  ' "$config_file" > "$tmp_config"
  mv "$tmp_config" "$config_file"
fi

if ! command -v jq >/dev/null 2>&1; then
  echo "jq is required to parse $config_file. Install jq and rerun make install." >&2
  exit 1
fi

enable_slide_value="$(jq -r '
  if has("enable_slide") then
    .enable_slide
  else
    true
  end
  | if type == "boolean" then
      if . then "true" else "false" end
    else
      "invalid"
    end
' "$config_file" 2>/dev/null || printf 'invalid')"
if [[ "$enable_slide_value" != "true" && "$enable_slide_value" != "false" ]]; then
  echo "Invalid config value: enable_slide must be true or false in $config_file" >&2
  exit 1
fi

mkdir -p "$(dirname "$hypr_snippet")"
{
  echo "# hyprvolume managed snippet"
  echo "exec-once = $exec_bin_escaped --watch --config $config_file_escaped"
  if [[ "$enable_slide_value" == "false" ]]; then
    cat <<'SNIPPET'
layerrule {
  name = hyprvolume-no-slide
  match:namespace = ^hyprvolume$
  no_anim = on
}
SNIPPET
  fi
} > "$hypr_snippet"

mkdir -p "$(dirname "$hypr_conf")"
if [[ ! -f "$hypr_conf" ]]; then
  touch "$hypr_conf"
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
' "$hypr_conf" > "$tmp_file"
mv "$tmp_file" "$hypr_conf"

{
  echo
  echo "$start_marker"
  echo "source = $hypr_snippet_escaped"
  echo "$end_marker"
} >> "$hypr_conf"

rm -f "$legacy_snippet"

if [[ "$start_now" -eq 1 && -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" && -n "${WAYLAND_DISPLAY:-}" ]]; then
  pkill -x hyprvolume >/dev/null 2>&1 || true
  pkill -x hypr-volume-osd >/dev/null 2>&1 || true
  nohup "$bin_dir/hyprvolume" --watch --config "$config_file" >/dev/null 2>&1 &
  sleep 0.2
  if pgrep -x hyprvolume >/dev/null 2>&1; then
    echo "Started watcher process in current session."
  else
    echo "Watcher failed to stay running; start manually to inspect errors." >&2
  fi
else
  echo "Watcher was not started immediately (missing Hyprland session or --no-start used)."
fi

echo "Installed hyprvolume to $bin_dir/hyprvolume"
echo "Config file: $config_file"
echo "Hyprland snippet: $hypr_snippet"
echo "Slide animation: $enable_slide_value (applies only to namespace ^hyprvolume$)"
echo "Restart Hyprland or run hyprctl reload to apply startup hook."
