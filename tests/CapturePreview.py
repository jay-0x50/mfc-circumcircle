"""Capture an offscreen MFC preview as PNG using ctypes and the standard library.

The process starts hidden, moves outside the desktop, and shows without activation
to exercise the real paint path. This adds no application rendering hooks.
Metadata checks the expected white canvas and black point centers.
Run: python tests/CapturePreview.py --configuration Debug
"""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import struct
import sys
import zlib

from UiSmokeTest import DialogSmoke, THICKNESS, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEMOVE, require


WM_PRINT = 0x0317
PRF_NONCLIENT, PRF_CLIENT, PRF_ERASEBKGND, PRF_CHILDREN = 2, 4, 8, 16
PW_RENDERFULLCONTENT = 2


class BitmapHeader(ctypes.Structure):
    _fields_ = [
        ("biSize", wintypes.DWORD), ("biWidth", wintypes.LONG),
        ("biHeight", wintypes.LONG), ("biPlanes", wintypes.WORD),
        ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
        ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", wintypes.LONG),
        ("biYPelsPerMeter", wintypes.LONG), ("biClrUsed", wintypes.DWORD),
        ("biClrImportant", wintypes.DWORD),
    ]


class BitmapInfo(ctypes.Structure):
    _fields_ = [("bmiHeader", BitmapHeader), ("bmiColors", wintypes.DWORD * 1)]


def save_png(path: Path, width: int, height: int, bgra: bytes) -> None:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    rows = bytearray()
    for y in range(height):
        rows.append(0)  # PNG filter type: none.
        row = bgra[y * width * 4:(y + 1) * width * 4]
        rgb = bytearray(width * 3)
        rgb[0::3], rgb[1::3], rgb[2::3] = row[2::4], row[1::4], row[0::4]
        rows.extend(rgb)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 6))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def capture(app: DialogSmoke, method: str, output: Path) -> dict:
    api = app.api
    user32 = api.user32
    gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
    user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user32.GetWindowRect.restype = wintypes.BOOL
    user32.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
    user32.ClientToScreen.restype = wintypes.BOOL
    user32.GetDC.argtypes = [wintypes.HWND]
    user32.GetDC.restype = wintypes.HDC
    user32.ReleaseDC.argtypes = [wintypes.HWND, wintypes.HDC]
    user32.ReleaseDC.restype = ctypes.c_int
    user32.PrintWindow.argtypes = [wintypes.HWND, wintypes.HDC, wintypes.UINT]
    user32.PrintWindow.restype = wintypes.BOOL
    gdi32.CreateCompatibleDC.argtypes = [wintypes.HDC]
    gdi32.CreateCompatibleDC.restype = wintypes.HDC
    gdi32.CreateDIBSection.argtypes = [
        wintypes.HDC, ctypes.POINTER(BitmapInfo), wintypes.UINT,
        ctypes.POINTER(ctypes.c_void_p), wintypes.HANDLE, wintypes.DWORD
    ]
    gdi32.CreateDIBSection.restype = wintypes.HBITMAP
    gdi32.SelectObject.argtypes = [wintypes.HDC, wintypes.HANDLE]
    gdi32.SelectObject.restype = wintypes.HANDLE
    gdi32.DeleteObject.argtypes = [wintypes.HANDLE]
    gdi32.DeleteObject.restype = wintypes.BOOL
    gdi32.DeleteDC.argtypes = [wintypes.HDC]
    gdi32.DeleteDC.restype = wintypes.BOOL
    gdi32.GdiFlush.argtypes = []
    gdi32.GdiFlush.restype = wintypes.BOOL

    rect = wintypes.RECT()
    require(bool(user32.GetWindowRect(app.hwnd, ctypes.byref(rect))), "Cannot read dialog bounds.")
    width, height = rect.right - rect.left, rect.bottom - rect.top
    require(0 < width <= 8192 and 0 < height <= 8192, "Unexpected dialog dimensions.")
    origin = wintypes.POINT(0, 0)
    require(bool(user32.ClientToScreen(app.canvas, ctypes.byref(origin))), "Cannot map canvas position.")
    canvas_x, canvas_y = origin.x - rect.left, origin.y - rect.top

    screen_dc = user32.GetDC(None)
    require(bool(screen_dc), "Cannot allocate a screen-compatible DC.")
    memory_dc = gdi32.CreateCompatibleDC(screen_dc)
    if not memory_dc:
        user32.ReleaseDC(None, screen_dc)
        raise RuntimeError("Cannot allocate the capture memory DC.")
    bitmap, previous = None, None
    try:
        info = BitmapInfo()
        info.bmiHeader.biSize = ctypes.sizeof(BitmapHeader)
        info.bmiHeader.biWidth = width
        info.bmiHeader.biHeight = -height
        info.bmiHeader.biPlanes = 1
        info.bmiHeader.biBitCount = 32
        bits = ctypes.c_void_p()
        bitmap = gdi32.CreateDIBSection(screen_dc, ctypes.byref(info), 0,
                                        ctypes.byref(bits), None, 0)
        require(bool(bitmap) and bool(bits.value), "Cannot allocate capture bitmap.")
        previous = gdi32.SelectObject(memory_dc, bitmap)
        require(bool(previous), "Cannot select capture bitmap.")
        sentinel = b"\xFF\x00\xFF\x00" * (width * height)
        ctypes.memmove(bits.value, sentinel, len(sentinel))
        if method == "printwindow":
            succeeded = bool(user32.PrintWindow(app.hwnd, memory_dc, PW_RENDERFULLCONTENT))
        else:
            api.send(app.hwnd, WM_PRINT, memory_dc,
                     PRF_NONCLIENT | PRF_CLIENT | PRF_ERASEBKGND | PRF_CHILDREN)
            succeeded = True
        gdi32.GdiFlush()
        pixels = ctypes.string_at(bits.value, width * height * 4)
        save_png(output, width, height, pixels)

        def rgb(x: int, y: int) -> tuple[int, int, int] | None:
            if not (0 <= x < width and 0 <= y < height):
                return None
            index = (y * width + x) * 4
            return pixels[index + 2], pixels[index + 1], pixels[index]

        center_samples = [rgb(canvas_x + x, canvas_y + y) for x, y in app.triangle]
        centers_dark = all(sample is not None and max(sample) < 64 for sample in center_samples)
        dark_pixels = 0
        white_pixels = 0
        sentinel_pixels = 0
        sampled_pixels = 0
        for y in range(max(0, canvas_y), min(height, canvas_y + app.height)):
            for x in range(max(0, canvas_x), min(width, canvas_x + app.width)):
                color = rgb(x, y)
                dark_pixels += max(color) < 64
                white_pixels += min(color) > 240
                sentinel_pixels += color == (255, 0, 255)
                sampled_pixels += 1
        # A failed DWM capture can be uniformly black. Black point centers alone
        # therefore cannot demonstrate that the custom canvas was painted.
        rendered = (centers_dark and sampled_pixels > 0
                    and white_pixels / sampled_pixels > 0.5
                    and dark_pixels / sampled_pixels < 0.2
                    and sentinel_pixels == 0)
        return {
            "method": method, "api_succeeded": succeeded, "png": str(output),
            "image_size": [width, height], "canvas_origin": [canvas_x, canvas_y],
            "canvas_size": [app.width, app.height], "point_center_rgb": center_samples,
            "canvas_dark_pixels": dark_pixels, "canvas_white_pixels": white_pixels,
            "canvas_sampled_pixels": sampled_pixels,
            "canvas_unpainted_sentinel_pixels": sentinel_pixels,
            "all_point_centers_rendered": rendered,
            "note": ("Offscreen capture contains the white canvas and all three black point centers; inspect the PNG for layout and circle rendering."
                     if rendered else "This capture method did not reproduce the custom canvas and cannot verify its pixels."),
        }
    finally:
        if previous:
            gdi32.SelectObject(memory_dc, previous)
        if bitmap:
            gdi32.DeleteObject(bitmap)
        gdi32.DeleteDC(memory_dc)
        user32.ReleaseDC(None, screen_dc)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Debug")
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    require(sys.platform == "win32", "This preview capture requires Windows.")
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    try:
        user32.SetProcessDpiAwarenessContext.argtypes = [wintypes.HANDLE]
        user32.SetProcessDpiAwarenessContext.restype = wintypes.BOOL
        user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    except AttributeError:
        pass
    root = Path(__file__).resolve().parents[1]
    exe = (args.exe or root / "bin" / "x64" / args.configuration / "Circumcircle.exe").resolve()
    output_dir = (args.output_dir or root / "artifacts").resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    app = DialogSmoke(exe, offscreen=True)
    report = {"hidden_launch": True, "offscreen": True,
              "configuration": args.configuration, "captures": []}

    def save_state(state: str, method: str, suffix: str) -> None:
        path = output_dir / f"preview-{args.configuration.lower()}-{suffix}.png"
        try:
            circle = app.verify_circle()
            result = capture(app, method, path)
            result.update({"state": state, "points": app.points(), "circle": circle})
            report["captures"].append(result)
            print(f"{state}/{method}: canvas rendered={result['all_point_centers_rendered']}; {path}")
        except Exception as error:
            report["captures"].append({"state": state, "method": method, "error": str(error)})
            print(f"{state}/{method}: {error}")

    try:
        app.seed()
        ordinary = list(app.triangle)
        for method in ("printwindow", "wm-print"):
            save_state("ordinary_triangle", method, method)

        moved = (app.width // 3, app.height // 2)
        app.mouse(WM_LBUTTONDOWN, ordinary[0], True)
        app.mouse(WM_MOUSEMOVE, moved, True)
        app.triangle = [moved] + ordinary[1:]
        require(app.points() == app.triangle, "Live drag did not update before capture.")
        save_state("live_drag_before_mouse_up", "printwindow", "live-drag")
        app.mouse(WM_LBUTTONUP, moved)

        app.triangle = [(10, 10), (110, 10), (60, 11)]
        app.seed()
        outside = app.verify_circle()
        require(outside["center"][1] < 0 and outside["radius"] > app.height,
                "The clipping preview did not create the requested outside-center circle.")
        save_state("large_circle_center_outside_canvas", "printwindow", "outside-center")

        app.triangle = ordinary
        app.seed()
        app.commit_edit(THICKNESS, "32")
        save_state("ordinary_triangle_thickness_32", "printwindow", "thickness32")
    finally:
        app.cleanup()
        path = output_dir / f"preview-{args.configuration.lower()}.json"
        path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    required = [item for item in report["captures"] if item["method"] == "printwindow"]
    return 0 if len(required) == 4 and all(item.get("all_point_centers_rendered") for item in required) else 1


if __name__ == "__main__":
    sys.exit(main())
