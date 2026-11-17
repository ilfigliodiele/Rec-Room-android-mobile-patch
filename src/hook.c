/*
 * Rec Room Android native patch — build 20230427 (versionCode 22397318)
 * arm64-v8a, IL2CPP, libil2cpp.so
 *
 * Two variants, pick exactly one at build time:
 *
 *   -DPATCH_VARIANT_GITHUB   (public/demo, LAN server)
 *       1. rewrites the RecNet discovery host
 *              https://ns.rec.net  ->  http://192.168.1.91:8805
 *          so the client asks the LAN server for the service map;
 *       2. hooks LHEGIADAAFB::GFAFKJLDJKL (the method that builds Photon's
 *          AppSettings) and sets
 *              Server = 192.168.1.91, Port = 8805, UseNameServer = false
 *
 *   -DPATCH_VARIANT_RECRUM   (private)
 *       Rewrites the RecNet discovery host from ns.rec.net to recrum.xyz
 *       inside every URL the client hands to the HTTP layer
 *       (UnityWebRequest ctor / set_url / Get / Post and System.Uri ctors).
 *       The discovery response then supplies every other service URL, which
 *       is exactly what the PC patch does with ApiHost.
 *
 * All RVAs below come from dump.cs of this exact build. They are build
 * specific: a different client build needs a new dump and new RVAs.
 */

#include <android/log.h>
#include <dlfcn.h>
#include <link.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dobby.h"

#define TAG "RecRoomPatch"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#if defined(PATCH_VARIANT_GITHUB) && defined(PATCH_VARIANT_RECRUM)
#error "define only one of PATCH_VARIANT_GITHUB / PATCH_VARIANT_RECRUM"
#endif
#if !defined(PATCH_VARIANT_GITHUB) && !defined(PATCH_VARIANT_RECRUM)
#error "define PATCH_VARIANT_GITHUB or PATCH_VARIANT_RECRUM"
#endif

#ifndef PATCH_PHOTON_SERVER
#define PATCH_PHOTON_SERVER "192.168.1.91"
#endif
#ifndef PATCH_PHOTON_PORT
#define PATCH_PHOTON_PORT 8805
#endif
#ifndef PATCH_SERVER_DOMAIN
#define PATCH_SERVER_DOMAIN "recrum.xyz"
#endif
#ifndef PATCH_DISCOVERY_URL
#define PATCH_DISCOVERY_URL "http://192.168.1.91:8805"
#endif

/* ------------------------------------------------------------------ */
/* RVAs (dump.cs, build 20230427 / versionCode 22397318)              */
/* ------------------------------------------------------------------ */

/* LHEGIADAAFB : IEFHEGMCFGI — public AppSettings GFAFKJLDJKL(MCAFIGPOLAC) */
#define RVA_PHOTON_APPSETTINGS   0x4F19B58u

/* UnityEngine.Networking.UnityWebRequest */
#define RVA_UWR_CTOR_STR         0x383DC84u  /* .ctor(string url, string method) */
#define RVA_UWR_CTOR_STR4        0x383E07Cu  /* .ctor(string, string, DownloadHandler, UploadHandler) */
#define RVA_UWR_SET_URL          0x383DCE4u  /* set_url(string) */
#define RVA_UWR_GET              0x383FC24u  /* static Get(string) */
#define RVA_UWR_POST             0x383FCCCu  /* static Post(string, string) */

/* System.Uri */
#define RVA_URI_CTOR_STR         0x69A192Cu  /* .ctor(string uriString) */
#define RVA_URI_CTOR_STR_KIND    0x69A1B88u  /* .ctor(string uriString, UriKind uriKind) */

/* ------------------------------------------------------------------ */
/* IL2CPP helpers                                                     */
/* ------------------------------------------------------------------ */

/* IL2CPP object header is { klass*, monitor* } = 16 bytes, then the
 * int32 length, then inline UTF-16 characters at offset 0x14. This is
 * NOT the Mono layout (klass/monitor/str) the original hook.c assumed. */
typedef struct {
	void* klass;
	void* monitor;
	int32_t length;
	uint16_t chars[];
} Il2CppString;

typedef Il2CppString* (*il2cpp_string_new_t)(const char*);

static uintptr_t g_base = 0;
static il2cpp_string_new_t g_il2cpp_string_new = NULL;

static int module_cb(struct dl_phdr_info* info, size_t size, void* data) {
	(void)size;
	if (info->dlpi_name && strstr(info->dlpi_name, "libil2cpp.so")) {
		*(uintptr_t*)data = (uintptr_t)info->dlpi_addr;
		return 1;
	}
	return 0;
}

static uintptr_t find_libil2cpp_base(void) {
	uintptr_t base = 0;
	dl_iterate_phdr(module_cb, &base);
	if (!base) {
		/* Make sure the linker has it mapped, then look again. */
		void* h = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
		if (h) {
			dl_iterate_phdr(module_cb, &base);
			dlclose(h);
		}
	}
	return base;
}

static bool il2cpp_contains(Il2CppString* s, const char* needle) {
	if (!s || s->length <= 0) return false;
	const int32_t n = (int32_t)strlen(needle);
	if (s->length < n) return false;
	for (int32_t i = 0; i + n <= s->length; i++) {
		int32_t j = 0;
		while (j < n && s->chars[i + j] == (uint16_t)(unsigned char)needle[j]) j++;
		if (j == n) return true;
	}
	return false;
}

/* Returns a new IL2CPP string with every `from` replaced by `to`, or the
 * original pointer when there is nothing to replace. URLs are ASCII. */
static Il2CppString* il2cpp_replace(Il2CppString* s, const char* from, const char* to) {
	if (!s || !il2cpp_contains(s, from) || !g_il2cpp_string_new) return s;

	const int32_t n = s->length;
	char* buf = (char*)malloc((size_t)n + 1);
	if (!buf) return s;
	for (int32_t i = 0; i < n; i++) {
		const uint16_t c = s->chars[i];
		buf[i] = (c < 0x80) ? (char)c : '?';
	}
	buf[n] = '\0';

	const size_t flen = strlen(from);
	const size_t tlen = strlen(to);
	size_t hits = 0;
	for (char* p = buf; (p = strstr(p, from)) != NULL; p += flen) hits++;
	if (!hits) {
		free(buf);
		return s;
	}

	const size_t grown = (tlen > flen) ? hits * (tlen - flen) : 0;
	char* out = (char*)malloc((size_t)n + grown + 1);
	if (!out) {
		free(buf);
		return s;
	}

	const char* src = buf;
	char* dst = out;
	while (true) {
		const char* hit = strstr(src, from);
		if (!hit) {
			strcpy(dst, src);
			break;
		}
		const size_t pre = (size_t)(hit - src);
		memcpy(dst, src, pre);
		dst += pre;
		memcpy(dst, to, tlen);
		dst += tlen;
		src = hit + flen;
	}

	Il2CppString* result = g_il2cpp_string_new(out);
	free(out);
	free(buf);
	return result ? result : s;
}

/* ------------------------------------------------------------------ */
/* Discovery-host rewrite (both variants)                             */
/* ------------------------------------------------------------------ */

static Il2CppString* patch_rewrite(Il2CppString* s) {
#ifdef PATCH_VARIANT_GITHUB
	/* https://ns.rec.net -> http://192.168.1.91:8805 (scheme included:
	 * the LAN server is plain HTTP), then bare ns.rec.net -> host:port. */
	Il2CppString* out = il2cpp_replace(s, "https://ns.rec.net", PATCH_DISCOVERY_URL);
	const char* hostport = PATCH_DISCOVERY_URL;
	const char* sep = strstr(hostport, "://");
	if (sep) hostport = sep + 3;
	out = il2cpp_replace(out, "ns.rec.net", hostport);
#else
	Il2CppString* out = il2cpp_replace(s, "ns.rec.net", PATCH_SERVER_DOMAIN);
#endif
	if (out != s && out) {
		char buf[512];
		int32_t n = out->length < 511 ? out->length : 511;
		for (int32_t i = 0; i < n; i++) buf[i] = (char)out->chars[i];
		buf[n] = '\0';
		LOGI("rewrote discovery URL -> %s", buf);
	}
	return out;
}

/* UnityWebRequest.ctor(string url, string method) */
typedef void (*uwr_ctor2_t)(void* self, Il2CppString* url, Il2CppString* method);
static uwr_ctor2_t orig_uwr_ctor2 = NULL;
static void hook_uwr_ctor2(void* self, Il2CppString* url, Il2CppString* method) {
	orig_uwr_ctor2(self, patch_rewrite(url), method);
}

/* UnityWebRequest.ctor(string url, string method, DownloadHandler, UploadHandler) */
typedef void (*uwr_ctor4_t)(void* self, Il2CppString* url, Il2CppString* method, void* dh, void* uh);
static uwr_ctor4_t orig_uwr_ctor4 = NULL;
static void hook_uwr_ctor4(void* self, Il2CppString* url, Il2CppString* method, void* dh, void* uh) {
	orig_uwr_ctor4(self, patch_rewrite(url), method, dh, uh);
}

/* UnityWebRequest.set_url(string) */
typedef void (*uwr_set_url_t)(void* self, Il2CppString* value);
static uwr_set_url_t orig_uwr_set_url = NULL;
static void hook_uwr_set_url(void* self, Il2CppString* value) {
	orig_uwr_set_url(self, patch_rewrite(value));
}

/* static UnityWebRequest.Get(string) */
typedef void* (*uwr_get_t)(Il2CppString* uri);
static uwr_get_t orig_uwr_get = NULL;
static void* hook_uwr_get(Il2CppString* uri) {
	return orig_uwr_get(patch_rewrite(uri));
}

/* static UnityWebRequest.Post(string, string) */
typedef void* (*uwr_post_t)(Il2CppString* uri, Il2CppString* data);
static uwr_post_t orig_uwr_post = NULL;
static void* hook_uwr_post(Il2CppString* uri, Il2CppString* data) {
	return orig_uwr_post(patch_rewrite(uri), data);
}

/* System.Uri.ctor(string) */
typedef void (*uri_ctor1_t)(void* self, Il2CppString* uriString);
static uri_ctor1_t orig_uri_ctor1 = NULL;
static void hook_uri_ctor1(void* self, Il2CppString* uriString) {
	orig_uri_ctor1(self, patch_rewrite(uriString));
}

/* System.Uri.ctor(string, UriKind) */
typedef void (*uri_ctor2_t)(void* self, Il2CppString* uriString, int32_t kind);
static uri_ctor2_t orig_uri_ctor2 = NULL;
static void hook_uri_ctor2(void* self, Il2CppString* uriString, int32_t kind) {
	orig_uri_ctor2(self, patch_rewrite(uriString), kind);
}

/* ------------------------------------------------------------------ */
/* Variant: github — Photon AppSettings -> LAN Photon                 */
/* ------------------------------------------------------------------ */

#ifdef PATCH_VARIANT_GITHUB

typedef struct {
	void* klass;
	void* monitor;
	Il2CppString* appIdRealtime;        /* 0x10 */
	Il2CppString* appIdChat;            /* 0x18 */
	Il2CppString* appIdVoice;           /* 0x20 */
	Il2CppString* appVersion;           /* 0x28 */
	uint8_t useNameServer;              /* 0x30 */
	Il2CppString* fixedRegion;          /* 0x38 */
	Il2CppString* bestRegionSummary;    /* 0x40 */
	Il2CppString* server;               /* 0x48 */
	int32_t port;                       /* 0x50 */
	Il2CppString* proxyServer;          /* 0x58 */
	int32_t protocol;                   /* 0x60 */
	uint8_t enableProtocolFallback;     /* 0x61 */
	int32_t authMode;                   /* 0x64 */
	uint8_t enableLobbyStatistics;      /* 0x68 */
	uint8_t networkLogging;             /* 0x69 */
} AppSettings;

typedef void* (*appsettings_builder_t)(void* self, void* matchInfo);
static appsettings_builder_t orig_appsettings_builder = NULL;

static void* hook_appsettings_builder(void* self, void* matchInfo) {
	void* ret = orig_appsettings_builder(self, matchInfo);
	AppSettings* s = (AppSettings*)ret;
	if (s) {
		s->server = g_il2cpp_string_new(PATCH_PHOTON_SERVER);
		s->port = PATCH_PHOTON_PORT;
		s->useNameServer = 0;
		LOGI("Photon AppSettings: Server=%s Port=%d UseNameServer=false",
			 PATCH_PHOTON_SERVER, PATCH_PHOTON_PORT);
	}
	return ret;
}

#endif /* PATCH_VARIANT_GITHUB */

/* ------------------------------------------------------------------ */
/* Hook installation                                                  */
/* ------------------------------------------------------------------ */

static void install_hook(const char* name, uintptr_t rva, void* replace, void** origin) {
	void* target = (void*)(g_base + rva);
	const int rc = DobbyHook(target, (dobby_dummy_func_t)replace, (dobby_dummy_func_t*)origin);
	if (rc == 0) {
		LOGI("hook %-28s rva=0x%lx addr=%p", name, (unsigned long)rva, target);
	} else {
		LOGE("hook %-28s rva=0x%lx FAILED rc=%d", name, (unsigned long)rva, rc);
	}
}

static void install_http_hooks(void) {
	install_hook("UnityWebRequest.ctor", RVA_UWR_CTOR_STR,
				 (void*)hook_uwr_ctor2, (void**)&orig_uwr_ctor2);
	install_hook("UnityWebRequest.ctor4", RVA_UWR_CTOR_STR4,
				 (void*)hook_uwr_ctor4, (void**)&orig_uwr_ctor4);
	install_hook("UnityWebRequest.set_url", RVA_UWR_SET_URL,
				 (void*)hook_uwr_set_url, (void**)&orig_uwr_set_url);
	install_hook("UnityWebRequest.Get", RVA_UWR_GET,
				 (void*)hook_uwr_get, (void**)&orig_uwr_get);
	install_hook("UnityWebRequest.Post", RVA_UWR_POST,
				 (void*)hook_uwr_post, (void**)&orig_uwr_post);
	install_hook("System.Uri.ctor", RVA_URI_CTOR_STR,
				 (void*)hook_uri_ctor1, (void**)&orig_uri_ctor1);
	install_hook("System.Uri.ctor(kind)", RVA_URI_CTOR_STR_KIND,
				 (void*)hook_uri_ctor2, (void**)&orig_uri_ctor2);
}

static void install_hooks(void) {
#ifdef PATCH_VARIANT_GITHUB
	install_hook("Photon.AppSettings", RVA_PHOTON_APPSETTINGS,
				 (void*)hook_appsettings_builder, (void**)&orig_appsettings_builder);
#endif
	install_http_hooks();
}

static void* init_thread(void* arg) {
	(void)arg;
	for (int i = 0; i < 600 && !g_base; i++) {
		g_base = find_libil2cpp_base();
		if (!g_base) usleep(50 * 1000); /* 50 ms, up to 30 s */
	}
	if (!g_base) {
		LOGE("libil2cpp.so not found, giving up");
		return NULL;
	}

	g_il2cpp_string_new = (il2cpp_string_new_t)dlsym(RTLD_DEFAULT, "il2cpp_string_new");
	if (!g_il2cpp_string_new) {
		void* h = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
		if (h) g_il2cpp_string_new = (il2cpp_string_new_t)dlsym(h, "il2cpp_string_new");
	}
	if (!g_il2cpp_string_new) {
		LOGE("il2cpp_string_new not found, cannot build strings");
		return NULL;
	}

	LOGI("libil2cpp base=%p", (void*)g_base);
	install_hooks();
	LOGI("patch ready");
	return NULL;
}

__attribute__((constructor))
static void patch_init(void) {
	LOGI("patch loaded");
	g_base = find_libil2cpp_base();
	if (g_base) {
		init_thread(NULL);
		return;
	}
	pthread_t t;
	if (pthread_create(&t, NULL, init_thread, NULL) == 0) {
		pthread_detach(t);
	} else {
		LOGE("pthread_create failed");
	}
}
