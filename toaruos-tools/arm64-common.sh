# arm64-common.sh - ToaruOS aarch64 in QEMU (sourced by tests/06-toaru-arm64.sh and tests/run-arm64.sh).
#
#   a64_prepare          release artifacts (_import/download/aarch64-virt.zip), cross TinyCC
#                        (mktcc-aarch64.sh), payload and ramdisk (mkarm64.sh)
#   a64_start <0|1> <log> QEMU: 1 = window (gtk, stays running), 0 = headless, serial pipe
#   a64_send <text>      one command line to the guest (through the serial pipe)
#   a64_log [n]          last n lines of the guest serial output (CR and ANSI removed)
#   a64_wait <line> [s]  wait until a serial output line is exactly <line>
#   a64_stop             stop QEMU and the serial reader
#
# Needs: qemu-system-aarch64, unzip, fakeroot, gzip, python3 (scripts/need.sh gives the hints).

A64_TOP=${A64_TOP:?set A64_TOP (the project root) before sourcing this file}
A64_B=$A64_TOP/_build
A64_WORK=$A64_B/arm64/work
A64_REL=$A64_TOP/_import/download/aarch64-virt
A64_ZIP=$A64_TOP/_import/download/aarch64-virt.zip
A64_URL="https://github.com/klange/toaruos/releases/download/v2.3.2/aarch64-virt.zip"
A64_QEMU_PID=0
A64_CAT_PID=0

a64_prepare ()
{
   if [ ! -f "$A64_REL/misaka-kernel" ] || [ ! -f "$A64_REL/ramdisk.igz" ]
   then
      if [ ! -f "$A64_ZIP" ]
      then
         mkdir -p "$(dirname "$A64_ZIP")"
         echo "== stahuji aarch64-virt.zip (ToaruOS 2.3.2) z $A64_URL" >&2
         curl -fL --retry 2 -o "$A64_ZIP.part" "$A64_URL" && mv "$A64_ZIP.part" "$A64_ZIP" || { rm -f "$A64_ZIP.part"; return 1; }
      fi
      mkdir -p "$A64_REL" && unzip -o "$A64_ZIP" -d "$A64_REL" > /dev/null || return 1
   fi
   [ -x "$A64_B/tcc-aarch64/bin/aarch64-toaru-tcc" ] || "$A64_TOP/toaruos-tools/mktcc-aarch64.sh" > /dev/null || return 1
   "$A64_TOP/toaruos-tools/mkarm64.sh" > /dev/null || return 1
}

a64_start ()   # a64_start <visible 0|1> <logfile>
{
   local visible=$1 log=$2
   mkdir -p "$A64_WORK"
   rm -f "$A64_WORK/serial.in" "$A64_WORK/serial.out" "$A64_WORK/serial.log"
   mkfifo "$A64_WORK/serial.in" "$A64_WORK/serial.out"
   # the options go into the positional parameters: the getty string has spaces
   if [ "$visible" = 1 ]
   then
      set -- -display gtk -device ramfb -device virtio-tablet-pci -device virtio-keyboard-pci \
             -append "root=/dev/ram0 migrate start=live-session ramfb vid=preset"
   else
      set -- -display none -fw_cfg "name=opt/org.toaruos.gettyargs,string=-a local /dev/ttyS0 115200 xterm" \
             -append "root=/dev/ram0 migrate start=--headless"
   fi
   qemu-system-aarch64 -M virt -cpu cortex-a72 -m 4G -smp 4 -no-reboot "$@" \
        -kernel "$A64_B/arm64/bootstub-a64" \
        -fw_cfg name=opt/org.toaruos.initrd,file="$A64_B/arm64/ramdisk.tgz" \
        -fw_cfg name=opt/org.toaruos.kernel,file="$A64_B/arm64/kernel-a64" \
        -serial pipe:"$A64_WORK/serial" > "$log" 2>&1 &
   A64_QEMU_PID=$!
   # the FIFO needs a permanent reader: the guest output goes to a plain file
   cat "$A64_WORK/serial.out" > "$A64_WORK/serial.log" 2> /dev/null &
   A64_CAT_PID=$!
}

a64_stop ()
{
   [ "$A64_QEMU_PID" != 0 ] && kill "$A64_QEMU_PID" 2> /dev/null
   [ "$A64_CAT_PID" != 0 ] && kill "$A64_CAT_PID" 2> /dev/null
   A64_QEMU_PID=0; A64_CAT_PID=0
}

a64_send ()
{
   printf '%s\n' "$1" > "$A64_WORK/serial.in"
}

a64_log ()
{
   tail -n "${1:-40}" "$A64_WORK/serial.log" 2> /dev/null | tr -d '\r' |
      sed 's/\x1b\[[0-9;?]*[a-zA-Z]//g; s/\x1b\][^\a]*\a//g'
}

a64_wait ()   # a64_wait <line> [timeout s] - the line must BE the text (the getty echoes commands)
{
   local i
   for i in $(seq 1 "${2:-120}")
   do
      a64_log 4000 | grep -qx "$1" && return 0
      kill -0 "$A64_QEMU_PID" 2> /dev/null || return 1
      sleep 1
   done
   return 1
}

a64_wait_for ()   # a64_wait_for <regex> [timeout s] - a regex in the output (the prompt), not the whole line
{
   local i
   for i in $(seq 1 "${2:-120}")
   do
      a64_log 4000 | grep -q "$1" && return 0
      kill -0 "$A64_QEMU_PID" 2> /dev/null || return 1
      sleep 1
   done
   return 1
}

