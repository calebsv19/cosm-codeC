"""Native code signing and sidecars for codeC's phase-owned release bundle."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

def sign(app, identity):
    if not identity or identity == "-":
        raise ValueError("Developer ID identity required")
    magics = {bytes.fromhex(v) for v in ("feedface", "cefaedfe", "feedfacf", "cffaedfe", "cafebabe", "bebafeca", "cafebabf", "bfbafeca")}
    for path in sorted(app.rglob("*"), key=lambda p: len(p.parts), reverse=True):
        if path.is_file() and not path.is_symlink():
            with path.open("rb") as stream:
                native = stream.read(4) in magics
            if native:
                subprocess.run(["codesign", "--force", "--timestamp", "--options", "runtime", "--sign", identity, str(path)], check=True)
    subprocess.run(["codesign", "--force", "--timestamp", "--options", "runtime", "--sign", identity, str(app)], check=True)


def manifest(source, root, signed):
    version = (source / "VERSION").read_text().strip()
    suffix = "" if signed else "-disposable"
    archive = root / f"codeC-{version}-macOS-arm64-stable{suffix}.zip"
    with archive.open("rb") as stream:
        checksum = hashlib.file_digest(stream, "sha256").hexdigest()
    Path(str(archive)+".sha256").write_text(f"{checksum}  {archive.name}\n")
    values = dict(product="codeC", program="ide", version=version, bundle_id="com.cosm.codec",
        platform="macOS", arch="arm64", format="zip", channel="stable", artifact=archive.name,
        zip=archive.name, sha256=checksum, signed=int(signed), notarized=int(signed))
    if not signed:
        head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
        tree = subprocess.check_output(["git", "rev-parse", "HEAD^{tree}"], cwd=source, text=True).strip()
        values.update(source_commit=head, source_tree=tree, disposable=1,
            release_signed=0, codesign_identity="ad-hoc", installed_app_replacement=0,
            app="codeC.app")
    if signed:
        notary = json.loads((root / "notary_submit.json").read_text())
        if not notary.get("id") or notary.get("status") != "Accepted":
            raise ValueError("retained Accepted submission required")
        values["notary_submission_id"] = notary["id"]
    archive.with_suffix(".manifest.txt").write_text("".join(f"{k}={v}\n" for k,v in values.items()))

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('phase', choices=('sign','manifest'))
    p.add_argument('--root', required=True)
    p.add_argument('--identity', default='')
    args=p.parse_args()
    source=Path(__file__).resolve().parents[2]
    root=Path(args.root).resolve()
    if args.phase=='sign':sign(root/'codeC.app',args.identity)
    else:manifest(source,root,True)
