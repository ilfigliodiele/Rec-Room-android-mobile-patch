# Rec Room 2023 Android Patch

> ⚠️ **Work in Progress** — This patch is currently in development.

A native patch for Rec Room Android (build `20230427` / versionCode `22397318`) that redirects the client to a custom private server, bypassing the official Against Gravity servers.

## Status

- [ ] Identify target functions in `libil2cpp.so`
- [ ] Calculate RVA offsets for build `22397318`
- [ ] Write native hook in C/C++
- [ ] Compile with Android NDK (arm64-v8a)
- [ ] Test on device

## Requirements

### To build the patch
- Android NDK
- Ghidra (for reverse engineering)
- The original `libil2cpp.so` from build `20230427`

### To apply the patch
- [ZArchiver](https://play.google.com/store/apps/details?id=ru.zdevs.zarchiver) or MT Manager
- [uber-apk-signer](https://github.com/patrickfav/uber-apk-signer) or apksigner
- ADB
- Rec Room APK build `20230427` (versionCode `22397318`)
- Il2cpp dumper *for the dump

## How to apply

1. Extract the APK contents (rename `.apk` to `.zip` or use ZArchiver)
2. Replace `lib/arm64-v8a/libil2cpp.so` with the patched version
3. Repack the APK
4. Sign with your own keystore:
   ```bash
   apksigner sign --ks my-key.jks --out RecRoom-patched.apk RecRoom-unsigned.apk
   ```
5. Install via ADB:
   ```bash
   adb install RecRoom-patched.apk
   ```
6. Copy the OBB folder to `Android/obb/com.AgainstGravity.RecRoom/` on your device

## Target Build

| Field | Value |
|---|---|
| versionName | `20230427` |
| versionCode | `22397318` |
| Architecture | `arm64-v8a` |
| Min Android | 7.1 |

## Related Projects

- [RecFlare](https://github.com/djdevin/recflare) — Original server implementation (Quest patch reference)

## Why Theres Only Android Apps?

Because i have this idea when im on the car and i dont have pc in car i dont want to wait to get home so ive done this on android :) anyways the patch can be used with pc with adb commands, sorry 🥲