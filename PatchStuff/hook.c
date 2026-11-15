#include <string.h>
#include <stdlib.h>
#include <android/log.h>
#include "dobby.h"

#define TAG "RecRoomPatch"
#define TARGET_SERVER "127.0.0.1:8805"

// RVA della funzione GFAFKJLDJKL che costruisce AppSettings
// Build: 20230427 / versionCode 22397318
#define GFAFKJLDJKL_RVA 0x4F19B58

// Offset del campo Server dentro AppSettings
#define APPSETTINGS_SERVER_OFFSET 0x48

typedef struct {
    void* klass;
    void* monitor;
    void* str; // puntatore alla stringa C# (System.String)
} Il2CppString;

typedef struct {
    char pad[0x48];
    Il2CppString* Server; // offset 0x48
} AppSettings;

// Puntatore alla funzione originale
static void* (*orig_GFAFKJLDJKL)(void* thisPtr, void* arg) = NULL;

// Funzione per creare una Il2CppString da una stringa C
// Dobbiamo usare il metodo di IL2CPP per allocare stringhe
extern Il2CppString* il2cpp_string_new(const char* str);

// Hook della funzione
static void* hook_GFAFKJLDJKL(void* thisPtr, void* arg) {
    // Chiama la funzione originale
    AppSettings* settings = (AppSettings*)orig_GFAFKJLDJKL(thisPtr, arg);

    if (settings != NULL) {
        // Sovrascrivi il campo Server con il nostro server locale
        settings->Server = il2cpp_string_new(TARGET_SERVER);
        // Disabilita NameServer per usare direttamente il nostro server
        // AppSettings.UseNameServer è a offset 0x30 (bool)
        *((uint8_t*)settings + 0x30) = 0; // UseNameServer = false

        __android_log_print(ANDROID_LOG_INFO, TAG,
            "Server redirected to: %s", TARGET_SERVER);
    }

    return (void*)settings;
}

// Funzione chiamata all'avvio della libreria
__attribute__((constructor))
void patch_init() {
    __android_log_print(ANDROID_LOG_INFO, TAG, "Patch loaded!");

    // Ottieni il base address di libil2cpp.so
    // Dobby si occupa di trovare il base address
    void* base = DobbyGetImageBase("libil2cpp.so");
    if (!base) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to get libil2cpp base!");
        return;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG, "libil2cpp base: %p", base);

    // Calcola l'indirizzo della funzione
    void* target = (void*)((uintptr_t)base + GFAFKJLDJKL_RVA);

    // Installa il hook con Dobby
    int ret = DobbyHook(target, (void*)hook_GFAFKJLDJKL, (void**)&orig_GFAFKJLDJKL);
    if (ret == 0) {
        __android_log_print(ANDROID_LOG_INFO, TAG,
            "Hook installed at RVA 0x%x", GFAFKJLDJKL_RVA);
    } else {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
            "Hook failed: %d", ret);
    }
}
