#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/Common.sh"

declare -A MSG_ja=(
  [usage]="使い方: Extract.sh [入力] [抽出先] [--data-only]

  入力        Undertaleフォルダ、または data.win そのもの
  抽出先      取り出したアセットを置く場所
  --data-only 隣の音声ファイルを取り込まない

引数を省略すると対話で尋ねる"
  [choose_mode]="何を取り出しますか"
  [mode_folder]="Undertaleフォルダごと取り出す"
  [mode_data]="data.win だけを取り出す(一部のアセットが使用できません)"
  [ask_folder]="Undertaleフォルダのパス: "
  [ask_data]="data.win のパス: "
  [ask_output]="抽出先のパス: "
  [not_found]="見つかりません:"
  [home_refused]="抽出先にホームディレクトリそのものは指定できません"
  [not_built]="抽出ツールがビルドされていません"
  [hint_build]="scripts/Build.sh を実行してください"
  [data_only]="data.win だけを対象にします。外部の音声は取り込みません"
  [running]="抽出します"
  [press_key]="何かキーを押すと終了します"
  [failed]="失敗しました"
  [cancelled]="中止しました"
  [empty_output]="抽出先が空です"
)

INPUT=""
OUTPUT=""
DATA_ONLY=0

parse_args() {
  while (( $# > 0 )); do
    case "$1" in
      --data-only) DATA_ONLY=1 ;;
      -h|--help)   msg usage; printf '\n'; exit 0 ;;
      *)
        if [[ -z "$INPUT" ]]; then INPUT="$1"
        elif [[ -z "$OUTPUT" ]]; then OUTPUT="$1"
        else msg usage; printf '\n'; exit 1
        fi
        ;;
    esac
    shift
  done
}

# 入力だけ標準出力へ
ask() {
  local prompt="$1" answer
  printf '%s' "$prompt" >&2
  IFS= read -r answer || answer=""
  printf '%s' "$answer"
}

main() {
  parse_args "$@"

  local tool="$TELLER_ROOT/build/Linux-debug/TellerExtract"
  [[ -x "$tool" ]] || tool="$TELLER_ROOT/build/Linux-release/TellerExtract"
  if [[ ! -x "$tool" ]]; then
    printf '%s%s%s\n%s\n' "$C_NG" "$(msg not_built)" "$C_RESET" "$(msg hint_build)" >&2
    exit 1
  fi

  # 引数で指定されていなければ選ばせる
  if [[ -z "$INPUT" ]]; then
    local picked=0
    choose picked "$(msg choose_mode)" "$(msg mode_folder)" "$(msg mode_data)"
    case "$picked" in
      0) DATA_ONLY=0; INPUT="$(ask "$(msg ask_folder)")" ;;
      1) DATA_ONLY=1; INPUT="$(ask "$(msg ask_data)")" ;;
      *) printf '%s\n' "$(msg cancelled)"; exit 0 ;;
    esac
  fi
  [[ -n "$OUTPUT" ]] || OUTPUT="$(ask "$(msg ask_output)")"

  if [[ -z "$INPUT" || -z "$OUTPUT" ]]; then
    printf '%s%s%s\n' "$C_NG" "$(msg empty_output)" "$C_RESET" >&2
    exit 1
  fi

  if [[ ! -e "$INPUT" ]]; then
    printf '%s%s %s%s\n' "$C_NG" "$(msg not_found)" "$INPUT" "$C_RESET" >&2
    exit 1
  fi

  local resolved
  resolved="$(cd "$(dirname "$OUTPUT")" 2>/dev/null && pwd)/$(basename "$OUTPUT")" || resolved="$OUTPUT"
  if [[ "$resolved" == "$HOME" || "$resolved" == "$HOME/" ]]; then
    printf '%s%s%s\n' "$C_NG" "$(msg home_refused)" "$C_RESET" >&2
    exit 1
  fi

  (( DATA_ONLY == 1 )) && printf '%s%s%s\n' "$C_WARN" "$(msg data_only)" "$C_RESET"

  printf '%s\n\n' "$(msg running)"
  local args=("$INPUT" "$OUTPUT")
  (( DATA_ONLY == 1 )) && args+=("--data-only")

  if ! "$tool" "${args[@]}"; then
    printf '%s%s%s\n' "$C_NG" "$(msg failed)" "$C_RESET" >&2
    close_soon 30
    exit 1
  fi

  close_soon 5
}

main "$@"