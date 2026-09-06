# 各スクリプトからsourceして使う共通部分

TELLER_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TELLER_MSG_LANG="${TELLER_MSG_LANG:-ja}"

msg() {
  local -n table="MSG_${TELLER_MSG_LANG}"
  printf '%s' "${table[$1]}"
}

# 端末装飾
if [[ -t 1 ]]; then
  C_RESET=$'\033[0m'; C_DIM=$'\033[2m'; C_OK=$'\033[32m'; C_NG=$'\033[31m'; C_WARN=$'\033[33m'; C_SEL=$'\033[36m'
else
  C_RESET=""; C_DIM=""; C_OK=""; C_NG=""; C_WARN=""; C_SEL=""
fi

have() { command -v "$1" >/dev/null 2>&1; }

# 全角を2桁として数えた表示幅
disp_width() {
  local s="$1" chars bytes
  chars=${#s}
  local LC_ALL=C
  bytes=${#s}
  printf '%s' $(( chars + (bytes - chars) / 2 ))
}

# 表示幅を揃えて右側を空白で埋める
pad() {
  local rest
  printf '%s' "$1"
  rest=$(( $2 - $(disp_width "$1") ))
  (( rest > 0 )) && printf '%*s' "$rest" ""
  return 0
}

# 表示幅で切り詰める
clip() {
  local s="$1" limit="$2" lo=0 hi=${#1} mid
  (( limit <= 0 )) && return 0
  (( $(disp_width "$s") <= limit )) && { printf '%s' "$s"; return 0; }
  while (( lo < hi )); do
    mid=$(( (lo + hi + 1) / 2 ))
    if (( $(disp_width "${s:0:mid}") <= limit )); then lo=$mid; else hi=$(( mid - 1 )); fi
  done
  printf '%s' "${s:0:lo}"
}

# 引数のうち最も広い表示幅
widest() {
  local w max=0
  for w in "$@"; do
    w=$(disp_width "$w")
    (( w > max )) && max=$w
  done
  printf '%s' "$max"
}

# 指定した長さの区切り線
make_sep() {
  local s=""
  printf -v s '%*s' "$1" ''
  printf '%s' "${s// /=}"
}

term_cols() {
  local c
  c="$(tput cols 2>/dev/null || echo 80)"
  (( c < 24 )) && c=24
  printf '%s' "$c"
}

# コマンドを表示してから実行する
run_step() {
  printf '%s$ %s%s\n' "$C_DIM" "$1" "$C_RESET"
  bash -c "$1"
}

# 出力を読む時間を与えてから終わる
wait_key() {
  [[ -t 0 && -t 1 ]] || return 0
  printf '\n%s%s%s' "$C_DIM" "$1" "$C_RESET"
  IFS= read -rsn1 _ || true
  printf '\n'
}

# 1つだけ選ばせる
choose() {
  local -n picked_out=$1
  local title="$2"
  shift 2
  local options=("$@")
  local cursor=0
  local last=$(( ${#options[@]} - 1 ))

  if [[ ! -t 0 || ! -t 1 ]]; then
    picked_out=0
    return 0
  fi

  trap 'tput cnorm 2>/dev/null || true; printf "\033[?1049l"' EXIT INT TERM
  printf '\033[?1049h'
  tput civis 2>/dev/null || true

  local key rest index pointer cols width sep
  while true; do
    cols="$(term_cols)"
    width=$(widest "$title" "${options[@]}")
    width=$(( width + 4 ))
    (( width > cols )) && width=$cols
    sep="$(make_sep "$width")"

    printf '\033[H'
    printf '%s\033[K\n' "$(clip "$title" "$width")"
    printf '%s\033[K\n' "$sep"
    for index in "${!options[@]}"; do
      if (( cursor == index )); then pointer="${C_SEL}>${C_RESET}"; else pointer=" "; fi
      printf ' %s %s\033[K\n' "$pointer" "$(clip "${options[index]}" $(( width - 3 )))"
    done
    printf '%s\033[K\n' "$sep"
    printf '%s%s%s\033[K\n' "$C_DIM" "↑↓ 移動   Enter 決定   q 中止" "$C_RESET"
    printf '\033[J'

    IFS= read -rsn1 key || key=""
    if [[ "$key" == $'\033' ]]; then
      read -rsn2 -t 0.05 rest || rest=""
      key="$key$rest"
    fi
    case "$key" in
      $'\033[A') (( cursor > 0 )) && cursor=$(( cursor - 1 )) ;;
      $'\033[B') (( cursor < last )) && cursor=$(( cursor + 1 )) ;;
      '') break ;;
      q|Q|$'\033') cursor=-1; break ;;
    esac
  done

  tput cnorm 2>/dev/null || true
  printf '\033[?1049l'
  trap - EXIT INT TERM
  picked_out=$cursor
}

# 何秒か待ってから自動で閉じる
# キーを押せば即座に閉じる
close_soon() {
  local left="${1:-5}"
  [[ -t 0 && -t 1 ]] || return 0

  while (( left > 0 )); do
    printf '\r%s%d秒後に自動で閉じます... (何かキーを押すと即座に閉じる)%s\033[K' \
      "$C_DIM" "$left" "$C_RESET"
    if IFS= read -rsn1 -t 1 _; then
      break
    fi
    left=$(( left - 1 ))
  done
  printf '\r\033[K'
}

# ホストのプリセット接頭辞
host_prefix() {
  case "$(uname -s)" in
    Linux) printf 'Linux' ;;
    *)     printf '' ;;
  esac
}
