# Building the Rec Room Android Patch

This guide explains how to compile and apply the native hook that redirects
Rec Room (build `20230427`) to a local server at `127.0.0.1:8805`.

---

## Prerequisites

### Tools to install

| Tool | Download |
|---|---|
| Android NDK r25c | https://developer.android.com/ndk/downloads |
| CMake 3.22+ | Included with Android Studio, or https://cmake.org |
| Dobby (hooking library) | https://github.com/jmpews/Dobby |
| apktool | https://apktool.org |
| uber-apk-signer | https://github.com/patrickfav/uber-apk-signer |
| ADB | https://developer.android.com/tools/adb |
| Java 11+ | https://adoptium.net |

---

## Step 1 — Build Dobby for Android arm64

Dobby is the hooking library used to intercept IL2CPP functions at runtime.
You need to compile it for `arm64-v8a` before building the patch.

```bash
git clone https://github.com/jmpews/Dobby.git
cd Dobby
mkdir build && cd build

cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release

make -j4
```

This produces `libdobby.a` inside the `build/` folder. Note its path — you
will need it in Step 2.

---

## Step 2 — Set up the patch project

Create the following folder structure:

```
recroom-patch/
├── CMakeLists.txt
├── hook.c
└── libs/
    ├── libdobby.a          ← copy from Dobby build/
    └── include/
        └── dobby.h         ← copy from Dobby include/
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.22)
project(recroom_patch)

set(CMAKE_C_STANDARD 11)

add_library(recroom_patch SHARED hook.c)

# Link Dobby (static) and Android system libs
target_link_libraries(recroom_patch
    ${CMAKE_SOURCE_DIR}/libs/libdobby.a
    android
    log
)

target_include_directories(recroom_patch PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/include
)
```

---

## Step 3 — Compile the hook

From the `recroom-patch/` folder:

```bash
mkdir build && cd build

cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release

make -j4
```

Replace `$NDK` with your actual NDK path, for example:
- Windows: `C:/Users/you/AppData/Local/Android/Sdk/ndk/25.2.9519653`
- Linux/macOS: `~/Android/Sdk/ndk/25.2.9519653`

This produces `librecroom_patch.so` inside `build/`.

---

## Step 4 — Inject the hook into the APK

### 4a. Extract the APK

Rename `com.AgainstGravity.RecRoom.apk` to `.zip` and extract it, or use:

```bash
mkdir RecRoom_extracted
cd RecRoom_extracted
unzip ../com.AgainstGravity.RecRoom.apk
```

### 4b. Copy the hook library

Place `librecroom_patch.so` into the extracted APK:

```bash
cp librecroom_patch.so RecRoom_extracted/lib/arm64-v8a/
```

### 4c. Repack the APK

```bash
cd RecRoom_extracted
zip -r ../RecRoom_patched_unsigned.apk .
```

---

## Step 5 — Sign the APK

### Create a keystore (first time only)

```bash
keytool -genkey -v \
  -keystore my-key.jks \
  -keyalg RSA \
  -keysize 2048 \
  -validity 10000 \
  -alias recroom
```

### Sign the APK

```bash
java -jar uber-apk-signer.jar \
  --apks RecRoom_patched_unsigned.apk \
  --ks my-key.jks \
  --ksAlias recroom \
  --out .
```

This produces `RecRoom_patched_unsigned-aligned-signed.apk`.

---

## Step 6 — Install on the device

```bash
adb install RecRoom_patched_unsigned-aligned-signed.apk
```

Then copy the OBB folder to your device:

```bash
adb push Android/obb/com.AgainstGravity.RecRoom \
  /sdcard/Android/obb/com.AgainstGravity.RecRoom
```

---

## Step 7 — Verify the patch is working

With the device connected, open logcat and filter for the patch tag:

```bash
adb logcat -s RecRoomPatch
```

You should see:

```
RecRoomPatch: Patch loaded!
RecRoomPatch: libil2cpp base: 0x...
RecRoomPatch: Hook installed at RVA 0x4F19B58
RecRoomPatch: Server redirected to: 127.0.0.1:8805
```

If you see these lines, the hook is active and the client will try to connect
to your local server on port 8805.

---

## Notes

- The RVA `0x4F19B58` is specific to build `20230427` (versionCode `22397318`).
  Using a different build will crash the app or produce no effect.
- Your local server must be running and listening on `127.0.0.1:8805` before
  launching the game.
- If the hook fails to install, check that Dobby was compiled for `arm64-v8a`
  and that the NDK version matches.

---

## License

MIT License — Copyright (c) 2026 ilfigliodiele
