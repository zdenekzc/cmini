#!/bin/bash
# toaru-vm.sh — ToaruOS in QEMU without podman/docker, controlled over the serial line.
#
#   ./toaru-vm.sh start            # starts the VM in the background, the log is _build/work/out.log
#   ./toaru-vm.sh send 'ls /'      # sends a command to the host (assumes a logged-in getty)
#   ./toaru-vm.sh log [N]          # prints N lines of the log without control characters
#   ./toaru-vm.sh status           # running? + log size
#   ./toaru-vm.sh stop             # kills QEMU and the FIFO holder
#
# The only way out of the machine: the serial line (log) or the network. The shared
# disk is one-way — ata.ko does not flush its write-back cache (reason in README.md).
#
# Optional: ISO=/path SHARE=/path ./toaru-vm.sh start
set -u

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ISO=${ISO:-${DIR}/../_import/toaruos-2.3.2.iso}
# Boot bez ISO: INITRD (ramdisk.tgz) + KERNEL (kernel.mb), viz mkramdisk.sh
INITRD=${INITRD:-}
KERNEL=${KERNEL:-${DIR}/_build/kernel.mb}
SHARE=${SHARE:-${DIR}/_build/share.img}
WORK=${WORK:-${DIR}/_build/work}
RAM=${RAM:-1024}
SMP=${SMP:-2}
FIFO=${WORK}/in
OUT=${WORK}/out.log
PIDF=${WORK}/qemu.pid
HOLDERF=${WORK}/holder.pid
# Okno QEMU: jen pri grafickem displeji (DISPLAY / WAYLAND_DISPLAY) a kdyz TOARU_GUI neni 0
# (tests/output.sh nastavi TOARU_GUI=0). Serial line (mon:stdio) zustava pro ovladani skripty.
if [[ "${TOARU_GUI:-1}" != 0 && ( -n "${DISPLAY:-}" || -n "${WAYLAND_DISPLAY:-}" ) ]]; then
    DISPLAY_OPT=(-display "${TOARU_DISPLAY:-gtk}")
    WINDOW=1
else
    DISPLAY_OPT=(-nographic)
    WINDOW=0
fi
# SOUND=none (default): AC97 with QEMU's "none" audio backend. Plain "-device AC97"
# opens the host's ALSA; without a sound server the guest can stall at startup
# The screen then shows "Launching startup application..." as its last line (the
# splash). With start=--headless the shell is the serial getty (ttyS1), not the
# window, so nothing more appears on the screen; that is expected. A desktop needs
# start=live-session, which has no serial getty and so the tests cannot log in.
SOUND=${SOUND:-none}

die() { echo "error: $*" >&2; exit 1; }

# A PID can be REUSED and assigned to another process — a transferred *.pid file
# is exactly such a case (PID from another boot/machine). So for every PID we
# also remember the cmdline and kill only the process we started ourselves.
snapshot() { tr '\0' ' ' < "/proc/$1/cmdline" > "$2" 2>/dev/null || : > "$2"; }

mine() {
    local f=$1 want
    [[ -f $f ]] || return 1
    local p; p=$(cat "$f")
    [[ -n "$p" ]] || return 1
    [[ -r /proc/$p/cmdline ]] || return 1
    want=$(cat "${f%.pid}.cmd" 2>/dev/null)
    if [[ -z $want ]]; then
        # old state without *.cmd — at least check it is our QEMU
        grep -qa qemu "/proc/$p/cmdline" 2>/dev/null && return 0 || {
            echo "warning: pid $p from $f has no saved cmdline and is not qemu — killing nothing" >&2; return 1; }
    fi
    [[ "$(tr '\0' ' ' < "/proc/$p/cmdline")" == "$want"* ]]
}

running() { mine "${PIDF}"; }

do_start() {
    if [[ -n $INITRD ]]; then
        [[ -f "$INITRD" ]] || die "missing INITRD: $INITRD (run ./mkramdisk.sh)"
        [[ -f "$KERNEL" ]] || die "missing KERNEL: $KERNEL (run ./mkramdisk.sh)"
    else
        [[ -f "$ISO" ]] || die "missing ISO: $ISO (rename it or use ISO=/path)"
    fi
    [[ -f "$SHARE" ]] || die "missing disk image: $SHARE"
    command -v qemu-system-x86_64 >/dev/null || die "missing qemu-system-x86_64"
    running && die "QEMU is already running (pid $(cat "${PIDF}")) — ./toaru-vm.sh stop"

    mkdir -p "$WORK"
    rm -f "$FIFO"; : > "$OUT"; mkfifo "$FIFO"

    # Keep the FIFO open for writing, so the host does not get EOF on every command
    sleep 7200 > "$FIFO" &
    echo $! > "${HOLDERF}"

    local -a accel=(-accel kvm)
    [[ -r /dev/kvm && -w /dev/kvm ]] || { echo "warning: no /dev/kvm — TCG, slow"; accel=(); }

    # -M pc is deliberate: ata.ko binds to the PIIX IDE 8086:7010/8086:7111 and on q35
    # ToaruOS cannot even read its own CD. Network: the only NIC supported by ToaruOS is e1000.
    # fw_cfg sets up boot without graphics and the getty on the second serial line.
    # Boot: INITRD => -kernel/-initrd (direct kernel boot, the kernel command line as
    # boot.sys would assemble it: root on the ramdisk, writable root, headless start
    # (serial getty on ttyS1, where the tests log in as root). Otherwise the ISO as CD-ROM.
    # The window (WINDOW=1) only shows the screen; the start mode stays headless, since
    # live-session would take the serial getty away and the tests would never see the prompt.
    local -a bootsrc bootmode=() sound=()
    # START=headless (default): serial getty, the tests log in there.
    # START=live: desktop in the window (needs a window); no serial login.
    case ${START:-headless} in
        headless) START_ARG="start=--headless" ;;
        live)
            [[ $WINDOW == 1 ]] || die "START=live needs a window (DISPLAY, TOARU_GUI not 0)"
            START_ARG="start=live-session vid=auto" ;;
        *) die "START should be headless|live" ;;
    esac
    if [[ -n $INITRD ]]; then
        bootsrc=(-kernel "$KERNEL" -initrd "$INITRD" -append "root=/dev/ram0 migrate $START_ARG")
    else
        bootsrc=(-cdrom "$ISO")
    fi
    [[ ${START:-headless} == headless ]] && bootmode=(-fw_cfg name=opt/org.toaruos.bootmode,string=headless)
    case $SOUND in
        none) sound=(-audiodev none,id=snd0 -device AC97,audiodev=snd0) ;;
        alsa) sound=(-device AC97) ;;
        off)  ;;
        *)    die "SOUND should be none|alsa|off" ;;
    esac

    qemu-system-x86_64 "${accel[@]}" \
        -M pc -m "$RAM" -smp "$SMP" \
        "${bootsrc[@]}" -vga std "${sound[@]}" \
        -drive format=raw,if=none,id=share,file="$SHARE" \
        -device ide-hd,drive=share,bus=ide.0,unit=0 \
        -netdev user,id=n0 -device e1000,netdev=n0 \
        "${bootmode[@]}" \
        -fw_cfg name=opt/org.toaruos.gettyargs,string='-a root /dev/ttyS1 115200 linux' \
        -no-reboot "${DISPLAY_OPT[@]}" -serial file:"${WORK}/ttyS0.log" -serial mon:stdio \
        < "$FIFO" > "$OUT" 2>&1 &
    echo $! > "${PIDF}"
    # take cmdline only after qemu has exec'd (right after & it may still be a bash subprocess; stop would
    # then not recognize QEMU as its own and leave it running)
    local q k
    q=$(cat "${PIDF}")
    for k in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        grep -qa '^qemu-system' "/proc/$q/cmdline" 2>/dev/null && break
        sleep 0.1
    done
    snapshot "$q" "${PIDF%.pid}.cmd"
    # cmdline of the FIFO holder only now: opening the FIFO for writing waits for QEMU (the reader),
    # until then it is a bash subprocess with a foreign cmdline and stop would refuse to kill it
    local h
    h=$(cat "${HOLDERF}")
    for k in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        grep -qa '^sleep' "/proc/$h/cmdline" 2>/dev/null && break
        sleep 0.1
    done
    snapshot "$h" "${HOLDERF%.pid}.cmd"
    disown
    echo "QEMU pid=$(cat "${PIDF}") log=${OUT}"
    echo "boot takes ~15 s; then: ./toaru-vm.sh send 'ls /dev'"
}

do_send() {
    [[ $# -gt 0 ]] || die "send: command, e.g. ./toaru-vm.sh send 'ls /'"
    [[ -p "$FIFO" ]] || die "FIFO missing — ./toaru-vm.sh start"
    running || die "QEMU is not running — ./toaru-vm.sh start"
    printf '%s\n' "$*" > "$FIFO"
}

clean() {
    sed -e 's/\x1b\[[0-9;?]*[a-zA-Z]//g' -e 's/\x1b[()][A-Z0-9]//g' "$OUT" \
        | tr '\r' '\n' | grep -v '^ *$'
}

case "${1:-}" in
    start)  do_start ;;
    send)   shift; do_send "$*" ;;
    log)    clean | tail -n "${2:-40}" ;;
    status)
        if running; then echo "QEMU is running, pid=$(cat "${PIDF}")";
        else echo "QEMU is not running"; fi
        [[ -f "$OUT" ]] && echo "log: $(wc -c < "$OUT") B, $(wc -l < "$OUT") lines." ;;
    stop)
        for f in "${PIDF}" "${HOLDERF}"; do
            [[ -f "$f" ]] || continue
            p=$(cat "$f" 2>/dev/null)
            if mine "$f"; then
                kill "$p" 2>/dev/null && echo "killed $p ($(cat "${f%.pid}.cmd" 2>/dev/null))"
                # QEMU sometimes ignores SIGTERM (measured 2026-11-11) => SIGKILL after 3 s
                for k in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
                    [[ -d /proc/$p ]] || break
                    sleep 0.2
                done
                [[ -d /proc/$p ]] && kill -9 "$p" 2>/dev/null && echo "  (SIGKILL $p)"
            elif [[ -n "$p" && -r /proc/$p/cmdline ]]; then
                echo "pid $p is alive, but it is not ours — leaving it alone" >&2
            fi
            rm -f "$f" "${f%.pid}.cmd"
        done ;;
    *)      sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//' ;;
esac
