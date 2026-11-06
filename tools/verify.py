import argparse
import ctypes as c
import json
import time
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(description="Verify ADX IO4 bridge decoding and forwarding")
    parser.add_argument("capture", type=Path)
    parser.add_argument("--bridge", type=Path, required=True)
    parser.add_argument("--live", action="store_true")
    parser.add_argument("--stub", type=Path)
    return parser.parse_args()


def load_reports(path):
    return [
        bytes.fromhex(json.loads(line)["data"])
        for line in path.read_text().splitlines()
        if line.strip()
    ]


def main():
    args = parse_args()
    bridge = c.CDLL(str(args.bridge.resolve()))
    bridge.adx_decode_report.argtypes = [c.POINTER(c.c_uint8), c.c_uint32]
    bridge.adx_decode_report.restype = c.c_uint32
    reports = load_reports(args.capture)
    base = reports[0]

    def decode(report):
        return bridge.adx_decode_report(
            (c.c_uint8 * len(report))(*report), len(report)
        )

    assert decode(base) == 0
    bits = [4, 8, 1, 0x8000, 0x4000, 0x2000, 0x1000, 0x800]
    for index, bit in enumerate(bits):
        report = bytearray(base)
        raw = int.from_bytes(report[29:31], "little") & ~bit
        report[29:31] = raw.to_bytes(2, "little")
        assert decode(report) == 1 << index

    for mask, expected in [(2, 0x100), (0x200, 0x10000), (0x40, 0x20000)]:
        report = bytearray(base)
        raw = int.from_bytes(report[29:31], "little") | mask
        report[29:31] = raw.to_bytes(2, "little")
        assert decode(report) == expected

    assert decode(base[:32]) == 0
    bad_id = bytearray(base)
    bad_id[0] = 2
    assert decode(bad_id) == 0
    states = {decode(report) for report in reports}
    expected_states = {1 << index for index in range(8)} | {
        0,
        0x100,
        0x10000,
        0x20000,
    }
    assert expected_states <= states
    print("PASS: captured ring states, SELECT/TEST/SERVICE, idle and malformed reports")

    if args.live:
        if args.stub is None:
            raise SystemExit("--stub is required with --live")
        stub = c.CDLL(str(args.stub.resolve()))
        assert bridge.mai2_io_init() == 0
        time.sleep(0.3)
        assert bridge.adx_get_report_count() > 5
        assert bridge.mai2_io_poll() == 0
        p1, p2, op = c.c_uint16(), c.c_uint16(), c.c_uint8()
        bridge.mai2_io_get_gamebtns(c.byref(p1), c.byref(p2))
        bridge.mai2_io_get_opbtns(c.byref(op))
        assert (p1.value, p2.value, op.value) == (0x100, 0x80, 2)
        data = (c.c_uint8 * 32)(77)
        assert bridge.mai2_io_touch_init(c.c_void_p(123)) == 42
        bridge.mai2_io_touch_set_sens(data)
        bridge.mai2_io_touch_update(1, 0)
        assert bridge.mai2_io_led_init() == 43
        for name in [
            "mai2_io_led_set_fet_output",
            "mai2_io_led_dc_update",
            "mai2_io_led_gs_update",
            "mai2_io_led_billboard_set",
        ]:
            getattr(bridge, name)(1, data)
        bridge.mai2_io_led_cam_set(77)
        assert stub.test_flags() == 1023
        print("PASS: live reader, keyboard preservation, touch and LED forwarding")


if __name__ == "__main__":
    main()
