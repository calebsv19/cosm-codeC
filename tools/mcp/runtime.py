"""Bounded transport from local MCP tools to the existing idebridge executable."""

import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import stat
from example_run import run_example


class Runtime:
    def __init__(self, bridge: Path, project_root: Path, session_file: Path | None = None):
        self.bridge = bridge.resolve(strict=True)
        self.project_root = project_root.resolve(strict=True)
        if not self.bridge.is_file() or not self.project_root.is_dir():
            raise ValueError("bridge_and_project_root_required")
        self.session_file = session_file.expanduser().absolute() if session_file else None
        self._bound_session_id = None
        self._last_example = None
        self._last_build = None

    def _session(self) -> dict | None:
        if self.session_file is None:
            return None
        info = self.session_file.lstat()
        if (not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid() or
                stat.S_IMODE(info.st_mode) & 0o077 or info.st_size > 2048):
            raise ValueError("insecure_ide_session_file")
        session = json.loads(self.session_file.read_text())
        if (session.get("schema") != "ide_agent_session_v1" or
                Path(session.get("project_root", "")).resolve() != self.project_root or
                not session.get("socket_path") or not session.get("auth_token") or
                not session.get("session_id")):
            raise ValueError("ide_session_file_mismatch")
        if self._bound_session_id and session["session_id"] != self._bound_session_id:
            raise ValueError("ide_session_changed_restart_mcp_client")
        return session

    def call(self, *args: str, timeout: int = 30) -> dict:
        if not args:
            raise ValueError("bridge_command_required")
        # idebridge dispatches on argv[1]; its global flags are parsed after
        # the subcommand despite their name.
        command = [str(self.bridge), str(args[0]), "--json", "--timeout_ms",
                   str(timeout * 1000), *map(str, args[1:])]
        env = os.environ.copy()
        env["MYIDE_PROJECT_ROOT"] = str(self.project_root)
        session = self._session()
        if session:
            env["MYIDE_SOCKET"] = session["socket_path"]
            env["MYIDE_AUTH_TOKEN"] = session["auth_token"]
        result = subprocess.run(command, capture_output=True, timeout=timeout + 2,
                                env=env, check=False)
        if len(result.stdout) > 1024 * 1024 or len(result.stderr) > 16384:
            raise ValueError("bridge_response_too_large")
        try:
            response = json.loads(result.stdout)
        except (UnicodeError, ValueError):
            raise ValueError("bridge_transport_failed: " +
                             result.stderr.decode(errors="replace")[:400]) from None
        if result.returncode or not response.get("ok"):
            raise ValueError(json.dumps(response.get("error", response))[:1200])
        return response["result"]

    def live(self) -> dict:
        session = self._session()
        result = self.call("ping")
        if Path(result.get("project_root", "")).resolve() != self.project_root:
            raise ValueError("connected_ide_workspace_mismatch")
        if session and result.get("session_id") != session["session_id"]:
            raise ValueError("connected_ide_session_mismatch")
        if session:
            self._bound_session_id = session["session_id"]
        return result

    def source(self, relative: str) -> Path:
        path = (self.project_root / relative).resolve(strict=True)
        if not path.is_relative_to(self.project_root) or not path.is_file():
            raise ValueError("source_must_be_existing_workspace_file")
        return path

    def read_document(self, relative: str) -> dict:
        path = self.source(relative)
        data = path.read_bytes()
        if len(data) > 32768 or b"\0" in data:
            raise ValueError("source_exceeds_text_read_limit")
        return {"path": relative, "sha256": hashlib.sha256(data).hexdigest(),
                "text": data.decode("utf-8"), "bytes": len(data),
                "authority": "saved_file_only_unsaved_ide_buffer_not_included"}

    def open_document(self, relative: str, line: int, column: int) -> dict:
        if not 1 <= line <= 1000000 or not 1 <= column <= 1000000:
            raise ValueError("invalid_location")
        self.source(relative)
        self.live()
        return self.call("open", f"{relative}:{line}:{column}")

    def apply_diff(self, relative: str, expected_sha256: str, diff: str) -> dict:
        if not 1 <= len(diff) <= 65536:
            raise ValueError("diff_size_out_of_range")
        targets = [line[4:].removeprefix("b/").split("\t", 1)[0]
                   for line in diff.splitlines() if line.startswith("+++ ")]
        if targets != [relative]:
            raise ValueError("exact_single_file_diff_required")
        source = self.source(relative)
        before = source.read_bytes()
        if hashlib.sha256(before).hexdigest() != expected_sha256:
            raise ValueError("source_sha256_changed")
        # idebridge and the IDE use this legacy FNV-1a value at request and
        # main-thread apply time. Bind it to the same bytes reviewed above.
        fnv64 = 1469598103934665603
        for byte in before:
            fnv64 = ((fnv64 ^ byte) * 1099511628211) & 0xffffffffffffffff
        self.live()
        with tempfile.TemporaryDirectory(prefix="ide-mcp-edit-") as temporary:
            patch = Path(temporary) / "edit.diff"
            patch.write_text(diff, encoding="utf-8")
            result = self.call("edit", "--apply", str(patch),
                               "--expected_fnv64", f"{fnv64:016x}")
        after = self.read_document(relative)
        return {"bridge": result, "saved": after}

    @staticmethod
    def _fnv64(data: bytes) -> str:
        value = 1469598103934665603
        for byte in data:
            value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
        return f"{value:016x}"

    def type_visible(self, relative: str, expected_sha256: str,
                     expected_text: str, replacement_text: str,
                     cadence_ms: int = 45) -> dict:
        """Replace one unique checked ASCII range through visible editor typing."""
        if not 16 <= cadence_ms <= 500:
            raise ValueError("typing_cadence_out_of_range")
        for label, value, allow_empty in (("expected", expected_text, True),
                                           ("replacement", replacement_text, False)):
            encoded = value.encode("utf-8")
            if ((not allow_empty and not encoded) or len(encoded) > 4096 or
                    any(byte > 127 or (byte < 32 and byte not in (9, 10)) or byte == 127
                        for byte in encoded)):
                raise ValueError(f"{label}_typing_text_must_be_bounded_ascii")
        source = self.source(relative)
        before = source.read_bytes()
        if hashlib.sha256(before).hexdigest() != expected_sha256:
            raise ValueError("source_sha256_changed")
        needle = expected_text.encode("ascii")
        if not needle:
            raise ValueError("expected_typing_text_must_identify_a_range")
        if before.count(needle) != 1:
            raise ValueError("expected_typing_text_must_be_unique")
        start = before.index(needle)
        replacement = replacement_text.encode("ascii")
        after = before[:start] + replacement + before[start + len(needle):]

        def location(offset: int) -> tuple[int, int]:
            prefix = before[:offset]
            return prefix.count(b"\n") + 1, len(prefix.rsplit(b"\n", 1)[-1]) + 1

        start_line, start_column = location(start)
        end_line, end_column = location(start + len(needle))
        self.live()
        timeout = min(620, max(45, (len(replacement) * cadence_ms) // 1000 + 45))
        result = self.call(
            "type", "--path", relative,
            "--expected_before_fnv64", self._fnv64(before),
            "--expected_after_fnv64", self._fnv64(after),
            "--expected_text", expected_text,
            "--replacement_text", replacement_text,
            "--start_line", str(start_line),
            "--start_column", str(start_column),
            "--end_line", str(end_line),
            "--end_column", str(end_column),
            "--cadence_ms", str(cadence_ms), timeout=timeout)
        saved = self.read_document(relative)
        if saved["sha256"] != hashlib.sha256(after).hexdigest():
            raise ValueError("visible_typing_saved_readback_mismatch")
        return {"bridge": result, "saved": saved,
                "expected_intermediate_states": result.get("mutation_steps", 0),
                "intermediate_frames_captured": False}

    def diagnostics(self, maximum: int = 40) -> dict:
        if not 1 <= maximum <= 100:
            raise ValueError("invalid_diagnostic_limit")
        self.live()
        return self.call("diag", "--max", str(maximum))

    def build(self, profile: str = "debug") -> dict:
        if profile not in ("debug", "perf"):
            raise ValueError("invalid_build_profile")
        session = self.live()
        self._last_build = None
        result = self.call("build", "--profile", profile, timeout=120)
        if self.live()["session_id"] != session["session_id"]:
            raise ValueError("ide_session_changed_during_build")
        output = result.get("output")
        if not isinstance(output, str) or len(output.encode("utf-8")) > 32768:
            raise ValueError("invalid_build_output")
        result["output_sha256"] = hashlib.sha256(output.encode("utf-8")).hexdigest()
        self._last_build = (session["session_id"], result)
        return result

    def build_present(self, expected_output_sha256: str, focus: str = "") -> dict:
        """Show only the last build result from this MCP and IDE session."""
        session = self.live()
        if (self._last_build is None or
                self._last_build[0] != session["session_id"] or
                self._last_build[1]["output_sha256"] != expected_output_sha256):
            raise ValueError("no_matching_build_in_this_mcp_session")
        result = self._last_build[1]
        output = result["output"]
        if len(focus) > 128 or any(ord(c) < 32 or ord(c) == 127 for c in focus):
            raise ValueError("invalid_build_focus")
        lines = output.splitlines()
        matching = next((i for i, line in enumerate(lines) if focus in line), None) if focus else 0
        if matching is None:
            raise ValueError("build_focus_absent_from_output")
        selected = lines[matching:matching + 3]
        safe_output = "".join(c if c == "\n" or c == "\t" or ord(c) >= 32 and ord(c) != 127
                              else f"\\x{ord(c):02x}" for c in "\n".join(selected) + "\n")
        state = "passed" if result.get("ok") else "failed"
        header = (f"[Agent build: {state}, selected output sha256={expected_output_sha256[:12]}]\n")
        available = 4096 - len(header.encode("utf-8")) - 40
        encoded = safe_output.encode("utf-8")
        clipped = encoded[:available].decode("utf-8", errors="ignore")
        truncated = len(encoded) > available or matching > 0 or matching + len(selected) < len(lines)
        text = header + clipped + ("[more in build result]\n" if truncated else "")
        present = self.call("present", "--text", text)
        if self.live()["session_id"] != session["session_id"]:
            raise ValueError("ide_session_changed_during_build_presentation")
        return {"presented": present.get("presented") is True,
                "ide_session": session["session_id"],
                "output_sha256": expected_output_sha256,
                "output_truncated_in_ide": truncated,
                "focused_on": focus or None,
                "terminal": "Run", "visible_content_reviewed": False}

    def example_run(self, expected_executable_sha256: str,
                    timeout_seconds: int = 3) -> dict:
        """Run only build/app from the connected workspace, with no arguments."""
        session = self.live()
        result = run_example(self.project_root, expected_executable_sha256,
                             timeout_seconds)
        if self.live()["session_id"] != session["session_id"]:
            raise ValueError("ide_session_changed_during_example_run")
        self._last_example = (session["session_id"], result)
        return result

    def example_present(self, expected_executable_sha256: str) -> dict:
        """Show the last verified result in the connected IDE's Run terminal."""
        session = self.live()
        if (self._last_example is None or
                self._last_example[0] != session["session_id"] or
                self._last_example[1]["executable_sha256"] != expected_executable_sha256):
            raise ValueError("no_matching_example_run_in_this_mcp_session")
        result = self._last_example[1]
        if result["status"] != "completed":
            raise ValueError("only_completed_example_runs_can_be_presented")
        output = result["output"]
        safe_output = "".join(c if c == "\n" or c == "\t" or ord(c) >= 32 and ord(c) != 127
                              else f"\\x{ord(c):02x}" for c in output)
        header = (f"[Agent example: build/app sha256={expected_executable_sha256[:12]} "
                  f"exit={result['exit_code']}]\n")
        available = 4096 - len(header.encode("utf-8")) - 40
        encoded = safe_output.encode("utf-8")
        clipped = encoded[:available].decode("utf-8", errors="ignore")
        truncated = len(encoded) > available
        text = header + clipped + ("\n[output truncated for IDE view]\n" if truncated else "")
        present = self.call("present", "--text", text)
        if self.live()["session_id"] != session["session_id"]:
            raise ValueError("ide_session_changed_during_presentation")
        return {"presented": present.get("presented") is True,
                "ide_session": session["session_id"],
                "executable_sha256": expected_executable_sha256,
                "output_truncated_in_ide": truncated,
                "terminal": "Run", "visible_content_reviewed": False}
