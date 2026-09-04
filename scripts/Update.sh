#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/Common.sh"

declare -A MSG_ja=(
  [usage]="使い方: Update.sh [-h|--help]

git pull で最新にしてから、configure済みの構成を全て再ビルドする"
  [not_repo]="gitリポジトリではありません:"
  [dirty]="未コミットの変更があります"
  [dirty_hint]="pushするかstashしてから実行してください"
  [pulling]="最新を取得します"
  [no_preset]="configure済みの構成がありません"
  [found_preset]="再ビルドする構成:"
  [building]="ビルドします"
  [done]="完了しました"
  [failed]="失敗しました"
  [press_key]="何かキーを押すと終了します"
)

die() {
  printf '%s%s%s\n' "$C_NG" "$*" "$C_RESET" >&2
  exit 1
}

parse_args() {
  while (( $# > 0 )); do
    case "$1" in
      -h|--help) msg usage; printf '\n'; exit 0 ;;
      *)         msg usage; printf '\n'; exit 1 ;;
    esac
    shift
  done
}

# 追跡済みファイルの変更だけを見る
# build/ のような未追跡物で止めないため
check_clean() {
  local dirty
  dirty="$(git -C "$TELLER_ROOT" status --porcelain --untracked-files=no)"
  [[ -z "$dirty" ]] && return 0

  printf '%s%s%s\n' "$C_NG" "$(msg dirty)" "$C_RESET" >&2
  printf '%s\n' "$dirty" >&2
  printf '%s%s%s\n' "$C_WARN" "$(msg dirty_hint)" "$C_RESET" >&2
  exit 1
}

# build/ の中でCMakeCache.txtを持つものがconfigure済み
configured_presets() {
  local dir
  for dir in "$TELLER_ROOT"/build/*/; do
    [[ -f "$dir/CMakeCache.txt" ]] || continue
    basename "$dir"
  done
}

main() {
  parse_args "$@"

  git -C "$TELLER_ROOT" rev-parse --git-dir >/dev/null 2>&1 \
    || die "$(msg not_repo) $TELLER_ROOT"

  check_clean

  printf '%s\n' "$(msg pulling)"
  if ! ( cd "$TELLER_ROOT" && run_step "git pull --ff-only" ); then
    printf '%s%s%s\n' "$C_NG" "$(msg failed)" "$C_RESET" >&2
    wait_key "$(msg press_key)"
    exit 1
  fi

  local presets=()
  mapfile -t presets < <(configured_presets)
  if (( ${#presets[@]} == 0 )); then
    printf '\n%s\n' "$(msg no_preset)"
    return 0
  fi

  printf '\n%s %s\n' "$(msg found_preset)" "${presets[*]}"

  local preset
  for preset in "${presets[@]}"; do
    printf '\n%s: %s\n' "$(msg building)" "$preset"
    if ! ( cd "$TELLER_ROOT" && run_step "cmake --build --preset $preset" ); then
      printf '%s%s: %s%s\n' "$C_NG" "$(msg failed)" "$preset" "$C_RESET" >&2
      wait_key "$(msg press_key)"
      exit 1
    fi
  done

  printf '\n%s%s%s\n' "$C_OK" "$(msg done)" "$C_RESET"
}

main "$@"
