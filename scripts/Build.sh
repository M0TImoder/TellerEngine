#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/Common.sh"

declare -A MSG_ja=(
  [usage]="使い方: Build.sh [構成] [オプション]

構成
  -D, --debug          デバッグビルド (既定)
  -R, --release        リリースビルド
  -W, --wasm           WASMビルド (-R がなければデバッグ)

オプション
  -C, --clean          ビルドディレクトリを消してからやり直す
  -T, --target <名前>  ビルドするターゲットを絞る
  -j, --jobs <数>      並列度を指定する
  -v, --verbose        実行されるコンパイルコマンドを表示する
  -h, --help           この説明を表示する"
  [unsupported_os]="このスクリプトはLinux用です"
  [conflict]="-D と -R は同時に指定できません"
  [need_value]="値が必要です:"
  [unknown_option]="不明なオプションです:"
  [target_preset]="構成"
  [not_configured]="configureが済んでいません:"
  [ask_configure]="Setup.shを実行してconfigureしますか? [y/N]: "
  [aborted]="中止しました"
  [still_not_configured]="configureされていないためビルドできません"
  [cleaning]="ビルドディレクトリを削除します"
  [configuring]="configureします"
  [building]="ビルドします"
  [done]="完了しました"
  [failed]="失敗しました"
  [press_key]="何かキーを押すと終了します"
)

WANT_DEBUG=0
WANT_RELEASE=0
WANT_WASM=0
CLEAN=0
TARGET=""
JOBS=""
VERBOSE=0
PRESET=""

die() {
  printf '%s%s%s\n' "$C_NG" "$*" "$C_RESET" >&2
  exit 1
}

need_value() {
  [[ $# -ge 2 && -n "$2" ]] || die "$(msg need_value) $1"
}

parse_args() {
  while (( $# > 0 )); do
    case "$1" in
      -D|--debug)   WANT_DEBUG=1 ;;
      -R|--release) WANT_RELEASE=1 ;;
      -W|--wasm)    WANT_WASM=1 ;;
      -C|--clean)   CLEAN=1 ;;
      -v|--verbose) VERBOSE=1 ;;
      -T|--target)  need_value "$1" "${2:-}"; TARGET="$2"; shift ;;
      -T*)          TARGET="${1#-T}" ;;
      -j|--jobs)    need_value "$1" "${2:-}"; JOBS="$2"; shift ;;
      -j*)          JOBS="${1#-j}" ;;
      -h|--help)    msg usage; printf '\n'; exit 0 ;;
      *)            printf '%s %s\n\n' "$(msg unknown_option)" "$1" >&2; msg usage; printf '\n'; exit 1 ;;
    esac
    shift
  done
}

resolve_preset() {
  local prefix
  (( WANT_DEBUG == 1 && WANT_RELEASE == 1 )) && die "$(msg conflict)"

  if (( WANT_WASM == 1 )); then
    prefix="Wasm"
  else
    prefix="$(host_prefix)"
    [[ -n "$prefix" ]] || die "$(msg unsupported_os)"
  fi

  if (( WANT_RELEASE == 1 )); then
    PRESET="$prefix-release"
  else
    PRESET="$prefix-debug"
  fi
}

build_dir() { printf '%s' "$TELLER_ROOT/build/$PRESET"; }

is_configured() { [[ -f "$(build_dir)/CMakeCache.txt" ]]; }

# configureが無いときはSetup.shに任せる
ensure_configured() {
  local answer
  is_configured && return 0

  printf '%s%s %s%s\n' "$C_NG" "$(msg not_configured)" "$(build_dir)" "$C_RESET" >&2
  [[ -t 0 && -t 1 ]] || exit 1

  printf '%s' "$(msg ask_configure)"
  IFS= read -r answer || answer=""
  case "$answer" in
    y|Y|yes|YES) ;;
    *) printf '%s\n' "$(msg aborted)"; exit 1 ;;
  esac

  TELLER_PRESET="$PRESET" "$TELLER_ROOT/scripts/Setup.sh"
  is_configured || die "$(msg still_not_configured)"
}

build_command() {
  local cmd="cmake --build --preset $PRESET"
  [[ -n "$TARGET" ]] && cmd+=" --target $TARGET"
  [[ -n "$JOBS" ]] && cmd+=" -j $JOBS"
  (( VERBOSE == 1 )) && cmd+=" --verbose"
  printf '%s' "$cmd"
}

main() {
  parse_args "$@"
  resolve_preset

  printf '%s: %s\n' "$(msg target_preset)" "$PRESET"

  if (( CLEAN == 1 )) && [[ -d "$(build_dir)" ]]; then
    printf '\n%s\n' "$(msg cleaning)"
    run_step "rm -rf $(build_dir)"
  fi

  if ! is_configured && (( CLEAN == 1 )); then
    printf '\n%s\n' "$(msg configuring)"
    run_step "cmake --preset $PRESET"
  else
    ensure_configured
  fi

  printf '\n%s\n' "$(msg building)"
  if ! ( cd "$TELLER_ROOT" && run_step "$(build_command)" ); then
    printf '%s%s%s\n' "$C_NG" "$(msg failed)" "$C_RESET" >&2
    wait_key "$(msg press_key)"
    exit 1
  fi

  printf '\n%s%s%s\n' "$C_OK" "$(msg done)" "$C_RESET"
}

main "$@"
