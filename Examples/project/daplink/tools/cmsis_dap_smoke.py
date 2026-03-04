#!/usr/bin/env python3
"""Minimal CMSIS-DAP HID smoke test for Charm daplink firmware."""

from __future__ import annotations

import argparse
import sys
from typing import Optional


PACKET_SIZE = 64

CMD_DAP_INFO = 0x00
CMD_DAP_HOST_STATUS = 0x01
CMD_DAP_CONNECT = 0x02
CMD_DAP_DISCONNECT = 0x03
CMD_DAP_TRANSFER = 0x05
CMD_DAP_TRANSFER_BLOCK = 0x06
CMD_DAP_RESET_TARGET = 0x0A
CMD_DAP_SWJ_CLOCK = 0x11

DAP_INFO_VENDOR = 0x01
DAP_INFO_PRODUCT = 0x02
DAP_INFO_SERIAL = 0x03
DAP_INFO_FW = 0x04

TRANSFER_REQ_DP_IDCODE_READ = 0x02  # APnDP=0,RnW=1,A2=0,A3=0


def packet(data: list[int]) -> bytes:
    if len(data) > PACKET_SIZE:
        raise ValueError("packet too large")
    return bytes(data + [0] * (PACKET_SIZE - len(data)))


def u32le(value: int) -> list[int]:
    return [
        value & 0xFF,
        (value >> 8) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 24) & 0xFF,
    ]


def open_hid(vid: int, pid: int):
    try:
        import hid  # type: ignore
    except ImportError as exc:
        raise RuntimeError("missing dependency: hidapi python package 'hid'") from exc

    dev = hid.device()
    dev.open(vid, pid)
    dev.set_nonblocking(False)
    return dev


def read_response(dev, timeout_ms: int) -> bytes:
    data = dev.read(PACKET_SIZE, timeout_ms)
    if not data:
        raise TimeoutError("read timeout")
    return bytes(data)


def send_recv(dev, payload: list[int], timeout_ms: int) -> bytes:
    dev.write(packet(payload))
    return read_response(dev, timeout_ms)


def parse_info(resp: bytes) -> str:
    n = resp[1]
    raw = resp[2 : 2 + n]
    return raw.rstrip(b"\x00").decode(errors="replace")


def run(vid: int, pid: int, timeout_ms: int, swj_clock_hz: int, skip_reset: bool) -> int:
    dev = open_hid(vid, pid)
    try:
        for info_id, name in [
            (DAP_INFO_VENDOR, "Vendor"),
            (DAP_INFO_PRODUCT, "Product"),
            (DAP_INFO_SERIAL, "Serial"),
            (DAP_INFO_FW, "Firmware"),
        ]:
            resp = send_recv(dev, [CMD_DAP_INFO, info_id], timeout_ms)
            print(f"{name}: {parse_info(resp)}")

        resp = send_recv(dev, [CMD_DAP_CONNECT, 0x00], timeout_ms)
        port = resp[1]
        print(f"Connect Port: {port}")
        if port == 0:
            print("connect failed")
            return 2

        resp = send_recv(dev, [CMD_DAP_HOST_STATUS, 0x00, 0x01], timeout_ms)
        if resp[1] != 0x00:
            print(f"host status (connect led) failed: 0x{resp[1]:02X}")
            return 5
        resp = send_recv(dev, [CMD_DAP_HOST_STATUS, 0x01, 0x01], timeout_ms)
        if resp[1] != 0x00:
            print(f"host status (running led) failed: 0x{resp[1]:02X}")
            return 5

        resp = send_recv(dev, [CMD_DAP_SWJ_CLOCK, *u32le(swj_clock_hz)], timeout_ms)
        if resp[1] != 0x00:
            print(f"swj clock failed: 0x{resp[1]:02X}")
            return 6

        resp = send_recv(
            dev,
            [CMD_DAP_TRANSFER, 0x00, 0x01, TRANSFER_REQ_DP_IDCODE_READ],
            timeout_ms,
        )
        count = resp[1]
        status = resp[2]
        print(f"Transfer Count={count}, Status=0x{status:02X}")
        if count == 1 and status == 0x01:
            idcode = int.from_bytes(resp[3:7], "little")
            print(f"DP IDCODE: 0x{idcode:08X}")
        else:
            print("IDCODE read failed")
            return 3

        resp = send_recv(
            dev,
            [CMD_DAP_TRANSFER_BLOCK, 0x01, 0x00, TRANSFER_REQ_DP_IDCODE_READ],
            timeout_ms,
        )
        block_count = int.from_bytes(resp[1:3], "little")
        block_status = resp[3]
        print(f"TransferBlock Count={block_count}, Status=0x{block_status:02X}")
        if block_count == 1 and block_status == 0x01:
            blk_idcode = int.from_bytes(resp[4:8], "little")
            print(f"DP IDCODE (block): 0x{blk_idcode:08X}")
        else:
            print("IDCODE block read failed")
            return 4

        if not skip_reset:
            resp = send_recv(dev, [CMD_DAP_RESET_TARGET], timeout_ms)
            if resp[1] == 0:
                print("ResetTarget not supported by device")
            else:
                print("ResetTarget OK")

        send_recv(dev, [CMD_DAP_HOST_STATUS, 0x01, 0x00], timeout_ms)
        send_recv(dev, [CMD_DAP_HOST_STATUS, 0x00, 0x00], timeout_ms)
        send_recv(dev, [CMD_DAP_DISCONNECT], timeout_ms)
        print("Smoke test passed.")
        return 0
    finally:
        dev.close()


def parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="CMSIS-DAP HID smoke test")
    p.add_argument("--vid", type=lambda x: int(x, 0), default=0xCAFE, help="USB VID, default 0xCAFE")
    p.add_argument("--pid", type=lambda x: int(x, 0), default=0x4001, help="USB PID, default 0x4001")
    p.add_argument("--timeout-ms", type=int, default=1000, help="HID read timeout in ms")
    p.add_argument("--swj-clock-hz", type=int, default=1000000, help="SWJ clock in Hz")
    p.add_argument("--skip-reset", action="store_true", help="skip ResetTarget command")
    return p.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv)
    try:
        return run(args.vid, args.pid, args.timeout_ms, args.swj_clock_hz, args.skip_reset)
    except Exception as exc:  # noqa: BLE001
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
