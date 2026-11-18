# Rec Room Android patch — public build

Native `arm64-v8a` hook for the Android client of Rec Room, build **20230427**
(versionCode `22397318`). This is the public build: the phone talks HTTPS to
`https://recrum.xyz` (but you can modify the url) and the anti-cheat quit is suppressed.

The private build (same server, no anti-cheat suppression) lives in
`../recroom-patch-recrum`.

## What it changes

1. **Discovery** — every URL the client hands to the HTTP layer is scanned and
   `ns.rec.net` is replaced with `recrum.xyz`, so the client asks
   `https://recrum.xyz` for the service map. Everything else (API, Auth,
   Matchmaking, Images, …) comes from that response.

   Hooked entry points:

   * `UnityWebRequest::.ctor(string, string)`
   * `UnityWebRequest::.ctor(string, string, DownloadHandler, UploadHandler)`
   * `UnityWebRequest::set_url(string)`
   * `UnityWebRequest::Get(string)` / `Post(string, string)`
   * `System.Uri::.ctor(string)` / `.ctor(string, UriKind)`

2. **TLS** — Cloudflare serves `recrum.xyz` with a Let's Encrypt ECDSA chain
   that the client's bundled mbedtls cannot validate on Android 17
   (`MBEDTLS_ERR_X509_CERT_VERIFY_FAILED`). Both Mono/UnityTLS verification
   callbacks are wrapped: the original still runs, a failure result is turned
   into `UNITYTLS_X509VERIFY_SUCCESS`.

3. **Photon** — after the client builds `AppSettings`, the NameServer is forced
   to `photon.recflare.net:5058` with `UseNameServer = true`. The three app ids
   (realtime/voice/chat) still come from `GET /player/connection-info`, so the
   server hands them out; nothing is baked into the APK.

4. **Google Play identity** — a re-signed APK cannot get a real GPGS identity,
   so the Java layer reports the placeholder `"1"`. Every install reporting the
   same id would share one server account, so the il2cpp sign-in callback
   replaces it with a random 32-hex id generated once per install and stored in
   the app's `files/rr_platform_id`.

5. **Anti-cheat** — the client's EAC certificate check fails against this
   server (the TLS chain is accepted, but the pinning data does not match) and
   ends in `SessionManager.FatalApplicationQuit`, which closes the app. That
   method is hooked and the quit is dropped; the server's `/eac/challenge` is a
   constant and enforces nothing.

6. **Images** — every image URL the client builds carries `?sig=p1`, and the
   client expects the response to carry a `Content-Signature` header: an
   RSA-SHA1 signature over the bytes, verified against Rec Room's public key
   `KEY:RSA:p1.rec.net`. This server cannot produce one (the private half of
   that key is not public), so `img` sends a structurally valid placeholder.
   The Android client actually verifies it — unlike the desktop client — and
   drops the image with `Image signature verification failed`, which leaves
   profile pictures, room thumbnails and banners blank. The check
   `KMNHANBDJGA::DGEDKKPIKLI(byte[], byte[])` is hooked and forced to `true`.

   That method is private to the image manager; nothing else in the client calls
   it, so the bypass only affects image acceptance. This is why the desktop
   patch never needed it.

## RVAs (dump.cs, build 20230427)

| Symbol | RVA |
|---|---|
| `UnityWebRequest::.ctor(string, string)` | `0x383DC84` |
| `UnityWebRequest::.ctor(string, string, DownloadHandler, UploadHandler)` | `0x383E07C` |
| `UnityWebRequest::set_url(string)` | `0x383DCE4` |
| `UnityWebRequest::Get(string)` | `0x383FC24` |
| `UnityWebRequest::Post(string, string)` | `0x383FCCCu` |
| `System.Uri::.ctor(string)` | `0x69A192C` |
| `System.Uri::.ctor(string, UriKind)` | `0x69A1B88` |
| `UnityTlsContext` verify callback (static) | `0x699C148` |
| `UnityTlsContext` verify callback (instance) | `0x699E0B0` |
| `AppSettings` builder (`LHEGIADAAFB::GFAFKJLDJKL`) | `0x4F19B58` |
| `IPHFAHKKDKK::JNICallback_OnSignInComplete(bool,string,string)` | `0xA436B34` |
| `SessionManager::FatalApplicationQuit(int,string)` | `0x3D5F98C` |
| `KMNHANBDJGA::DGEDKKPIKLI(byte[], byte[])` (image signature) | `0x64A02A0` |

`AppSettings` offsets: `UseNameServer 0x30`, `Server 0x48`, `Port 0x50`,
`Protocol 0x60` (`AppIdRealtime 0x10`, `AppIdChat 0x18`, `AppIdVoice 0x20`,
`AppVersion 0x28`, `FixedRegion 0x38`, `BestRegionSummary 0x40`,
`ProxyServer 0x58`, `EnableProtocolFallback 0x61`, `AuthMode 0x64`,
`EnableLobbyStatistics 0x68`, `NetworkLogging 0x69`).

## Build

Dobby is vendored in `libs/` (`libdobby.a` + `include/dobby.h`), so the only
requirement is Android NDK r25c+:

```bash
export NDK=/path/to/ndk
./build.sh            # -> build/librecroom_patch.so
```

All configuration is in `CMakeLists.txt` (`PATCH_SERVER_DOMAIN`,
`PATCH_PHOTON_NAMESERVER`, `PATCH_PHOTON_NAMESERVER_PORT`); no build flags are
needed.

To rebuild Dobby from source instead:

```bash
git clone https://github.com/jmpews/Dobby.git
cd Dobby && mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
         -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
         -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON
make -j4
```

Copy `libdobby.a` to `libs/libdobby.a` and Dobby's headers to `libs/include/`.

> Upstream Dobby `master` does not compile for Android out of the box in this
> snapshot. The vendored `libdobby.a` was built from tag `latest` with small
> fixes (arm64 closure bridge asm relocation, `OSMemory` permission call in
> `os_arch_features.h`, `RuntimeModule.base` in the Linux backend).

## Prebuilt

```
prebuilt/arm64-v8a/librecroom_patch.so
```

## Load the library

Copying the `.so` into `lib/arm64-v8a/` is not enough. Either make an existing
library depend on it:

```bash
patchelf --add-needed librecroom_patch.so lib/arm64-v8a/libunity.so
```

or call it from Java (apktool), in the `Application` class:

```smali
const-string v0, "recroom_patch"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
```

Check `adb logcat -s RecRoomPatch`: `patch loaded`, `libil2cpp base=...`, the
`hook ... rva=...` lines, `patch ready`, `install platform id = ...`, and
`rewrote discovery URL -> https://recrum.xyz/...`.

## Apply to the APK

`tools/package_apk.py` does the whole repack (library, `libmain.so` `DT_NEEDED`,
optional patched `classes.dex`):

```bash
python tools/package_apk.py \
  --apk RecRoom.apk \
  --libmain libmain.patched.so \
  --patch-so build/librecroom_patch.so \
  --dex classes.patched.dex \
  --out RecRoom-patched.apk
```

Then `zipalign -p -f 4` and `apksigner sign`. Keep the OBB folder of the
**same** build (`20230427`); a repacked APK is signed with your key, so uninstall
the Play version first.

## Server requirements

* `20230427` must be accepted:

  ```env
  RECFLARE_SUPPORTED_GAME_VERSIONS=20230414,20230427,20250718
  ```

* Photon app ids must be set (they are handed to the client):

  ```env
  RECFLARE_PHOTON_REALTIME_APP_ID=rf-...
  RECFLARE_PHOTON_VOICE_APP_ID=rf-...
  RECFLARE_PHOTON_CHAT_APP_ID=rf-...
  RECFLARE_PHOTON_REGION=us
  ```

  and the NameServer must be reachable at `photon.recflare.net:5058`.

* Google Play is trusted as a login platform for this deployment
  (`CACHED_LOGIN_PLATFORMS` + the GooglePlay branch in `verifyPlatformProof`).

## Limitations

* `20230427` only — RVAs and field offsets are build specific.
* A reinstall that clears app data generates a new platform id, i.e. a new
  account; log in with the account's password to link it again.
* `photon.recflare.net` is UDP-only; TCP probes to 5058/5055/4530 are refused
  even when the client connects fine.
