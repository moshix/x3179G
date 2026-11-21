#!/usr/bin/env python3
"""
A tiny scripted TN3270 host for tests/host_tests.sh.

It listens on 127.0.0.1:PORT, does the telnet negotiation x3179 expects
(TERMINAL-TYPE, EOR, BINARY), then:

  1. sends a Read Partition Query, like go3270 (3270BBS) does, and writes
     the code page x3179 reports in its Character Sets reply to CPID;
  2. paints one screen (see screen()) and creates READY;
  3. runs commands the test script drops into WORKDIR as cmd.1, cmd.2,
     ... in order.  "rm" sends a host Read Modified - what the 3270BBS
     typing indicator does - and writes the reply to resp.N, one
     "ADDRESS:text" line per modified field (text decoded from CP037).
     "quit" ends the session.

Usage: fakehost.py PORT WORKDIR
"""
import os
import socket
import sys
import time

IAC, SB, SE, WILL, WONT, DO, DONT = 255, 250, 240, 251, 252, 253, 254
TTYPE, EOR_OPT, BINARY = 24, 25, 0
EOR = 239

# 3270 commands, orders and attributes
CMD_EW = 0xF5          # Erase/Write
CMD_RM = 0xF6          # Read Modified
CMD_WSF = 0xF3         # Write Structured Field
WCC_RESTORE = 0xC2     # reset + keyboard restore (as go3270 sends)
SBA, SF, SFE, SA, IC = 0x11, 0x1D, 0x29, 0x28, 0x13
PROT = 0x60            # protected field attribute (with the 0x40 bit)
UNPROT = 0x40          # unprotected field attribute
BLUE, PINK, TURQUOISE, YELLOW = 0xF1, 0xF3, 0xF5, 0xF6

# Characters that differ between EBCDIC code pages, plus Latin-1 ones.
# Shown on row 7 in yellow; the copy test selects exactly this text.
SPECIAL = "Copy![\\]^{}|~#@$ Aeéü"
FIELD1 = 5 * 80 + 1    # first input field, row 5 (cols 1-39)
FIELD2 = 9 * 80 + 1    # second input field, row 9 (cols 1-39)

# 6-bit 3270 buffer address code table
CODES = [0x40, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
         0xC8, 0xC9, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
         0x50, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
         0xD8, 0xD9, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
         0x60, 0x61, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
         0xE8, 0xE9, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
         0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
         0xF8, 0xF9, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F]


def sba(addr):
    return bytes([SBA, CODES[addr >> 6], CODES[addr & 0x3F]])


def rc(row, col):
    return row * 80 + col


def ebcdic(text):
    return text.encode("cp037")


def screen():
    """
    row 0: blue header, then the SA highlight/colour reset 3270BBS sends
    rows 2, 3: protected pink and turquoise fields (colour test)
    row 5: input field 1        row 7: yellow SPECIAL text
    row 9: input field 2        cursor: start of field 1
    """
    s = bytes([CMD_EW, WCC_RESTORE])
    s += sba(rc(0, 0)) + bytes([SFE, 2, 0xC0, PROT, 0x42, BLUE])
    s += ebcdic("HEADER")
    s += bytes([SA, 0x41, 0x00, SA, 0x42, 0x00])
    s += sba(rc(2, 0)) + bytes([SFE, 2, 0xC0, PROT, 0x42, PINK])
    s += ebcdic("P" * 40)
    s += sba(rc(3, 0)) + bytes([SFE, 2, 0xC0, PROT, 0x42, TURQUOISE])
    s += ebcdic("T" * 40)
    s += sba(FIELD1 - 1) + bytes([SF, UNPROT])
    s += sba(FIELD1 + 39) + bytes([SF, PROT])
    s += sba(rc(7, 0)) + bytes([SFE, 2, 0xC0, PROT, 0x42, YELLOW])
    s += ebcdic(SPECIAL)
    s += sba(rc(7, 40)) + bytes([SF, PROT])
    s += sba(FIELD2 - 1) + bytes([SF, UNPROT])
    s += sba(FIELD2 + 39) + bytes([SF, PROT])
    s += sba(FIELD1) + bytes([IC])
    return s


def send(conn, data):
    """Send a 3270 record: double IACs, terminate with IAC EOR."""
    conn.sendall(data.replace(b"\xff", b"\xff\xff") + bytes([IAC, EOR]))


def read_record(conn, timeout=10):
    """Read one inbound record up to IAC EOR; telnet commands skipped."""
    conn.settimeout(timeout)
    buf = bytearray()
    while True:
        b = conn.recv(1)
        if not b:
            raise EOFError("connection closed")
        if b[0] == IAC:
            c = conn.recv(1)[0]
            if c == EOR:
                return bytes(buf)
            if c == IAC:
                buf.append(IAC)
            elif c in (WILL, WONT, DO, DONT):
                conn.recv(1)          # option byte
            elif c == SB:             # skip to IAC SE
                while not (conn.recv(1)[0] == IAC and
                           conn.recv(1)[0] == SE):
                    pass
            continue
        buf += b


def query_cpid(reply):
    """
    Code page of the base character set in a query reply (AID 0x88 then
    structured fields: 2-byte length, 0x81, QCODE, data).  Same parse
    as go3270's getCodepageID: in the Character Sets reply (QCODE 0x85)
    byte 10 is the descriptor length, descriptors start at byte 11, the
    code page is the last two bytes of the one with local ID 0.
    """
    i = 1
    while i + 4 <= len(reply):
        n = (reply[i] << 8) | reply[i + 1]
        if n < 4:
            break
        sf = reply[i + 2:i + n]
        if sf[0] == 0x81 and sf[1] == 0x85 and len(sf) > 11:
            dl = sf[10]
            pos = 11
            while pos + dl <= len(sf):
                if sf[pos] == 0:
                    return (sf[pos + dl - 2] << 8) | sf[pos + dl - 1]
                pos += dl
        i += n
    return -1


def modified_fields(reply):
    """Read Modified reply -> [(address, text)]: AID, cursor address,
    then per modified field SBA + 2-byte address + data."""
    fields, i = [], 3
    while i < len(reply):
        if reply[i] == SBA and i + 2 < len(reply):
            addr = (CODES.index(reply[i + 1]) << 6) | \
                CODES.index(reply[i + 2])
            fields.append([addr, bytearray()])
            i += 3
            continue
        if fields:
            fields[-1][1].append(reply[i])
        i += 1
    return [(a, bytes(t).decode("cp037")) for a, t in fields]


def wait_for(path, limit=60):
    end = time.time() + limit
    while not os.path.exists(path):
        if time.time() > end:
            sys.exit("fakehost: timed out waiting for %s" % path)
        time.sleep(0.05)


def put(work, name, text=""):
    """Write a result file atomically (the test polls for it)."""
    tmp = os.path.join(work, name + ".tmp")
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(text)
    os.rename(tmp, os.path.join(work, name))


def main(port, work):
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", port))
    srv.listen(1)
    conn, _ = srv.accept()

    # telnet negotiation: terminal type first, then EOR and BINARY
    conn.sendall(bytes([IAC, DO, TTYPE]))
    time.sleep(0.3)
    conn.sendall(bytes([IAC, SB, TTYPE, 1, IAC, SE]))
    time.sleep(0.3)
    conn.sendall(bytes([IAC, DO, EOR_OPT, IAC, WILL, EOR_OPT,
                        IAC, DO, BINARY, IAC, WILL, BINARY]))
    time.sleep(0.5)
    conn.settimeout(0.5)
    try:                          # drain the negotiation replies
        while conn.recv(4096):
            pass
    except socket.timeout:
        pass

    # Read Partition Query, as go3270 sends it
    send(conn, bytes([CMD_WSF, 0x00, 0x05, 0x01, 0xFF, 0x02]))
    put(work, "CPID", str(query_cpid(read_record(conn))))

    send(conn, screen())
    put(work, "READY")

    n = 1
    while True:
        wait_for(os.path.join(work, "cmd.%d" % n))
        cmd = open(os.path.join(work, "cmd.%d" % n)).read().strip()
        if cmd == "quit":
            break
        if cmd == "rm":
            send(conn, bytes([CMD_RM]))
            lines = ["%d:%s" % f for f in
                     modified_fields(read_record(conn))]
            put(work, "resp.%d" % n, "\n".join(lines) + "\n")
        n += 1
    conn.close()


if __name__ == "__main__":
    main(int(sys.argv[1]), sys.argv[2])
