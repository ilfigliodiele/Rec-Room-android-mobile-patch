Rec Room 2023 Android Patch
⚠️ Work in Progress — This patch is currently in development.
A native patch for Rec Room Android (build 20230427 / versionCode 22397318) that redirects the client to a custom private server, bypassing the official Against Gravity servers.
Status
[ ] Identify target functions in libil2cpp.so
[ ] Calculate RVA offsets for build 22397318
[ ] Write native hook in C/C++
[ ] Compile with Android NDK (arm64-v8a)
[ ] Test on device
Requirements
To build the patch
Android NDK
Ghidra (for reverse engineering)
The original libil2cpp.so from build 20230427
To apply the patch
ZArchiver or MT Manager
uber-apk-signer or apksigner
ADB
Rec Room APK build 20230427 (versionCode 22397318)
How to apply
Extract the APK contents
Replace lib/arm64-v8a/libil2cpp.so with the patched version
Repack the APK
Sign with your own keystore
Install via ADB: adb install RecRoom-patched.apk
Copy the OBB folder to Android/obb/com.AgainstGravity.RecRoom/
Target Build
Field
Value
versionName
20230427
versionCode
22397318
Architecture
arm64-v8a
Min Android
7.1
Related Projects
RecFlare — Original server implementation (Quest patch reference)
License
MIT License — Copyright (c) 2026 ilfigliodiele