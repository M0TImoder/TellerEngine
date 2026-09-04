#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/Common.sh"

declare -A MSG_ja=(
  [usage]="使い方: Setup.sh [-Y|--yes] [-h|--help]

  -Y, --yes    確認を挟まず、全ての項目を実行する
  -h, --help   この説明を表示する

環境をスキャンし、不足しているものの導入、configure、ビルドまでを行う。
対話的な端末では、何を行うかをその場で選択できる。"
  [scanning]="環境をスキャンしています"
  [scan_result]="スキャン結果"
  [ok]="あり"
  [missing]="なし"
  [too_old]="バージョン不足"
  [inactive]="未有効化"
  [hint_yes]="導入するには -Y または --yes を付けて実行してください"
  [no_tty]="対話的な端末ではないため、スキャン結果のみを表示しました"
  [nothing_to_do]="実行する項目がありません"
  [keys]="↑↓ 移動   スペース 選択   Enter 実行   q 中止"
  [run_all]="全て行う"
  [cancelled]="中止しました"
  [press_key]="何かキーを押すと終了します"
  [running]="実行中"
  [done]="完了しました"
  [failed]="失敗しました"
  [no_pm]="対応するパッケージマネージャが見つかりません"
  [unsupported_os]="このスクリプトはLinux用です"
  [item_cmake]="CMake 3.28以上"
  [item_ninja]="Ninja"
  [item_cxx]="C++コンパイラ (C++20対応)"
  [item_git]="Git"
  [item_ccache]="ccache または sccache"
  [item_emsdk]="Emscripten SDK"
  [install_cmake]="CMakeを導入する"
  [update_cmake]="CMakeを更新する"
  [install_ninja]="Ninjaを導入する"
  [install_cxx]="C++コンパイラを導入する"
  [update_cxx]="C++コンパイラを更新する"
  [install_git]="Gitを導入する"
  [install_ccache]="ccacheを導入する"
  [install_emsdk]="EmscriptenSDKを導入する"
  [activate_emsdk]="EmscriptenSDKを有効化する"
  [do_configure]="configureを実行する"
  [do_build]="ビルドする"
)

# 行末の消去
# メニュー描画中だけ有効
LINE_CLEAR=""

ITEM_COL=0
STATE_COL=0
ACTION_COL=0
CONTENT_COL=0


# 表示幅は内容の最長行で固定
# 端末がそれより狭いときだけ端末に合わせる
view_cols() {
  local c
  c="$(term_cols)"
  (( CONTENT_COL > 0 && CONTENT_COL < c )) && c=$CONTENT_COL
  printf '%s' "$c"
}

# $1 >= $2 を判定する
version_ge() { printf '%s\n%s\n' "$2" "$1" | sort -V -C; }

declare -A STATE
declare -A FOUND

PM=""
SUDO=""
PRESET="${TELLER_PRESET:-Linux-debug}"

detect_package_manager() {
  local pm
  for pm in apt-get dnf pacman zypper; do
    if have "$pm"; then PM="$pm"; break; fi
  done
  if [[ $EUID -ne 0 ]] && have sudo; then SUDO="sudo"; fi
}

# パッケージマネージャごとのパッケージ名
package_name() {
  case "$1:$PM" in
    cmake:*)          echo "cmake" ;;
    ninja:apt-get)    echo "ninja-build" ;;
    ninja:dnf)        echo "ninja-build" ;;
    ninja:pacman)     echo "ninja" ;;
    ninja:zypper)     echo "ninja" ;;
    cxx:apt-get)      echo "g++" ;;
    cxx:dnf)          echo "gcc-c++" ;;
    cxx:pacman)       echo "gcc" ;;
    cxx:zypper)       echo "gcc-c++" ;;
    git:*)            echo "git" ;;
    ccache:*)         echo "ccache" ;;
  esac
}

install_command() {
  local pkg
  pkg="$(package_name "$1")"
  case "$PM" in
    apt-get) echo "$SUDO apt-get install -y $pkg" ;;
    dnf)     echo "$SUDO dnf install -y $pkg" ;;
    pacman)  echo "$SUDO pacman -S --noconfirm $pkg" ;;
    zypper)  echo "$SUDO zypper install -y $pkg" ;;
    *)       echo "" ;;
  esac
}

# aptが配れるCMakeが古い場合はKitwareのリポジトリを足す
apt_cmake_is_new_enough() {
  local cand
  cand="$(apt-cache policy cmake 2>/dev/null | awk '/Candidate:/{print $2}')"
  [[ -n "$cand" ]] && version_ge "${cand%%-*}" "3.28"
}

cmake_install_command() {
  if [[ "$PM" == "apt-get" ]] && ! apt_cmake_is_new_enough; then
    echo "Kitwareのaptリポジトリを追加してからCMakeを導入"
  else
    install_command cmake
  fi
}

run_install_cmake() {
  if [[ "$PM" == "apt-get" ]] && ! apt_cmake_is_new_enough; then
    run_step "$SUDO apt-get update"
    run_step "$SUDO apt-get install -y ca-certificates gpg wget lsb-release"
    run_step "wget -qO- https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor - | $SUDO tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null"
    run_step "echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ $(lsb_release -cs 2>/dev/null || echo jammy) main' | $SUDO tee /etc/apt/sources.list.d/kitware.list >/dev/null"
    run_step "$SUDO apt-get update"
  fi
  run_step "$(install_command cmake)"
}

emsdk_dir() { echo "${EMSDK:-$HOME/emsdk}"; }

run_install_emsdk() {
  local dir
  dir="$(emsdk_dir)"
  if [[ ! -d "$dir" ]]; then
    run_step "git clone https://github.com/emscripten-core/emsdk.git $dir"
  fi
  run_step "$dir/emsdk install latest"
  run_step "$dir/emsdk activate latest"
}

scan() {
  local v cxx

  if have cmake; then
    v="$(cmake --version | head -1 | awk '{print $3}')"
    FOUND[cmake]="$v"
    if version_ge "$v" "3.28"; then STATE[cmake]=ok; else STATE[cmake]=too_old; fi
  else
    STATE[cmake]=missing
  fi

  if have ninja; then
    STATE[ninja]=ok; FOUND[ninja]="$(ninja --version)"
  else
    STATE[ninja]=missing
  fi

  STATE[cxx]=missing
  for cxx in "${CXX:-}" g++ clang++; do
    [[ -n "$cxx" ]] && have "$cxx" || continue
    FOUND[cxx]="$("$cxx" --version | head -1)"
    if echo 'int main(){}' | "$cxx" -std=c++20 -x c++ - -o /dev/null 2>/dev/null; then
      STATE[cxx]=ok
      break
    fi
    STATE[cxx]=too_old
  done

  if have git; then
    STATE[git]=ok; FOUND[git]="$(git --version)"
  else
    STATE[git]=missing
  fi

  if have ccache; then
    STATE[ccache]=ok; FOUND[ccache]="$(ccache --version | head -1)"
  elif have sccache; then
    STATE[ccache]=ok; FOUND[ccache]="$(sccache --version | head -1)"
  else
    STATE[ccache]=missing
  fi

  if have emcc; then
    STATE[emsdk]=ok; FOUND[emsdk]="$(emcc --version | head -1)"
  elif [[ -x "$(emsdk_dir)/emsdk" ]]; then
    STATE[emsdk]=inactive; FOUND[emsdk]="$(emsdk_dir)"
  else
    STATE[emsdk]=missing
  fi
}

state_color() {
  case "$1" in
    ok)      printf '%s' "$C_OK" ;;
    missing) printf '%s' "$C_NG" ;;
    *)       printf '%s' "$C_WARN" ;;
  esac
}

print_scan_result() {
  local key cols detail room
  cols="$(view_cols)"
  room=$(( cols - 2 - ITEM_COL - 2 - STATE_COL - 2 ))
  printf '%s%s\n' "$(msg scan_result)" "$LINE_CLEAR"
  for key in cmake ninja cxx git ccache emsdk; do
    detail="$(clip "${FOUND[$key]:-}" "$room")"
    printf '  %s  %s%s%s  %s%s%s%s\n' \
      "$(pad "$(msg "item_$key")" "$ITEM_COL")" \
      "$(state_color "${STATE[$key]}")" "$(pad "$(msg "${STATE[$key]}")" "$STATE_COL")" "$C_RESET" \
      "$C_DIM" "$detail" "$C_RESET" "$LINE_CLEAR"
  done
}

# 実行候補
ACTION_KEY=()
ACTION_LABEL=()
ACTION_DETAIL=()
ACTION_CHECKED=()

add_action() {
  ACTION_KEY+=("$1"); ACTION_LABEL+=("$2"); ACTION_DETAIL+=("$3"); ACTION_CHECKED+=(1)
}

# 状態に応じて導入と更新を書き分ける
action_label() {
  local key="$1"
  case "${STATE[$key]}" in
    too_old)  msg "update_$key" ;;
    inactive) msg "activate_$key" ;;
    *)        msg "install_$key" ;;
  esac
}

compute_columns() {
  local key i w labels=()
  for key in cmake ninja cxx git ccache emsdk; do labels+=("$(msg "item_$key")"); done
  ITEM_COL=$(widest "${labels[@]}")
  STATE_COL=$(widest "$(msg ok)" "$(msg missing)" "$(msg too_old)" "$(msg inactive)")
  ACTION_COL=$(widest "${ACTION_LABEL[@]}" "$(msg run_all)")

  CONTENT_COL=$(widest "$(msg scan_result)" "$(msg keys)")
  for key in cmake ninja cxx git ccache emsdk; do
    w=$(( 2 + ITEM_COL + 2 + STATE_COL + 2 + $(disp_width "${FOUND[$key]:-}") ))
    (( w > CONTENT_COL )) && CONTENT_COL=$w
  done
  for i in "${!ACTION_KEY[@]}"; do
    w=$(( 7 + ACTION_COL + 2 + $(disp_width "${ACTION_DETAIL[i]}") ))
    (( w > CONTENT_COL )) && CONTENT_COL=$w
  done
  return 0
}

build_actions() {
  local key
  for key in cmake ninja cxx git ccache; do
    [[ "${STATE[$key]}" != "ok" ]] || continue
    if [[ "$key" == "cmake" ]]; then
      add_action "install_cmake" "$(action_label cmake)" "$(cmake_install_command)"
    else
      add_action "install_$key" "$(action_label "$key")" "$(install_command "$key")"
    fi
  done
  if [[ "${STATE[emsdk]}" != "ok" ]]; then
    add_action "install_emsdk" "$(action_label emsdk)" "$(emsdk_dir)"
  fi
  add_action "configure" "$(msg do_configure)" "cmake --preset $PRESET"
  add_action "build" "$(msg do_build)" "cmake --build --preset $PRESET"
}

# 選択UI
CURSOR=0

enter_screen() { printf '\033[?1049h'; tput civis 2>/dev/null || true; }
leave_screen() { tput cnorm 2>/dev/null || true; printf '\033[?1049l'; }

render_menu() {
  local cols sep i last mark pointer detail room
  cols="$(view_cols)"
  sep="$(make_sep "$cols")"
  last=${#ACTION_KEY[@]}
  room=$(( cols - 7 - ACTION_COL - 2 ))

  LINE_CLEAR=$'\033[K'
  printf '\033[H'

  print_scan_result
  printf '%s%s\n' "$sep" "$LINE_CLEAR"

  for i in "${!ACTION_KEY[@]}"; do
    if (( CURSOR == i )); then pointer="${C_SEL}>${C_RESET}"; else pointer=" "; fi
    if (( ACTION_CHECKED[i] == 1 )); then mark="x"; else mark=" "; fi
    detail="$(clip "${ACTION_DETAIL[i]}" "$room")"
    printf ' %s [%s] %s  %s%s%s%s\n' \
      "$pointer" "$mark" "$(pad "${ACTION_LABEL[i]}" "$ACTION_COL")" \
      "$C_DIM" "$detail" "$C_RESET" "$LINE_CLEAR"
  done

  if (( CURSOR == last )); then pointer="${C_SEL}>${C_RESET}"; else pointer=" "; fi
  printf ' %s     %s%s\n' "$pointer" "$(msg run_all)" "$LINE_CLEAR"

  printf '%s%s\n' "$sep" "$LINE_CLEAR"
  printf '%s%s%s%s\n' "$C_DIM" "$(clip "$(msg keys)" "$cols")" "$C_RESET" "$LINE_CLEAR"

  printf '\033[J'
  LINE_CLEAR=""
}

read_key() {
  local key rest
  IFS= read -rsn1 key || return 1
  if [[ "$key" == $'\033' ]]; then
    read -rsn2 -t 0.05 rest || rest=""
    key="$key$rest"
  fi
  printf '%s' "$key"
}

select_actions() {
  local last=${#ACTION_KEY[@]} key i
  trap 'leave_screen' EXIT INT TERM
  trap 'true' WINCH
  enter_screen
  render_menu
  while true; do
    if key="$(read_key)"; then
      case "$key" in
        $'\033[A') (( CURSOR > 0 )) && CURSOR=$(( CURSOR - 1 )) ;;
        $'\033[B') (( CURSOR < last )) && CURSOR=$(( CURSOR + 1 )) ;;
        ' ')
          if (( CURSOR < last )); then
            ACTION_CHECKED[CURSOR]=$(( 1 - ACTION_CHECKED[CURSOR] ))
          fi
          ;;
        '')
          if (( CURSOR == last )); then
            for i in "${!ACTION_KEY[@]}"; do ACTION_CHECKED[i]=1; done
          fi
          break
          ;;
        q|Q|$'\033')
          leave_screen
          trap - EXIT INT TERM WINCH
          printf '%s\n' "$(msg cancelled)"
          exit 0
          ;;
      esac
    fi
    render_menu
  done
  leave_screen
  trap - EXIT INT TERM WINCH
}

run_action() {
  case "$1" in
    install_cmake) run_install_cmake ;;
    install_emsdk) run_install_emsdk ;;
    install_*)     run_step "$(install_command "${1#install_}")" ;;
    configure)     run_step "cmake --preset $PRESET" ;;
    build)         run_step "cmake --build --preset $PRESET" ;;
  esac
}

execute_selected() {
  local i any=0
  for i in "${!ACTION_KEY[@]}"; do
    (( ACTION_CHECKED[i] == 1 )) || continue
    any=1
    printf '\n%s: %s\n' "$(msg running)" "${ACTION_LABEL[i]}"
    if ! ( cd "$TELLER_ROOT" && run_action "${ACTION_KEY[i]}" ); then
      printf '%s%s: %s%s\n' "$C_NG" "$(msg failed)" "${ACTION_LABEL[i]}" "$C_RESET"
      return 1
    fi
  done
  if (( any == 0 )); then
    printf '%s\n' "$(msg nothing_to_do)"
    return 0
  fi
  printf '\n%s%s%s\n' "$C_OK" "$(msg done)" "$C_RESET"
}

ASSUME_YES=0

parse_args() {
  while (( $# > 0 )); do
    case "$1" in
      -Y|--yes)  ASSUME_YES=1 ;;
      -h|--help) msg usage; printf '\n'; exit 0 ;;
      *)         msg usage; printf '\n'; exit 1 ;;
    esac
    shift
  done
}

main() {
  parse_args "$@"

  if [[ "$(uname -s)" != "Linux" ]]; then
    printf '%s\n' "$(msg unsupported_os)"
    exit 1
  fi

  detect_package_manager
  printf '%s\n\n' "$(msg scanning)"
  scan
  build_actions
  compute_columns

  if (( ASSUME_YES == 1 )); then
    print_scan_result
    printf '\n'
    execute_selected
    return
  fi

  if [[ ! -t 0 || ! -t 1 ]]; then
    print_scan_result
    printf '\n'
    [[ -z "$PM" ]] && printf '%s%s%s\n' "$C_WARN" "$(msg no_pm)" "$C_RESET"
    printf '%s\n' "$(msg no_tty)"
    printf '%s\n' "$(msg hint_yes)"
    return
  fi

  select_actions
  print_scan_result
  execute_selected || true
  wait_key "$(msg press_key)"
}

main "$@"
