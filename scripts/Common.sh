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

# ホストのプリセット接頭辞
host_prefix() {
  case "$(uname -s)" in
    Linux) printf 'Linux' ;;
    *)     printf '' ;;
  esac
}
