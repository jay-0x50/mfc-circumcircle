"""Exercise the real MFC dialog using only Python's Windows standard library.

Run from the repository root:
    python tests/UiSmokeTest.py --configuration Debug --offscreen
    python tests/UiSmokeTest.py --configuration Release --offscreen

Mouse messages use canvas-local coordinates. SendMessageTimeout imposes a 100 ms
responsiveness deadline, including while the worker is moving the points.
This checks observable UI behavior; GeometryTests covers raster pixels and geometry.
"""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import traceback


TITLE = "3-Point Circumcircle"
CANVAS, RADIUS, THICKNESS = 1000, 1001, 1002
POINT_IDS = (1003, 1004, 1005)
RESET, RANDOM, STATUS, PROGRESS = 1006, 1007, 1008, 1009
VALIDATION, CIRCLE_INFO = 1010, 1011
WM_NULL, WM_SETTEXT, WM_GETTEXT = 0x0000, 0x000C, 0x000D
WM_CLOSE, WM_COMMAND = 0x0010, 0x0111
WM_MOUSEMOVE, WM_LBUTTONDOWN, WM_LBUTTONUP = 0x0200, 0x0201, 0x0202
WM_CAPTURECHANGED = 0x0215
BM_CLICK, EN_KILLFOCUS, MK_LBUTTON = 0x00F5, 0x0200, 1
SMTO_ABORTIFHUNG, SW_HIDE = 0x0002, 0


class SmokeFailure(AssertionError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SmokeFailure(message)


class Windows:
    def __init__(self) -> None:
        self.user32 = ctypes.WinDLL("user32", use_last_error=True)
        self.callback_type = ctypes.WINFUNCTYPE(
            wintypes.BOOL, wintypes.HWND, ctypes.c_ssize_t
        )
        self.user32.EnumWindows.argtypes = [self.callback_type, ctypes.c_ssize_t]
        self.user32.EnumWindows.restype = wintypes.BOOL
        self.user32.GetWindowThreadProcessId.argtypes = [
            wintypes.HWND, ctypes.POINTER(wintypes.DWORD)
        ]
        self.user32.GetWindowThreadProcessId.restype = wintypes.DWORD
        self.user32.GetWindowTextW.argtypes = [
            wintypes.HWND, wintypes.LPWSTR, ctypes.c_int
        ]
        self.user32.GetWindowTextW.restype = ctypes.c_int
        self.user32.GetDlgItem.argtypes = [wintypes.HWND, ctypes.c_int]
        self.user32.GetDlgItem.restype = wintypes.HWND
        self.user32.GetClientRect.argtypes = [
            wintypes.HWND, ctypes.POINTER(wintypes.RECT)
        ]
        self.user32.GetClientRect.restype = wintypes.BOOL
        self.user32.IsWindowEnabled.argtypes = [wintypes.HWND]
        self.user32.IsWindowEnabled.restype = wintypes.BOOL
        self.user32.IsWindow.argtypes = [wintypes.HWND]
        self.user32.IsWindow.restype = wintypes.BOOL
        self.user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
        self.user32.ShowWindow.restype = wintypes.BOOL
        self.user32.SetWindowPos.argtypes = [
            wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int,
            ctypes.c_int, ctypes.c_int, wintypes.UINT
        ]
        self.user32.SetWindowPos.restype = wintypes.BOOL
        self.user32.GetSystemMetrics.argtypes = [ctypes.c_int]
        self.user32.GetSystemMetrics.restype = ctypes.c_int
        self.user32.UpdateWindow.argtypes = [wintypes.HWND]
        self.user32.UpdateWindow.restype = wintypes.BOOL
        self.user32.PostMessageW.argtypes = [
            wintypes.HWND, wintypes.UINT, ctypes.c_size_t, ctypes.c_ssize_t
        ]
        self.user32.PostMessageW.restype = wintypes.BOOL
        self.user32.SendMessageTimeoutW.argtypes = [
            wintypes.HWND, wintypes.UINT, ctypes.c_size_t, ctypes.c_ssize_t,
            wintypes.UINT, wintypes.UINT, ctypes.POINTER(ctypes.c_size_t)
        ]
        self.user32.SendMessageTimeoutW.restype = ctypes.c_ssize_t

    def find_dialog(self, process_id: int) -> int | None:
        found: list[int] = []

        @self.callback_type
        def visit(hwnd: int, _context: int) -> bool:
            owner = wintypes.DWORD()
            self.user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
            if owner.value == process_id:
                title = ctypes.create_unicode_buffer(256)
                self.user32.GetWindowTextW(hwnd, title, len(title))
                if title.value == TITLE:
                    found.append(hwnd)
                    return False
            return True

        self.user32.EnumWindows(visit, 0)
        return found[0] if found else None

    def send(self, hwnd: int, message: int, wparam: int = 0,
             lparam: int = 0) -> tuple[int, float]:
        result = ctypes.c_size_t()
        ctypes.set_last_error(0)
        start = time.perf_counter()
        success = self.user32.SendMessageTimeoutW(
            hwnd, message, wparam, lparam, SMTO_ABORTIFHUNG, 100,
            ctypes.byref(result)
        )
        elapsed = time.perf_counter() - start
        if not success:
            error = ctypes.get_last_error()
            raise SmokeFailure(
                f"Message 0x{message:04X} failed or exceeded 100 ms "
                f"(Win32 error {error}, elapsed {elapsed * 1000:.1f} ms)."
            )
        return result.value, elapsed

    def text(self, hwnd: int) -> str:
        value = ctypes.create_unicode_buffer(2048)
        self.send(hwnd, WM_GETTEXT, len(value), ctypes.addressof(value))
        return value.value

    def set_text(self, hwnd: int, value: str) -> None:
        buffer = ctypes.create_unicode_buffer(value)
        self.send(hwnd, WM_SETTEXT, 0, ctypes.addressof(buffer))

    def size(self, hwnd: int) -> tuple[int, int]:
        rect = wintypes.RECT()
        require(bool(self.user32.GetClientRect(hwnd, ctypes.byref(rect))),
                "Cannot read the canvas client rectangle.")
        return rect.right - rect.left, rect.bottom - rect.top


class DialogSmoke:
    def __init__(self, exe: Path, offscreen: bool = False) -> None:
        self.exe = exe
        self.offscreen = offscreen
        self.api = Windows()
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = SW_HIDE
        self.process = subprocess.Popen(
            [str(exe)], cwd=str(exe.parent), startupinfo=startup,
            creationflags=subprocess.CREATE_NO_WINDOW
        )
        self.hwnd = 0
        try:
            deadline = time.perf_counter() + 10
            while time.perf_counter() < deadline:
                require(self.process.poll() is None,
                        f"App exited during startup: {self.process.returncode}.")
                window = self.api.find_dialog(self.process.pid)
                if window:
                    self.hwnd = window
                    controls = {
                        control_id: self.api.user32.GetDlgItem(window, control_id)
                        for control_id in range(CANVAS, CIRCLE_INFO + 1)
                    }
                    # A dialog HWND exists before its template finishes creating
                    # children. Wait for both the template and OnInitDialog values.
                    if (all(controls.values())
                            and self.api.text(controls[RADIUS]).strip() == "6"
                            and self.api.text(controls[THICKNESS]).strip() == "2"):
                        break
                time.sleep(0.02)
            require(bool(self.hwnd), f"Window {TITLE!r} was not created.")
            self.api.user32.ShowWindow(self.hwnd, SW_HIDE)
            self.controls = {
                control_id: self.api.user32.GetDlgItem(self.hwnd, control_id)
                for control_id in range(CANVAS, CIRCLE_INFO + 1)
            }
            missing = [key for key, value in self.controls.items() if not value]
            require(not missing, f"Missing required dialog controls: {missing}.")
            self.canvas = self.controls[CANVAS]
            self.width, self.height = self.api.size(self.canvas)
            require(self.width >= 200 and self.height >= 160,
                    f"Canvas is too small: {self.width} x {self.height}.")
            self.triangle = [
                (self.width // 5, self.height // 3),
                (self.width // 2, self.height // 5),
                (self.width * 3 // 4, self.height * 3 // 4),
            ]
            if offscreen:
                # Show only after moving beyond the desktop, and never activate.
                # This exercises the real WM_PAINT path without covering user UI.
                dialog_width, _ = self.api.size(self.hwnd)
                x = min(-20000, self.api.user32.GetSystemMetrics(76) - dialog_width - 512)
                require(bool(self.api.user32.SetWindowPos(
                    self.hwnd, None, x, -20000, 0, 0, 0x0001 | 0x0004 | 0x0010
                )), "Could not move the test dialog offscreen.")
                self.api.user32.ShowWindow(self.hwnd, 4)  # SW_SHOWNOACTIVATE
                self.api.user32.UpdateWindow(self.hwnd)
                self.api.user32.UpdateWindow(self.canvas)
        except Exception:
            self.cleanup()
            raise

    def control_text(self, control_id: int) -> str:
        return self.api.text(self.controls[control_id])

    def enabled(self, control_id: int) -> bool:
        return bool(self.api.user32.IsWindowEnabled(self.controls[control_id]))

    def points(self) -> list[tuple[int, int] | None]:
        points: list[tuple[int, int] | None] = []
        for index, control_id in enumerate(POINT_IDS, 1):
            value = self.control_text(control_id)
            match = re.fullmatch(
                rf"\s*Point\s+{index}\s*:\s*\(\s*(-?\d+)\s*,\s*(-?\d+)\s*\)\s*",
                value
            )
            if match:
                points.append((int(match[1]), int(match[2])))
            else:
                require(bool(re.fullmatch(rf"\s*Point\s+{index}\s*:\s*-\s*", value)),
                        f"Invalid coordinate label: {value!r}.")
                points.append(None)
        return points

    def progress(self) -> int:
        value = self.control_text(PROGRESS)
        match = re.search(r"(\d+)\s*/\s*10\s*$", value)
        require(match is not None, f"Invalid random progress label: {value!r}.")
        return int(match[1])

    def verify_circle(self) -> dict:
        info = self.control_text(CIRCLE_INFO)
        numbers = re.findall(r"-?\d+(?:\.\d+)?", info)
        require(len(numbers) == 3, f"Missing circumcircle center/radius: {info!r}.")
        center_x, center_y, radius = map(float, numbers)
        require(radius > 0 and all(math.isfinite(value) for value in (center_x, center_y, radius)),
                f"Invalid circumcircle values: {info!r}.")
        for point in self.points():
            require(point is not None, "A circumcircle was shown without three points.")
            # UI numbers are rounded to two decimals, so allow their rounding error.
            error = abs(math.hypot(point[0] - center_x, point[1] - center_y) - radius)
            require(error < 0.025, f"Circumcircle misses point {point} by {error:.5f} pixels.")
        return {"center": [center_x, center_y], "radius": radius}

    def mouse(self, message: int, point: tuple[int, int],
              button: bool = False, hwnd: int | None = None) -> None:
        x, y = point
        self.api.send(hwnd or self.canvas, message, MK_LBUTTON if button else 0,
                      (x & 0xFFFF) | ((y & 0xFFFF) << 16))

    def click(self, point: tuple[int, int], hwnd: int | None = None) -> None:
        self.mouse(WM_LBUTTONDOWN, point, True, hwnd)
        self.mouse(WM_LBUTTONUP, point, False, hwnd)

    def button(self, control_id: int) -> None:
        self.api.send(self.controls[control_id], BM_CLICK)

    def reset(self) -> None:
        self.button(RESET)
        require(self.points() == [None] * 3, "Reset did not clear all coordinate labels.")
        require(self.progress() == 0, "Reset did not clear random progress.")
        require(not self.enabled(RANDOM), "Random movement must be disabled with no points.")

    def seed(self) -> None:
        self.reset()
        for point in self.triangle:
            self.click(point)
        require(self.points() == self.triangle, "Triangle coordinates do not match mouse input.")
        require(self.enabled(RANDOM), "Random movement is disabled for a valid triangle.")

    def commit_edit(self, control_id: int, value: str) -> None:
        control = self.controls[control_id]
        self.api.set_text(control, value)
        # Deliver the standard focus-loss notification without stealing desktop focus.
        self.api.send(self.hwnd, WM_COMMAND,
                      control_id | (EN_KILLFOCUS << 16), control)

    def initial_controls(self) -> dict:
        require(self.points() == [None] * 3, "Points exist on startup.")
        require(self.control_text(RADIUS).strip() == "6", "Default point radius is not 6.")
        require(self.control_text(THICKNESS).strip() == "2", "Default circle thickness is not 2.")
        require(self.progress() == 0, "Initial progress must be 0 / 10.")
        require(not self.enabled(RANDOM), "Random button is enabled before a circle exists.")
        return {"canvas_size": [self.width, self.height], "status": self.control_text(STATUS)}

    def click_and_drag(self) -> dict:
        self.reset()
        circle_states = []
        for index, point in enumerate(self.triangle):
            self.mouse(WM_LBUTTONDOWN, point, True)
            expected = self.triangle[:index + 1] + [None] * (2 - index)
            require(self.points() == expected, f"Click {index + 1} did not immediately update labels.")
            require(self.enabled(RANDOM) == (index == 2),
                    "Circle validity/random-button state is wrong immediately after a click.")
            if index == 2:
                circle_states.append(self.verify_circle())
            self.mouse(WM_LBUTTONUP, point)
        self.click((self.width * 4 // 5, self.height // 5))
        require(self.points() == self.triangle, "A fourth empty-space click changed the three points.")

        live_positions = [
            (self.width // 4, self.height // 2),
            (self.width // 3, self.height * 3 // 5),
        ]
        self.mouse(WM_LBUTTONDOWN, self.triangle[0], True)
        for point in live_positions:
            self.mouse(WM_MOUSEMOVE, point, True)
            require(self.points() == [point] + self.triangle[1:],
                    "Drag did not update point coordinates before mouse-up.")
            require(self.enabled(RANDOM), "Dragging a valid triangle lost its circle.")
            circle_states.append(self.verify_circle())
        self.mouse(WM_LBUTTONUP, live_positions[-1])
        stopped = self.points()
        self.mouse(WM_MOUSEMOVE, self.triangle[0])
        require(self.points() == stopped, "Point moved after mouse-up ended the drag.")
        return {"click_points": self.triangle, "live_drag_positions": live_positions,
                "circle_states": circle_states, "status": self.control_text(STATUS)}

    def input_isolation_and_capture(self) -> dict:
        self.reset()
        self.click((3, 3), self.hwnd)
        for control_id in (RADIUS, THICKNESS, STATUS, *POINT_IDS):
            self.click((3, 3), self.controls[control_id])
        require(self.points() == [None] * 3, "Clicking outside the canvas created a point.")
        self.seed()
        self.mouse(WM_LBUTTONDOWN, self.triangle[0], True)
        self.api.send(self.canvas, WM_CAPTURECHANGED)
        self.mouse(WM_MOUSEMOVE, (self.width // 3, self.height // 2), True)
        require(self.points() == self.triangle, "Drag continued after capture loss.")
        self.mouse(WM_LBUTTONUP, self.triangle[0])

        self.mouse(WM_LBUTTONDOWN, self.triangle[0], True)
        self.mouse(WM_MOUSEMOVE, (-80, self.height + 80), True)
        dragged = self.points()[0]
        require(dragged is not None, "Dragged point disappeared.")
        require(0 <= dragged[0] < self.width and 0 <= dragged[1] < self.height,
                f"Dragging beyond the canvas did not clamp the point center: {dragged}.")
        self.mouse(WM_LBUTTONUP, (-80, self.height + 80))
        self.reset()
        self.click(self.triangle[0])
        require(self.points() == [self.triangle[0], None, None],
                "Reset did not allow a new first point.")
        return {"clamped_drag_point": dragged}

    def collinear_points(self) -> dict:
        self.reset()
        y = self.height // 2
        line = [(self.width // 5, y), (self.width // 2, y), (self.width * 4 // 5, y)]
        for point in line:
            self.click(point)
        require(self.points() == line, "Collinear points were not retained.")
        require(not self.enabled(RANDOM), "Random movement is enabled without a valid circumcircle.")
        require(not self.control_text(CIRCLE_INFO).strip(),
                "Collinear points retained a stale circumcircle description.")
        status = self.control_text(STATUS)
        require(any(term in status.lower() for term in ("일직선", "collinear", "직선")),
                f"Collinear points have no explanatory status: {status!r}.")
        self.api.send(self.hwnd, WM_NULL)
        return {"points": line, "status": status}

    def numeric_validation(self) -> dict:
        self.seed()
        observations = []
        for control_id, valid_value in ((RADIUS, "9"), (THICKNESS, "4")):
            self.commit_edit(control_id, valid_value)
            require(self.control_text(control_id).strip() == valid_value,
                    f"Valid numeric input {valid_value} was rejected by control {control_id}.")
            for invalid in ("0", "-5", "999999999999999", "", "abc", "2.5"):
                previous_status = self.control_text(VALIDATION)
                self.commit_edit(control_id, invalid)
                displayed = self.control_text(control_id).strip()
                status = self.control_text(VALIDATION)
                maximum = 64 if control_id == RADIUS else 32
                normalized = bool(re.fullmatch(r"[0-9]+", displayed)) and 1 <= int(displayed) <= maximum
                warning = status != previous_status and bool(status.strip())
                require(normalized or warning,
                        f"Invalid input {invalid!r} was neither normalized nor explained "
                        f"(control {control_id}, displayed {displayed!r}, status {status!r}).")
                self.api.send(self.hwnd, WM_NULL)
                require(self.points() == self.triangle, "Numeric validation changed point coordinates.")
                observations.append({"control": control_id, "input": invalid,
                                     "displayed": displayed, "status": status})
                self.commit_edit(control_id, valid_value)
        self.commit_edit(RADIUS, "6")
        self.commit_edit(THICKNESS, "2")
        return {"invalid_inputs": observations}

    def random_ten_steps(self) -> dict:
        self.seed()
        self.commit_edit(RADIUS, "9")
        radius = int(self.control_text(RADIUS).strip())
        previous = self.points()
        start = time.perf_counter()
        self.button(RANDOM)
        require(not self.enabled(RANDOM), "Random button was not disabled while running.")
        require(self.progress() == 0, "Random movement did not start with 0 / 10 progress.")
        events = []
        response_ms = []
        last_count = 0
        while time.perf_counter() - start < 7:
            _, response = self.api.send(self.hwnd, WM_NULL)
            response_ms.append(response * 1000)
            count = self.progress()
            if count != last_count:
                require(count == last_count + 1, f"Random progress skipped or repeated a step: {last_count} -> {count}.")
                coordinates = self.points()
                # If a worker notification arrived between reads, retry the snapshot.
                if self.progress() != count:
                    continue
                require(coordinates != previous, f"Step {count} did not move the points.")
                for point in coordinates:
                    require(point is not None, f"Step {count} removed a point.")
                    require(radius <= point[0] <= self.width - 1 - radius and
                            radius <= point[1] <= self.height - 1 - radius,
                            f"Step {count} placed a point disk beyond the canvas: {point}.")
                events.append({"step": count, "elapsed_seconds": round(time.perf_counter() - start, 4),
                               "points": coordinates})
                previous, last_count = coordinates, count
                if count == 10:
                    break
            time.sleep(0.02)
        require(last_count == 10, f"Expected 10 random moves; observed {last_count}.")
        times = [entry["elapsed_seconds"] for entry in events]
        intervals = [times[0]] + [b - a for a, b in zip(times, times[1:])]
        require(all(0.35 <= interval <= 0.85 for interval in intervals),
                f"Random movement is not occurring every 500 ms: {intervals}.")
        require(4.5 <= times[-1] <= 6.0,
                f"Ten random steps took {times[-1]:.3f} s, expected about 5 s.")
        require(self.enabled(RANDOM), "Random button did not re-enable after step 10.")
        stopped = self.points()
        stop_deadline = time.perf_counter() + 0.7
        while time.perf_counter() < stop_deadline:
            self.api.send(self.hwnd, WM_NULL)
            require(self.progress() == 10 and self.points() == stopped,
                    "An extra random move occurred after step 10.")
            time.sleep(0.025)
        self.commit_edit(RADIUS, "6")
        return {"events": events, "interval_seconds": [round(value, 4) for value in intervals],
                "responsiveness_samples": len(response_ms),
                "maximum_wm_null_ms": round(max(response_ms), 3),
                "extra_step_observation_seconds": 0.7}

    def wait_for_progress(self, minimum: int) -> None:
        deadline = time.perf_counter() + 2
        while time.perf_counter() < deadline:
            self.api.send(self.hwnd, WM_NULL)
            if self.progress() >= minimum:
                return
            time.sleep(0.02)
        raise SmokeFailure(f"Random movement did not reach step {minimum}.")

    def reset_during_random(self) -> dict:
        self.seed()
        self.button(RANDOM)
        self.wait_for_progress(1)
        started = time.perf_counter()
        self.reset()
        elapsed = time.perf_counter() - started
        deadline = time.perf_counter() + 0.8
        while time.perf_counter() < deadline:
            self.api.send(self.hwnd, WM_NULL)
            require(self.points() == [None] * 3 and self.progress() == 0,
                    "A stale worker update restored points after reset.")
            time.sleep(0.025)
        self.seed()
        self.button(RANDOM)
        require(self.progress() == 0, "Restart reused the previous random run's progress.")
        self.wait_for_progress(1)
        self.reset()
        return {"reset_elapsed_ms": round(elapsed * 1000, 3),
                "stale_update_observation_seconds": 0.8, "restart_verified": True}

    def close_during_random(self) -> dict:
        self.seed()
        self.button(RANDOM)
        self.wait_for_progress(1)
        start = time.perf_counter()
        require(bool(self.api.user32.PostMessageW(self.hwnd, WM_CLOSE, 0, 0)),
                "Could not post WM_CLOSE.")
        try:
            code = self.process.wait(timeout=3)
        except subprocess.TimeoutExpired as error:
            raise SmokeFailure("The app did not exit within 3 s while its worker was running.") from error
        close_elapsed = time.perf_counter() - start
        # MFC dialog apps can retain IDCANCEL (2) as their normal process result.
        # Compare with an idle close instead of misreporting that result as a crash.
        baseline = DialogSmoke(self.exe, offscreen=self.offscreen)
        try:
            require(bool(baseline.api.user32.PostMessageW(baseline.hwnd, WM_CLOSE, 0, 0)),
                    "Could not post the idle baseline WM_CLOSE.")
            normal_code = baseline.process.wait(timeout=3)
        finally:
            baseline.cleanup()
        require(code in (0, 2) and code == normal_code,
                f"Worker close exit code {code} differs from a normal idle exit ({normal_code}).")
        return {"exit_code": code, "idle_exit_code": normal_code,
                "close_elapsed_ms": round(close_elapsed * 1000, 3)}

    def snapshot(self) -> dict:
        if not self.hwnd or self.process.poll() is not None:
            return {"process_exit_code": self.process.poll()}
        try:
            return {"points": self.points(), "status": self.control_text(STATUS),
                    "progress": self.control_text(PROGRESS)}
        except Exception as error:
            return {"snapshot_error": str(error)}

    def cleanup(self) -> None:
        if self.process.poll() is not None:
            return
        if self.hwnd:
            self.api.user32.PostMessageW(self.hwnd, WM_CLOSE, 0, 0)
        try:
            self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=3)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--configuration", choices=("Debug", "Release"), default="Debug")
    parser.add_argument("--exe", type=Path, help="Path to the real MFC Circumcircle executable")
    parser.add_argument("--output", type=Path, help="Override the JSON result path")
    parser.add_argument("--offscreen", action="store_true",
                        help="Show beyond the desktop without activation to exercise WM_PAINT")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    exe = (args.exe or root / "bin" / "x64" / args.configuration / "Circumcircle.exe").resolve()
    output = (args.output or root / "artifacts" / f"ui-smoke-{args.configuration.lower()}.json").resolve()
    report = {"configuration": args.configuration, "executable": str(exe),
              "started_utc": datetime.now(timezone.utc).isoformat(),
              "method": "ctypes Win32 messages against the real MFC process; hidden launch",
              "offscreen_paint_enabled": args.offscreen,
              "message_timeout_ms": 100, "checks": [], "passed": False}
    app = None
    started = time.perf_counter()
    try:
        require(os.name == "nt", "This UI integration test requires Windows.")
        require(exe.is_file(), f"Executable does not exist: {exe}")
        app = DialogSmoke(exe, offscreen=args.offscreen)
        for name in (
            "initial_controls", "click_and_drag", "input_isolation_and_capture",
            "collinear_points", "numeric_validation", "random_ten_steps",
            "reset_during_random", "close_during_random",
        ):
            step_start = time.perf_counter()
            try:
                details = getattr(app, name)()
                report["checks"].append({"name": name, "passed": True,
                                         "elapsed_seconds": round(time.perf_counter() - step_start, 4),
                                         "details": details})
                print(f"PASS {name}")
            except Exception as error:
                report["checks"].append({"name": name, "passed": False,
                                         "elapsed_seconds": round(time.perf_counter() - step_start, 4),
                                         "error": str(error), "snapshot": app.snapshot()})
                print(f"FAIL {name}: {error}")
                if app.process.poll() is not None:
                    break
        report["passed"] = len(report["checks"]) == 8 and all(check["passed"] for check in report["checks"])
    except Exception as error:
        report["error"] = str(error)
        report["traceback"] = traceback.format_exc()
        print(f"FAIL startup: {error}")
    finally:
        if app is not None:
            app.cleanup()
        report["elapsed_seconds"] = round(time.perf_counter() - started, 4)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    passed = sum(check["passed"] for check in report["checks"])
    print(f"{args.configuration}: {passed}/8 UI checks passed; report: {output}")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
