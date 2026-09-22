"""Run the one conventional lesson executable with bounded local output."""

import hashlib
import os
from pathlib import Path
import select
import signal
import stat
import subprocess
import tempfile
import time


MAX_OUTPUT = 16 * 1024


def run_example(project_root: Path, expected_sha256: str, timeout_seconds: int = 3) -> dict:
    project_root = project_root.resolve(strict=True)
    if (len(expected_sha256) != 64 or
            any(c not in "0123456789abcdef" for c in expected_sha256)):
        raise ValueError("invalid_executable_sha256")
    if not 1 <= timeout_seconds <= 10:
        raise ValueError("example_timeout_out_of_range")

    executable = project_root / "build" / "app"
    info = executable.lstat()
    if not stat.S_ISREG(info.st_mode) or not os.access(executable, os.X_OK):
        raise ValueError("example_must_be_regular_executable")
    if not 0 < info.st_size <= 64 * 1024 * 1024:
        raise ValueError("example_executable_size_out_of_range")
    if not executable.resolve(strict=True).is_relative_to(project_root):
        raise ValueError("example_outside_workspace")
    digest = hashlib.sha256()
    with executable.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    actual_sha256 = digest.hexdigest()
    if actual_sha256 != expected_sha256:
        raise ValueError("example_executable_changed")

    with tempfile.TemporaryDirectory(prefix="codec-example-") as private_home:
        env = {"PATH": "/usr/bin:/bin", "HOME": private_home,
               "TMPDIR": private_home, "LANG": "C", "LC_ALL": "C"}
        started = time.monotonic()
        process = subprocess.Popen([str(executable)], cwd=project_root, env=env,
                                   stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, start_new_session=True,
                                   close_fds=True)
        output = bytearray()
        status = "completed"
        try:
            while True:
                remaining = timeout_seconds - (time.monotonic() - started)
                if remaining <= 0:
                    status = "timed_out"
                    break
                ready, _, _ = select.select([process.stdout], [], [], min(remaining, 0.1))
                if ready:
                    chunk = os.read(process.stdout.fileno(), MAX_OUTPUT + 1 - len(output))
                    if not chunk:
                        break
                    output.extend(chunk)
                    if len(output) > MAX_OUTPUT:
                        status = "output_limit_exceeded"
                        output = output[:MAX_OUTPUT]
                        break
                elif process.poll() is not None:
                    # A process may exit before select reports its last pipe bytes.
                    continue
        finally:
            # Reap the whole group, including children that inherited stdout.
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            except PermissionError:
                if process.poll() is None:
                    process.kill()
            process.wait(timeout=2)
            process.stdout.close()

    return {"schema": "ide_example_run_v1", "path": "build/app",
            "executable_sha256": actual_sha256, "status": status,
            "exit_code": process.returncode if status == "completed" else None,
            "output": output.decode("utf-8", errors="replace"),
            "output_bytes": len(output), "duration_ms": int((time.monotonic() - started) * 1000),
            "visible_in_ide": False}
