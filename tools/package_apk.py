#!/usr/bin/env python3
"""Repack the Rec Room base APK with the github/LAN patch.

The patch library has to be loaded before libil2cpp.so exists. ``libunity.so``
already depends on ``libmain.so``, so the only edit to the original APK is:

  * replace ``lib/arm64-v8a/libmain.so`` with a copy that carries an extra
    ``DT_NEEDED librecroom_patch.so`` (made with LIEF, see README), and
  * add ``lib/arm64-v8a/librecroom_patch.so``.

Optionally ``--dex`` swaps the top-level ``classes.dex`` for a patched copy
(the fake Google Play sign-in used by the Android build).

Everything else is copied byte-for-byte. The result is unsigned: run
``zipalign -p -f 4`` then ``apksigner sign`` on it.

    python tools/package_apk.py \
        --apk rec-room-20230427-base.apk \
        --libmain libmain.patched.so \
        --patch-so prebuilt/arm64-v8a/librecroom_patch.so \
        --dex classes.patched.dex \
        --out recroom-github-unsigned.apk
"""

from __future__ import annotations

import argparse
import os
import sys
import zipfile

LIB_DIR = "lib/arm64-v8a"
MAIN_SO = f"{LIB_DIR}/libmain.so"
PATCH_SO = f"{LIB_DIR}/librecroom_patch.so"
DEX = "classes.dex"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", required=True, help="original base APK")
    parser.add_argument("--libmain", required=True, help="LIEF-patched libmain.so")
    parser.add_argument("--patch-so", required=True, help="built librecroom_patch.so")
    parser.add_argument("--dex", help="optional patched classes.dex")
    parser.add_argument("--out", required=True, help="unsigned output APK")
    args = parser.parse_args()

    replacements = {MAIN_SO: open(args.libmain, "rb").read()}
    if args.dex:
        replacements[DEX] = open(args.dex, "rb").read()
    additions = {PATCH_SO: open(args.patch_so, "rb").read()}

    with zipfile.ZipFile(args.apk) as zin, zipfile.ZipFile(args.out, "w", zipfile.ZIP_DEFLATED) as zout:
        seen = set()
        for item in zin.infolist():
            seen.add(item.filename)
            data = replacements.get(item.filename)
            if data is None:
                data = zin.read(item.filename)
            info = zipfile.ZipInfo(item.filename, date_time=item.date_time)
            info.compress_type = item.compress_type
            info.external_attr = item.external_attr
            info.internal_attr = item.internal_attr
            info.create_system = item.create_system
            zout.writestr(info, data)

        for name, data in additions.items():
            if name in seen:
                sys.exit(f"{name} already present in {args.apk}")
            info = zipfile.ZipInfo(name, date_time=(2026, 11, 17, 12, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            zout.writestr(info, data)

    print(f"wrote {args.out} ({os.path.getsize(args.out) / (1024 * 1024):.1f} MB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
