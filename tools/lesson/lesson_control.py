#!/usr/bin/env python3
"""Apply one source-bound lesson step through codeC and retain a capture handoff."""

import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "mcp"))
from runtime import Runtime


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_manifest(path: Path) -> dict:
    manifest = json.loads(path.read_text())
    if manifest.get("schema") != "ide_video_lesson_v1":
        raise ValueError("unsupported_lesson_schema")
    steps = manifest.get("steps")
    if not isinstance(steps, list) or not steps:
        raise ValueError("lesson_requires_steps")
    ids = set()
    for step in steps:
        if not isinstance(step, dict) or not isinstance(step.get("id"), str):
            raise ValueError("invalid_step")
        if step["id"] in ids:
            raise ValueError("duplicate_step_id")
        ids.add(step["id"])
        for field in ("path", "before_sha256", "after_sha256", "unified_diff",
                      "intent", "capture_seconds"):
            if field not in step:
                raise ValueError("step_missing_" + field)
        if not 1 <= step["capture_seconds"] <= 30:
            raise ValueError("capture_duration_out_of_range")
    return manifest


def step_prepare(manifest: dict, step_id: str, workspace: Path) -> dict:
    """Create an isolated before-state workspace; never replace an existing one."""
    step = next((value for value in manifest["steps"] if value["id"] == step_id), None)
    if step is None:
        raise ValueError("unknown_step")
    relative = Path(step["path"])
    if relative.is_absolute() or not relative.parts or any(part in (".", "..") for part in relative.parts):
        raise ValueError("unsafe_lesson_source_path")
    source_text = step.get("before_text")
    if not isinstance(source_text, str):
        raise ValueError("lesson_step_missing_before_text")
    source_bytes = source_text.encode("utf-8")
    if len(source_bytes) > 32768 or digest(source_bytes) != step["before_sha256"]:
        raise ValueError("lesson_before_text_hash_mismatch")
    workspace.mkdir()
    source = workspace / relative
    source.parent.mkdir(parents=True, exist_ok=True)
    with source.open("xb") as stream:
        stream.write(source_bytes)
    return {"schema": "ide_video_lesson_prepared_v1", "lesson_id": manifest.get("lesson_id"),
            "step_id": step_id, "workspace": str(workspace.resolve()),
            "path": step["path"], "before_sha256": digest(source.read_bytes())}


def step_apply(manifest: dict, step_id: str, bridge: Path,
               workspace: Path, receipt_dir: Path,
               session_file: Path | None = None) -> dict:
    step = next((value for value in manifest["steps"] if value["id"] == step_id), None)
    if step is None:
        raise ValueError("unknown_step")
    runtime = Runtime(bridge, workspace, session_file)
    receipt_dir.mkdir(parents=True, exist_ok=True)
    receipt_path = receipt_dir / (step_id + ".json")
    current = runtime.read_document(step["path"])
    if receipt_path.exists():
        receipt = json.loads(receipt_path.read_text())
        if (receipt.get("lesson_id") == manifest.get("lesson_id") and
                receipt.get("step_id") == step_id and
                receipt.get("after_sha256") == current["sha256"]):
            return receipt
        raise ValueError("existing_receipt_or_source_changed")
    if current["sha256"] == step["after_sha256"]:
        raise ValueError("source_already_after_without_receipt_inspect_before_retry")
    if current["sha256"] != step["before_sha256"]:
        raise ValueError("lesson_step_source_changed")
    edit = runtime.apply_diff(step["path"], step["before_sha256"],
                              step["unified_diff"])
    if edit["saved"]["sha256"] != step["after_sha256"]:
        raise ValueError("post_edit_source_does_not_match_lesson_step")
    position = step.get("present", {"line": 1, "column": 1})
    opened = runtime.open_document(step["path"], position["line"],
                                   position.get("column", 1))
    build = runtime.build(step.get("build_profile", "debug")) if step.get("build") else None
    receipt = {"schema": "ide_video_lesson_step_v1",
               "lesson_id": manifest.get("lesson_id"), "step_id": step_id,
               "path": step["path"], "before_sha256": step["before_sha256"],
               "after_sha256": edit["saved"]["sha256"],
               "ide_session": runtime.live().get("session_id"),
               "document_revision_after": edit["bridge"].get("document_revision_after"),
               "open_result": opened, "build_result": build,
               "capture_handoff": {"shot_id": step_id, "intent": step["intent"],
                                   "seconds": step["capture_seconds"],
                                   "target": "select_exact_ide_window_at_record_time"},
               "visible_content_reviewed": False, "capture_status": "unrecorded"}
    with receipt_path.open("x") as stream:
        stream.write(json.dumps(receipt, indent=2) + "\n")
    return receipt


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("plan", "prepare", "apply"))
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--step")
    parser.add_argument("--idebridge", type=Path)
    parser.add_argument("--project-root", type=Path)
    parser.add_argument("--receipts", type=Path)
    parser.add_argument("--session-file", type=Path)
    args = parser.parse_args()
    manifest = load_manifest(args.manifest)
    if args.command == "plan":
        result = {"schema": manifest["schema"], "lesson_id": manifest.get("lesson_id"),
                  "steps": [{"id": step["id"], "path": step["path"],
                             "intent": step["intent"],
                             "capture_seconds": step["capture_seconds"]}
                            for step in manifest["steps"]]}
    elif args.command == "prepare":
        if not all((args.step, args.project_root)):
            parser.error("prepare requires --step --project-root")
        result = step_prepare(manifest, args.step, args.project_root)
    else:
        if not all((args.step, args.idebridge, args.project_root, args.receipts)):
            parser.error("apply requires --step --idebridge --project-root --receipts")
        result = step_apply(manifest, args.step, args.idebridge,
                            args.project_root, args.receipts, args.session_file)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
