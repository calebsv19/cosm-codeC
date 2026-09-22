#!/usr/bin/env python3
"""Build and verify the IDE's isolated, local-only Main Edit app."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def run(*args: str, **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs)


def paths(source: Path) -> tuple[Path, Path, Path]:
    dist = source / "build" / "main-edit-dist"
    return dist, dist / "codeCMainEdit.app", dist / "codeC Main Edit.app"


def helper(source: Path) -> Path:
    return source.parents[1] / "shared" / "scripts" / "mew1" / "mew1.py"


def fingerprint(source: Path) -> str:
    return run("python3", str(helper(source)), "fingerprint", "--repo", str(source)).stdout.strip()


def verify(source: Path) -> Path:
    _, _, app = paths(source)
    if not app.is_dir() or app.is_symlink():
        raise ValueError("Main Edit app is missing or is a symlink")
    contents = app / "Contents"
    binary = contents / "MacOS" / "ide-bin"
    identity = contents / "Resources" / "development_identity.json"
    plist = contents / "Info.plist"
    if run("/usr/libexec/PlistBuddy", "-c", "Print :CFBundleIdentifier", str(plist)).stdout.strip() != "com.cosm.codec.mainedit":
        raise ValueError("Main Edit bundle ID mismatch")
    if run("/usr/libexec/PlistBuddy", "-c", "Print :CFBundleDisplayName", str(plist)).stdout.strip() != "codeC Main Edit":
        raise ValueError("Main Edit display name mismatch")
    version = (source / "VERSION").read_text().strip()
    run("python3", str(helper(source)), "verify-identity", "--identity", str(identity),
        "--source-root", str(source), "--binary", str(binary), "--profile", "main-edit",
        "--program", "ide", "--product", "codeC", "--version", version)
    run("codesign", "--verify", "--deep", "--strict", str(app))
    with tempfile.TemporaryDirectory(prefix="ide-main-edit-self-test-") as temporary:
        test_home = Path(temporary) / "home"
        test_home.mkdir()
        env = os.environ.copy()
        env["HOME"] = str(test_home)
        env.pop("IDE_LOG_DIR", None)
        env.pop("IDE_RUNTIME_DIR", None)
        env.pop("XDG_CACHE_HOME", None)
        launcher = contents / "MacOS" / "ide-launcher"
        output = run(str(launcher), "--self-test", env=env).stdout
        profile_root = test_home / "Library" / "Application Support" / "codeC Main Edit"
        if "self-test: ok" not in output or "IDE_PROFILE=main-edit" not in output:
            raise ValueError("Main Edit launcher self-test failed")
        if f"IDE_RUNTIME_DIR={profile_root.resolve()}/runtime" not in output or f"LOG_FILE={profile_root}/logs/launcher.log" not in output:
            raise ValueError("Main Edit runtime/log namespace mismatch")
        if (test_home / ".custom_c_ide").exists() or (test_home / ".cache" / "caleb_ide").exists():
            raise ValueError("Main Edit touched canonical preference/cache names")
    return app


def build(source: Path, compiler_root: Path) -> Path:
    if run("git", "-C", str(source), "branch", "--show-current").stdout.strip() != "codex/ide-main-edit":
        raise ValueError("Main Edit package requires registered branch")
    dist, temporary_app, final_app = paths(source)
    if dist.is_symlink() or temporary_app.is_symlink() or final_app.is_symlink():
        raise ValueError("Main Edit dist path is a symlink; preserve and inspect it")
    if final_app.exists():
        verified = verify(source)
        print(json.dumps({"status": "existing_verified", "profile": "main-edit", "app": str(verified)}))
        return verified
    before = fingerprint(source)
    version = (source / "VERSION").read_text().strip()
    arguments = ["make", f"FISICS_DIR={compiler_root}", f"DIST_DIR={dist}",
                 "PACKAGE_APP_NAME=codeCMainEdit.app",
                 "PACKAGE_INFO_PLIST_SRC=tools/packaging/macos/MainEdit-Info.plist",
                 "PACKAGE_LAUNCHER_SRC=tools/packaging/macos/main-edit-launcher",
                 "PACKAGE_LAUNCHER_BASE_SRC=tools/packaging/macos/ide-launcher",
                 "PACKAGE_MAIN_EDIT_IDENTITY=1", "package-desktop"]
    subprocess.run(arguments, cwd=source, check=True)
    if fingerprint(source) != before:
        raise ValueError("source changed during Main Edit package build")
    if not temporary_app.is_dir():
        raise ValueError("Main Edit package missing after build")
    temporary_app.rename(final_app)
    print(json.dumps({"status": "built", "profile": "main-edit", "app": str(final_app),
                      "source_fingerprint_sha256": before, "version": version}))
    return final_app


def refresh(source: Path) -> None:
    app = verify(source)
    destination = Path.home() / "Desktop" / "codeC Main Edit.app"
    if destination.exists() or destination.is_symlink():
        raise ValueError("Desktop Main Edit app already exists; inspect it before replacement")
    destination.parent.mkdir(parents=True, exist_ok=True)
    run("/usr/bin/ditto", str(app), str(destination))
    run("codesign", "--verify", "--deep", "--strict", str(destination))
    print(json.dumps({"status": "refreshed", "app": str(destination)}))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("build", "verify", "refresh"))
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--compiler-root", type=Path)
    args = parser.parse_args()
    source = args.source_root.resolve(strict=True)
    if args.command == "build":
        if args.compiler_root is None:
            parser.error("build requires --compiler-root")
        build(source, args.compiler_root.resolve(strict=True))
    elif args.command == "verify":
        print(json.dumps({"status": "verified", "app": str(verify(source))}))
    else:
        refresh(source)


if __name__ == "__main__":
    main()
