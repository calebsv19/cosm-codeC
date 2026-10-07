#!/usr/bin/env python3
"""Build and verify the IDE's isolated, local-only Main Edit app."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import hashlib
import shutil


def run(*args: str, **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs)


def paths(source: Path, output_root: Path | None = None) -> tuple[Path, Path, Path]:
    dist = output_root or source / "build" / "main-edit-dist"
    return dist, dist / "codeCMainEdit.app", dist / "codeC Main Edit.app"


def helper(source: Path) -> Path:
    return source.parents[1] / "shared" / "scripts" / "mew1" / "mew1.py"


def fingerprint(source: Path) -> str:
    return run("python3", str(helper(source)), "fingerprint", "--repo", str(source)).stdout.strip()


def verify(source: Path, output_root: Path | None = None) -> Path:
    _, _, app = paths(source, output_root)
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
    icon = contents / "Resources" / "AppIcon.icns"
    expected_icon = source / "tools/packaging/macos/local_app_icon/AppIcon.icns"
    if not icon.is_file() or icon.read_bytes() != expected_icon.read_bytes():
        raise ValueError("Main Edit icon is missing or differs from source")
    if run("/usr/libexec/PlistBuddy", "-c", "Print :CFBundleIconFile", str(plist)).stdout.strip() != "AppIcon":
        raise ValueError("Main Edit icon metadata mismatch")
    run("codesign", "--verify", "--deep", "--strict", str(app))
    with tempfile.TemporaryDirectory(prefix="ide-main-edit-self-test-") as temporary:
        test_home = Path(temporary) / "home"
        test_home.mkdir()
        env = os.environ.copy()
        env["HOME"] = str(test_home)
        env.pop("IDE_LOG_DIR", None)
        env.pop("IDE_RUNTIME_DIR", None)
        env.pop("XDG_CACHE_HOME", None)
        env.pop("IDE_DEFAULT_WORKSPACE", None)
        launcher = contents / "MacOS" / "ide-launcher"
        output = run(str(launcher), "--self-test", env=env).stdout
        config = run(str(launcher), "--print-config", env=env).stdout
        if f"IDE_DEFAULT_WORKSPACE={test_home}/Desktop/CodeWork\n" not in config:
            raise ValueError("Main Edit default workspace used the isolated preference home")
        env["IDE_DEFAULT_WORKSPACE"] = str(test_home / "lesson")
        config = run(str(launcher), "--print-config", env=env).stdout
        if f"IDE_DEFAULT_WORKSPACE={test_home}/lesson\n" not in config:
            raise ValueError("Main Edit default workspace override was lost")
        profile_root = test_home / "Library" / "Application Support" / "codeC Main Edit"
        if "self-test: ok" not in output or "IDE_PROFILE=main-edit" not in output:
            raise ValueError("Main Edit launcher self-test failed")
        if f"IDE_RUNTIME_DIR={profile_root.resolve()}/runtime" not in output or f"LOG_FILE={profile_root}/logs/launcher.log" not in output:
            raise ValueError("Main Edit runtime/log namespace mismatch")
        if (test_home / ".custom_c_ide").exists() or (test_home / ".cache" / "caleb_ide").exists():
            raise ValueError("Main Edit touched canonical preference/cache names")
    return app


def snapshot_shared(source: Path, build_root: Path) -> Path:
    """Nested SDK makes own build/ caches: borrow source into the contained root."""
    shared = build_root / "shared-source"
    shared.mkdir(parents=True, exist_ok=False)
    prefix = "third_party/codework_shared/"
    names = run("git", "-C", str(source), "ls-files", "-z", "--", prefix).stdout.split("\0")
    manifest = {}
    for name in names:
        if not name:
            continue
        relative = Path(name.removeprefix(prefix))
        if not name.startswith(prefix) or relative.is_absolute() or ".." in relative.parts:
            raise ValueError("invalid shared snapshot path")
        original = source / name
        if not original.is_file() or original.is_symlink():
            raise ValueError("shared snapshot requires regular tracked source files")
        target = shared / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)
        value = hashlib.sha256(original.read_bytes()).hexdigest()
        if hashlib.sha256(target.read_bytes()).hexdigest() != value:
            raise ValueError("shared source changed during snapshot")
        manifest[str(relative)] = value
    if not manifest:
        raise ValueError("shared snapshot is empty")
    (build_root / "shared-source-inputs.json").write_text(json.dumps(manifest, indent=2))
    return shared


def build(source: Path, compiler_root: Path, output_root: Path | None = None, build_root: Path | None = None) -> Path:
    if run("git", "-C", str(source), "branch", "--show-current").stdout.strip() != "codex/ide-main-edit":
        raise ValueError("Main Edit package requires registered branch")
    dist, temporary_app, final_app = paths(source, output_root)
    if dist.is_symlink() or temporary_app.is_symlink() or final_app.is_symlink():
        raise ValueError("Main Edit dist path is a symlink; preserve and inspect it")
    if final_app.exists():
        verified = verify(source, output_root)
        print(json.dumps({"status": "existing_verified", "profile": "main-edit", "app": str(verified)}))
        return verified
    if temporary_app.exists():
        raise ValueError("unfinished package destination is occupied; retain it and use a fresh output root")
    if output_root is not None and build_root is None:
        raise ValueError("contained package output requires an explicit fresh build root")
    if build_root is not None and (build_root.exists() or build_root.is_symlink()):
        raise ValueError("contained package build root must be fresh")
    before = fingerprint(source)
    version = (source / "VERSION").read_text().strip()
    arguments = ["make", f"FISICS_DIR={compiler_root}", f"DIST_DIR={dist}",
                 "PACKAGE_APP_NAME=codeCMainEdit.app",
                 "PACKAGE_INFO_PLIST_SRC=tools/packaging/macos/MainEdit-Info.plist",
                 "PACKAGE_LAUNCHER_SRC=tools/packaging/macos/main-edit-launcher",
                 "PACKAGE_LAUNCHER_BASE_SRC=tools/packaging/macos/ide-launcher",
                 "PACKAGE_MAIN_EDIT_IDENTITY=1", "package-desktop"]
    if build_root is not None:
        shared_snapshot = snapshot_shared(source, build_root)
        arguments.insert(1, f"SHARED_ROOT={shared_snapshot}")
        arguments.insert(1, f"TARGET_BUILD_ROOT={build_root}")
    subprocess.run(arguments, cwd=source, check=True)
    if fingerprint(source) != before:
        raise ValueError("source changed during Main Edit package build")
    if not temporary_app.is_dir():
        raise ValueError("Main Edit package missing after build")
    temporary_app.rename(final_app)
    print(json.dumps({"status": "built", "profile": "main-edit", "app": str(final_app),
                      "source_fingerprint_sha256": before, "version": version}))
    return final_app


def bundle_manifest(app: Path) -> dict[str, str]:
    result = {}
    for path in sorted(app.rglob("*")):
        name = str(path.relative_to(app))
        if path.is_symlink():
            result[name] = "link:" + os.readlink(path)
        elif path.is_file():
            result[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def assert_not_running(destination: Path) -> None:
    processes = run("ps", "-axo", "command=").stdout.splitlines()
    prefix = str(destination / "Contents" / "MacOS") + "/"
    if any(line.strip().startswith(prefix) for line in processes):
        raise ValueError("Desktop Main Edit app is running; close it normally before replacement")


def refresh(source: Path, output_root: Path | None = None, backup_root: Path | None = None) -> None:
    app = verify(source, output_root)
    destination = Path.home() / "Desktop" / "codeC Main Edit.app"
    if destination.is_symlink():
        raise ValueError("Desktop Main Edit destination is a symlink")
    occupied = destination.exists()
    if occupied and backup_root is None:
        raise ValueError("Desktop Main Edit app exists; supply a fresh retained backup root for authorized replacement")
    assert_not_running(destination)
    if occupied:
        plist = destination / "Contents" / "Info.plist"
        if run("/usr/libexec/PlistBuddy", "-c", "Print :CFBundleIdentifier", str(plist)).stdout.strip() != "com.cosm.codec.mainedit":
            raise ValueError("occupied destination is not the Main Edit app")
    before = bundle_manifest(destination) if occupied else {}
    if backup_root is not None:
        if backup_root.is_symlink() or any(parent.is_symlink() for parent in backup_root.parents):
            raise ValueError("backup root must not traverse symlinks")
        backup_root.mkdir(parents=True, exist_ok=False)
    # Stage beside the destination, so installing the verified bundle is an atomic rename.
    stage_parent = Path(tempfile.mkdtemp(prefix=".codec-main-edit-stage-", dir=destination.parent))
    staged = stage_parent / destination.name
    run("/usr/bin/ditto", str(app), str(staged))
    run("codesign", "--verify", "--deep", "--strict", str(staged))
    expected = bundle_manifest(app)
    if bundle_manifest(staged) != expected:
        raise ValueError("staged package byte mismatch; retained for inspection")
    assert_not_running(destination)
    if occupied and bundle_manifest(destination) != before:
        raise ValueError("Desktop bundle changed during staging")
    backup = backup_root / destination.name if occupied else None
    if backup is not None:
        destination.rename(backup)
    try:
        staged.rename(destination)
        run("codesign", "--verify", "--deep", "--strict", str(destination))
        if bundle_manifest(destination) != expected:
            raise ValueError("installed package byte mismatch")
    except Exception:
        if destination.exists():
            destination.rename(stage_parent / "failed-installed.app")
        if backup is not None:
            backup.rename(destination)
        raise
    stage_parent.rmdir()
    receipt = {"status": "refreshed", "app": str(destination), "backup": str(backup) if backup else None,
               "installed_manifest": expected, "previous_manifest": before}
    if backup_root is not None:
        (backup_root / "refresh-receipt.json").write_text(json.dumps(receipt, indent=2))
    print(json.dumps({key: value for key, value in receipt.items() if not key.endswith("manifest")}))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("build", "verify", "refresh"))
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--compiler-root", type=Path)
    parser.add_argument("--output-root", type=Path, help="fresh retained package directory")
    parser.add_argument("--build-root", type=Path, help="fresh contained compiler/app build directory")
    parser.add_argument("--backup-root", type=Path, help="fresh retained backup required to replace an occupied Desktop Main Edit")
    args = parser.parse_args()
    for path in (args.output_root, args.build_root, args.backup_root):
        if path is not None and (not path.is_absolute() or ".." in path.parts or path.is_symlink()):
            parser.error("package, build and backup roots must be absolute non-symlink paths")
    source = args.source_root.resolve(strict=True)
    if args.command == "build":
        if args.compiler_root is None:
            parser.error("build requires --compiler-root")
        build(source, args.compiler_root.resolve(strict=True), args.output_root, args.build_root)
    elif args.command == "verify":
        print(json.dumps({"status": "verified", "app": str(verify(source, args.output_root))}))
    else:
        refresh(source, args.output_root, args.backup_root)


if __name__ == "__main__":
    main()
