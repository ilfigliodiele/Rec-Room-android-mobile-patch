# Rec Room Android patch — github / LAN (192.168.1.91:8805)

Native `arm64-v8a` hook for the Android client of Rec Room, build **20230427**
(versionCode `22397318`). This is the public/demo variant: the phone talks to a
server on the local network.

## What it changes

1. **Discovery** — every URL containing `https://ns.rec.net` is rewritten to
   `http://192.168.1.91:8805` (scheme included, because the LAN server is plain
   HTTP). A bare `ns.rec.net` becomes `192.168.1.91:8805`.
2. **Photon** — `LHEGIADAAFB::GFAFKJLDJKL`, the method that builds Photon's
   `AppSettings`, is hooked after it runs and gets:

   ```
   Server        = 192.168.1.91
   Port          = 8805
   UseNameServer = false
   ```

Both values live in `CMakeLists.txt` (`PATCH_PHOTON_SERVER`, `PATCH_PHOTON_PORT`,
`PATCH_DISCOVERY_URL`) — change them there, no build flags needed.

## RVAs (dump.cs, build 20230427)

| Symbol | RVA |
|---|---|
| `LHEGIADAAFB::GFAFKJLDJKL(MCAFIGPOLAC)` → `AppSettings` | `0x4F19B58` |
| `UnityWebRequest::.ctor(string, string)` | `0x383DC84` |
| `UnityWebRequest::.ctor(string, string, DownloadHandler, UploadHandler)` | `0x383E07C` |
| `UnityWebRequest::set_url(string)` | `0x383DCE4` |
| `UnityWebRequest::Get(string)` | `0x383FC24` |
| `UnityWebRequest::Post(string, string)` | `0x383FCCCu` |
| `System.Uri::.ctor(string)` | `0x69A192C` |
| `System.Uri::.ctor(string, UriKind)` | `0x69A1B88` |

`AppSettings` offsets: `UseNameServer 0x30`, `Server 0x48`, `Port 0x50`,
`Protocol 0x60` (`AppIdRealtime 0x10`, `AppIdChat 0x18`, `AppIdVoice 0x20`,
`AppVersion 0x28`, `FixedRegion 0x38`, `BestRegionSummary 0x40`,
`ProxyServer 0x58`, `EnableProtocolFallback 0x61`, `AuthMode 0x64`,
`EnableLobbyStatistics 0x68`, `NetworkLogging 0x69`).

## Build

Needs Android NDK r25c+ and a static Dobby for `arm64-v8a`:

```bash
git clone https://github.com/jmpews/Dobby.git
cd Dobby && mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
         -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
         -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON
make -j4
```

Copy `libdobby.a` to `libs/libdobby.a` and Dobby's `include/` to
`libs/include/`, then:

```bash
export NDK=/path/to/ndk
./build.sh            # -> build/librecroom_patch.so
```

## Load the library

Copying the `.so` into `lib/arm64-v8a/` is not enough. Either:

```bash
# A. make an existing library depend on it
patchelf --add-needed librecroom_patch.so lib/arm64-v8a/libunity.so
```

or call it from Java (apktool), in the `Application` class:

```smali
const-string v0, "recroom_patch"
invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
```

Check `adb logcat -s RecRoomPatch`: `patch loaded`, `libil2cpp base=...`,
the `hook ... rva=...` lines, `patch ready`.

## Apply to the APK

```bash
unzip -q RecRoom.apk -d out
cp build/librecroom_patch.so out/lib/arm64-v8a/
patchelf --add-needed librecroom_patch.so out/lib/arm64-v8a/libunity.so   # option A
cd out && zip -qr ../RecRoom-patched.apk . && cd ..
zipalign -p -f 4 RecRoom-patched.apk RecRoom-aligned.apk
apksigner sign --ks my-key.jks --out RecRoom-signed.apk RecRoom-aligned.apk
adb install -r RecRoom-signed.apk
adb push Android/obb/com.AgainstGravity.RecRoom /sdcard/Android/obb/com.AgainstGravity.RecRoom
```

Keep the OBB folder of the **same** build (`20230427`); a repacked APK is signed
with your key, so uninstall the Play version first.

## Phone / server requirements

1. **Cleartext HTTP.** Android 9+ blocks plain `http://` by default. Add
   `android:usesCleartextTraffic="true"` to the `<application>` tag of
   `AndroidManifest.xml` (apktool) and rebuild, otherwise every request fails
   with `Cleartext HTTP traffic not permitted`.
2. **Reachable address.** `192.168.1.91` must be the PC's LAN IP, the server
   must listen on `0.0.0.0:8805`, and the phone must be on the same Wi-Fi (no
   guest-network isolation).
3. **Service map.** Only the discovery request is rewritten. The JSON returned
   by the server supplies every other service URL, so it must advertise hosts
   the phone can reach (e.g. a local DNS entry for `*.recrum.xyz` → `192.168.1.91`,
   with the worker serving `http://…:8805`). If the response still points at
   public `https://` hosts, the phone will use those instead.
4. **Server build list.** `20230427` must be accepted:

   ```env
   RECFLARE_SUPPORTED_GAME_VERSIONS=20230414,20230427,20250718
   ```

## Limitations

* `20230427` only — RVAs and field offsets are build specific.
* Photon must actually listen on `192.168.1.91:8805` and speak the **Photon
  Realtime** protocol. The RecFlare HTTP/JSON API on that port will not answer
  it; a self-hosted Photon server usually listens on `9093`, in which case set
  `PATCH_PHOTON_PORT` accordingly.
* Realtime still needs valid Photon app ids in `AppSettings`; the hook does not
  invent them.
* Not runtime-tested: the RVAs come from `dump.cs` and the hooks follow that
  build's IL2CPP ABI.
