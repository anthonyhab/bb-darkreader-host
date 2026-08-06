#!/usr/bin/env python3
import json
import os
import select
import struct
import subprocess
import tempfile
import time
from pathlib import Path

HOST = os.environ["HOST_BINARY"]


def write_colors(path: Path, background: str = "#101010") -> None:
    colors = {f"color{i}": f"#{i:06x}" for i in range(16)}
    colors["color10"] = "#abcdef"
    special = {
        "background": background,
        "foreground": "#eeeeee",
        "cursor": "#abcdef",
    }
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps({"special": special, "colors": colors}), encoding="utf-8")
    os.replace(temporary, path)


def write_raw(path: Path, value: str) -> None:
    temporary = path.with_suffix(".tmp")
    temporary.write_text(value, encoding="utf-8")
    os.replace(temporary, path)


def frame(message) -> bytes:
    payload = json.dumps(message, separators=(",", ":")).encode()
    return struct.pack("=I", len(payload)) + payload


def send(process: subprocess.Popen, message) -> None:
    process.stdin.write(frame(message))
    process.stdin.flush()


def send_many(process: subprocess.Popen, messages) -> None:
    process.stdin.write(b"".join(frame(message) for message in messages))
    process.stdin.flush()


def read_exact(stream, length: int) -> bytes:
    chunks = []
    remaining = length
    while remaining:
        chunk = stream.read(remaining)
        if not chunk:
            raise AssertionError("native host closed its output early")
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)


def receive(process: subprocess.Popen, timeout: float = 1.0) -> dict:
    ready, _, _ = select.select([process.stdout], [], [], timeout)
    if not ready:
        raise AssertionError("timed out waiting for native host response")
    length = struct.unpack("=I", read_exact(process.stdout, 4))[0]
    return json.loads(read_exact(process.stdout, length))


def assert_no_message(process: subprocess.Popen, timeout: float = 0.15) -> None:
    ready, _, _ = select.select([process.stdout], [], [], timeout)
    if ready:
        raise AssertionError(f"unexpected native message: {receive(process, 0)}")


def assert_error(message: dict, expected: str) -> None:
    assert message == {
        "type": "error",
        "data": {"message": expected},
        "isNative": True,
    }


def assert_reset(message: dict) -> None:
    assert message == {"type": "resetThemeVars", "isNative": True}


def assert_theme_data(message: dict, background: str, foreground: str, selection: str) -> None:
    assert message["type"] == "setTheme"
    assert message["isNative"] is True
    assert message["data"] == {
        "darkSchemeBackgroundColor": background,
        "darkSchemeTextColor": foreground,
        "selectionColor": selection,
    }


def assert_theme(message: dict, background: str) -> None:
    assert_theme_data(message, background, "#eeeeee", "#abcdef")


def receive_theme(process: subprocess.Popen, background: str, timeout: float = 4.0) -> None:
    """Wait for a setTheme carrying `background`.

    The watch retry runs every WATCH_RETRY_MS (250 ms), and a retry can land
    between the test's mkdir() and write_colors() steps, making the host emit
    a transient error/reset first. Tolerate those and keep reading until the
    expected theme arrives or the deadline passes.
    """
    deadline = time.monotonic() + timeout
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise AssertionError(f"no setTheme({background}) within {timeout}s")
        message = receive(process, remaining)
        if message.get("type") == "setTheme":
            return assert_theme(message, background)


def start_process(environment: dict) -> subprocess.Popen:
    return subprocess.Popen(
        [HOST, "moz-extension://darkreader-test/"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        bufsize=0,
        env=environment,
    )


def stop_process(process: subprocess.Popen) -> None:
    process.terminate()
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=2)
    stderr = process.stderr.read().decode(errors="replace")
    if stderr:
        print(stderr, end="", file=os.sys.stderr)


def test_self_healing_watch(root: Path, base_environment: dict) -> None:
    colors_path = root / "late" / "wal" / "colors.json"
    environment = base_environment.copy()
    environment["DARKREADER_COLORS_PATH"] = str(colors_path)

    process = subprocess.Popen(
        [HOST, "moz-extension://darkreader-test/"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        bufsize=0,
        env=environment,
    )
    try:
        assert_no_message(process, 0.1)
        colors_path.parent.mkdir(parents=True)
        write_colors(colors_path, "#334455")
        receive_theme(process, "#334455")

        old_directory = colors_path.parent.with_name("wal-old")
        colors_path.parent.rename(old_directory)
        colors_path.parent.mkdir()
        write_colors(colors_path, "#556677")
        receive_theme(process, "#556677")
    finally:
        stop_process(process)


def test_oversized_frame_exits(environment: dict) -> None:
    # A 65537-byte length header exceeds the 64 KiB frame cap: the host must
    # exit without emitting anything.
    process = start_process(environment)
    try:
        process.stdin.write(struct.pack("=I", 65537))
        process.stdin.flush()
        process.stdin.close()
        assert process.wait(timeout=2) == 0
        assert process.stdout.read() == b""
    finally:
        stop_process(process)


def test_truncated_frames_exit_cleanly(environment: dict) -> None:
    # Zero-length frame, truncated header, and truncated body must not crash
    # or produce output; the host exits cleanly.
    cases = (
        struct.pack("=I", 0),      # zero-length frame
        b"\x05\x00",              # truncated header
        b"\x05\x00\x00\x00ab",   # truncated body
    )
    for raw in cases:
        process = start_process(environment)
        try:
            process.stdin.write(raw)
            process.stdin.flush()
            process.stdin.close()
            assert process.wait(timeout=2) == 0
            assert process.stdout.read() == b""
        finally:
            stop_process(process)


def test_stdin_eof_exits_cleanly(environment: dict) -> None:
    process = start_process(environment)
    try:
        process.stdin.close()
        assert process.wait(timeout=2) == 0
        assert process.stdout.read() == b""
    finally:
        stop_process(process)


def test_invalid_palettes_rejected(colors_path: Path, environment: dict) -> None:
    invalid_palettes = (
        '{"special":{"background":"#10101","foreground":"#eeeeee"},"colors":{}}',   # wrong-length color
        '{"special":{"background":"#10zz10","foreground":"#eeeeee"},"colors":{}}',  # non-hex
        '{"special":{"background":"101010","foreground":"#eeeeee"},"colors":{}}',   # missing '#'
        "",                                                                                 # empty palette
    )
    for palette in invalid_palettes:
        write_raw(colors_path, palette)
        process = start_process(environment)
        try:
            send(process, {"type": "init"})
            assert_error(receive(process), "Palette file is missing or invalid")
            assert_no_message(process)
        finally:
            stop_process(process)


def test_fallback_palettes(colors_path: Path, environment: dict) -> None:
    # special-only, colors-only, and missing color10 (selection falls back to
    # foreground) must all produce the expected setTheme values.
    fallback_palettes = (
        (
            '{"special":{"background":"#101010","foreground":"#eeeeee"}}',
            "#101010", "#eeeeee", "#eeeeee",
        ),
        (
            '{"colors":{"color0":"#000000","color7":"#eeeeee","color10":"#abcdef"}}',
            "#000000", "#eeeeee", "#abcdef",
        ),
        (
            '{"special":{"foreground":"#eeeeee"},'
            '"colors":{"color0":"#000000","color7":"#ffffff"}}',
            "#000000", "#eeeeee", "#eeeeee",
        ),
    )
    for palette, background, foreground, selection in fallback_palettes:
        write_raw(colors_path, palette)
        process = start_process(environment)
        try:
            send(process, {"type": "init"})
            assert_theme_data(receive(process), background, foreground, selection)
            assert_no_message(process)
        finally:
            stop_process(process)


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="darkreader-host-test-") as directory:
        root = Path(directory)
        colors_path = root / "colors.json"
        write_colors(colors_path)

        environment = os.environ.copy()
        environment["DARKREADER_COLORS_PATH"] = str(colors_path)
        environment["ASAN_OPTIONS"] = "detect_leaks=0"

        install_home = root / "install-home"
        (install_home / ".mozilla").mkdir(parents=True)
        install_environment = environment.copy()
        install_environment["HOME"] = str(install_home)
        subprocess.run([HOST, "install"], env=install_environment, check=True, capture_output=True)
        manifest = json.loads((install_home / ".mozilla/native-messaging-hosts/darkreader.json").read_text())
        assert manifest["allowed_extensions"] == [
            "coloreader@bb.hab.rip",
            "darkreader@alexhulbert.com",
        ]

        process = subprocess.Popen(
            [HOST, "moz-extension://darkreader-test/"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0,
            env=environment,
        )
        try:
            assert_no_message(process)

            send(process, "init")
            assert_theme(receive(process), "#101010")

            write_colors(colors_path, "#101010")
            assert_no_message(process)

            write_raw(colors_path, '{"special":{"background":"#101010"}')
            assert_error(receive(process), "Palette file is missing or invalid")

            duplicate_keys = (
                '{"special":{"background":"#101010",'
                '"background":"#202020","foreground":"#eeeeee",'
                '"cursor":"#abcdef"},"colors":{"color0":"#000000"}}'
            )
            write_raw(colors_path, duplicate_keys)
            assert_no_message(process)
            send(process, {"type": "init"})
            assert_error(receive(process), "Palette file is missing or invalid")

            write_colors(colors_path, "#101010")
            assert_no_message(process)

            started = time.monotonic()
            write_colors(colors_path, "#202020")
            changed = receive(process)
            elapsed_ms = (time.monotonic() - started) * 1000
            assert_theme(changed, "#202020")
            assert elapsed_ms < 500, elapsed_ms
            assert_no_message(process, 0.05)

            send(process, {"type": "init"})
            assert_theme(receive(process), "#202020")

            send_many(process, [
                {"type": "init"},
                {"type": "reset"},
            ])
            assert_theme(receive(process), "#202020")
            assert_reset(receive(process))
            send(process, {"type": "init"})
            assert_theme(receive(process), "#202020")

            send(process, {"type": "unknown"})
            assert_error(receive(process), "Unsupported native request")

            colors_path.unlink()
            assert_reset(receive(process))

            print(
                "strict JSON, framed set/reset/error, atomic inotify, and dedup "
                f"passed ({elapsed_ms:.2f} ms)"
            )
        finally:
            stop_process(process)

        test_oversized_frame_exits(environment)
        test_truncated_frames_exit_cleanly(environment)
        test_stdin_eof_exits_cleanly(environment)
        test_invalid_palettes_rejected(colors_path, environment)
        test_fallback_palettes(colors_path, environment)
        test_self_healing_watch(root, environment)


if __name__ == "__main__":
    main()
