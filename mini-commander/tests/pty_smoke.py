#!/usr/bin/env python3
"""pty_smoke.py - mini-commander in a pseudo-terminal: it starts, draws its panels and
exits with F10 (exit status 0). Only the Python standard library is used.

    python3 mini-commander/tests/pty_smoke.py ./mc-cmini.bin
"""
import fcntl
import os
import pty
import select
import signal
import struct
import sys
import termios
import time

binary = sys.argv[1]
pid, fd = pty.fork()
if pid == 0:
    os.environ["TERM"] = "xterm"
    os.environ["LANG"] = "C"
    os.execv(binary, [binary])
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
out = b""


def drain(seconds):
    global out
    end = time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            try:
                out += os.read(fd, 65536)
            except OSError:
                return


drain(2.0)
os.write(fd, b"\x1b[21~")          # F10 = exit
drain(2.0)
done, status = os.waitpid(pid, os.WNOHANG)
if done == 0:
    os.kill(pid, signal.SIGKILL)
    os.waitpid(pid, 0)
    print("FAIL: mc still running after F10")
    sys.exit(1)
rc = os.waitstatus_to_exitcode(status)
print("mc: %d bytes of screen output, exit %d" % (len(out), rc))
sys.exit(0 if rc == 0 and len(out) > 0 else 1)
