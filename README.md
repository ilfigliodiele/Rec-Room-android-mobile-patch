# Rec Room 2023 Android Patch

> ⚠️ **Work in Progress** — Patch compiled, testing in progress.

A native patch for Rec Room Android (build `20230427` / versionCode `22397318`) that redirects the client to a custom private server at `127.0.0.1:8805`, bypassing the official Against Gravity servers.

## Status

- [x] Identify target functions in `libil2cpp.so`
- [x] Calculate RVA offsets for build `22397318` → `0x4F19B58`
- [x] Write native hook in C/C++
- [ ] Compile with Android NDK (arm64-v8a)
- [ ] Test on device

## Requirements

### To build the patch
- Android NDK r25c
- CMake 3.22+
- [Dobby](https://github.com/jmpews/Dobby) (hooking library)
- The original `libil2cpp.so` from build `20230427`

### To apply the patch
- [ZArchiver](https://play.google.com/store/apps/details?id=ru.zdevs.zarchiver) or MT Manager
- [uber-apk-signer](https://github.com/patrickfav/uber-apk-signer) or apksigner
- ADB
- Rec Room APK build `20230427` (versionCode `22397318`)
- Il2CppDumper (used to generate the `dump.cs` and find RVAs)

## How to apply

See [PatchStuff/BUILDING.md](PatchStuff/BUILDING.md) for the full step-by-step guide.

1. Build Dobby for `arm64-v8a`
2. Compile `hook.c` with Android NDK
3. Extract the APK and place `librecroom_patch.so` in `lib/arm64-v8a/`
4. Repack, sign and install via ADB
5. Copy the OBB folder to `Android/obb/com.AgainstGravity.RecRoom/`

## Target Build

| Field | Value |
|---|---|
| versionName | `20230427` |
| versionCode | `22397318` |
| Architecture | `arm64-v8a` |
| Min Android | 7.1 |
| Hook target RVA | `0x4F19B58` |
| Redirect to | `127.0.0.1:8805` |

## How it works

The patch hooks `GFAFKJLDJKL` (the obfuscated function that builds Photon's
`AppSettings` object at startup), overwrites the `Server` field with
`127.0.0.1:8805` and disables `UseNameServer` so the client connects directly
to the local server instead of Exitgames' name server.

## Related Projects

- [RecFlare](https://github.com/djdevin/recflare) — Original server implementation (Quest patch reference)

## Why Theres Only Android Apps?

Because i have this idea when im on the car and i dont have pc in car i dont want to wait to get home so ive done this on android :) anyways the patch can be used with pc with adb commands, sorry 🥲

## License

MIT License — Copyright (c) 2026 ilfigliodiele
