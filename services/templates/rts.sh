#!/usr/bin/env sh
# Riviera Tech Solutions project runner. rts.env is data, never shell code.
set -eu
cd "$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)"
fail() { printf '%s\n' "ERROR: $1" >&2; exit "${2:-1}"; }
need() { command -v "$1" >/dev/null 2>&1 || fail "Install $1 and add it to PATH. See README.txt." 3; }
[ -f rts.env ] || fail 'Missing rts.env.' 2
while IFS= read -r line || [ -n "$line" ]; do
  line=$(printf '%s' "$line" | tr -d '\r')
  case "$line" in ''|'#'*) continue ;; esac
  key=${line%%=*}; value=${line#*=}
  case "$key" in
    BOARD_LABEL|BOARD_CONFIG_CONFIRMED|MEMORY_CONFIG_CONFIRMED|TOOL|INTERFACE|FLASH_ADDRESS|OPENOCD_CFG|PROBE_SERIAL) export "$key=$value" ;;
    *) fail "Unknown configuration key: $key" 2 ;;
  esac
done < rts.env
build() {
  need arm-none-eabi-gcc; need cmake; need ninja
  cmake -S . -B build -G Ninja || fail 'CMake configuration failed.' 5
  cmake --build build || fail 'Build failed.' 5
  [ -s build/firmware.bin ] || fail 'Build did not produce firmware.bin.' 6
}
check_board() {
  [ "${BOARD_CONFIG_CONFIRMED:-false}" = true ] || fail 'Confirm board/pins/clocks in rts.env before flashing.' 2
  [ "${MEMORY_CONFIG_CONFIRMED:-false}" = true ] || fail 'Verify core/linker.ld against the exact part and confirm memory in rts.env.' 2
  case "${BOARD_LABEL:-}" in ''|SET_THIS*) fail 'Set your exact BOARD_LABEL.' 2 ;; esac
  [ "${FLASH_ADDRESS:-}" = 0x08000000 ] || fail 'This cold-reset starter supports flash base 0x08000000 only.' 2
  [ "${INTERFACE:-}" = SWD ] || fail 'This runner supports SWD only.' 2
}
check_tool() {
  case "${TOOL:-}" in
    stm32_programmer_cli) need STM32_Programmer_CLI ;;
    openocd) need openocd; [ -n "${OPENOCD_CFG:-}" ] || fail 'Set OPENOCD_CFG to a board config containing both probe and target.' 2 ;;
    *) fail 'TOOL must be stm32_programmer_cli or openocd.' 2 ;;
  esac
}
case "${1:-doctor}" in
  doctor)
    need arm-none-eabi-gcc; need cmake; need ninja
    printf '%s\n' 'Build tools found. No hardware was contacted. Configure rts.env before flash.'
    ;;
  build) build ;;
  flash)
    check_board; check_tool; build
    case "$TOOL" in
      stm32_programmer_cli)
        if [ -n "${PROBE_SERIAL:-}" ]; then
          STM32_Programmer_CLI -c "port=$INTERFACE" "sn=$PROBE_SERIAL" -w build/firmware.bin "$FLASH_ADDRESS" -v -rst || fail 'Flash/verify failed.' 10
        else
          STM32_Programmer_CLI -c "port=$INTERFACE" -w build/firmware.bin "$FLASH_ADDRESS" -v -rst || fail 'Flash/verify failed.' 10
        fi ;;
      openocd)
        # Fixed artifact path; no config values are interpolated into Tcl.
        openocd -f "$OPENOCD_CFG" -c 'program build/firmware.bin 0x08000000 verify reset exit' || fail 'Flash/verify failed.' 10 ;;
    esac
    printf '%s\n' 'Firmware programmed, verified and reset. Verify application behaviour on the board.'
    ;;
  *) fail 'Usage: sh rts.sh <doctor|build|flash>' 2 ;;
esac
