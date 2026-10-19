#include "core.h"

/*
 * HTTPS via WinHTTP, loaded at run time so the program still starts on Windows 95-Vista.
 * Only used on Windows 7 and later (modern servers need TLS 1.2).
 */
typedef void *HINET;
typedef HINET(WINAPI *PFOpen)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
typedef HINET(WINAPI *PFConnect)(HINET, LPCWSTR, WORD, DWORD);
typedef HINET(WINAPI *PFOpenRequest)(HINET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR *, DWORD);
typedef BOOL(WINAPI *PFSend)(HINET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
typedef BOOL(WINAPI *PFReceive)(HINET, LPVOID);
typedef BOOL(WINAPI *PFRead)(HINET, LPVOID, DWORD, LPDWORD);
typedef BOOL(WINAPI *PFClose)(HINET);
typedef BOOL(WINAPI *PFSetOption)(HINET, DWORD, LPVOID, DWORD);
typedef BOOL(WINAPI *PFQuery)(HINET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);

static PFOpen pOpen;
static PFConnect pConnect;
static PFOpenRequest pOpenRequest;
static PFSend pSend;
static PFReceive pReceive;
static PFRead pRead;
static PFClose pClose;
static PFSetOption pSetOption;
static PFQuery pQuery;
static int g_netState; /* 0 unknown, 1 ready, -1 unavailable */

int net_available(void) {
    HMODULE m;
    if (g_netState) return g_netState > 0;
    g_netState = -1;
    if (!os_at_least_win7()) return 0;
    m = LoadLibraryA("winhttp.dll");
    if (!m) return 0;
    pOpen = (PFOpen)GetProcAddress(m, "WinHttpOpen");
    pConnect = (PFConnect)GetProcAddress(m, "WinHttpConnect");
    pOpenRequest = (PFOpenRequest)GetProcAddress(m, "WinHttpOpenRequest");
    pSend = (PFSend)GetProcAddress(m, "WinHttpSendRequest");
    pReceive = (PFReceive)GetProcAddress(m, "WinHttpReceiveResponse");
    pRead = (PFRead)GetProcAddress(m, "WinHttpReadData");
    pClose = (PFClose)GetProcAddress(m, "WinHttpCloseHandle");
    pSetOption = (PFSetOption)GetProcAddress(m, "WinHttpSetOption");
    pQuery = (PFQuery)GetProcAddress(m, "WinHttpQueryHeaders");
    if (pOpen && pConnect && pOpenRequest && pSend && pReceive && pRead && pClose) g_netState = 1;
    return g_netState > 0;
}

static void widen(const char *s, WCHAR *w, int n) {
    MultiByteToWideChar(CP_ACP, 0, s, -1, w, n);
}

/* GET https://host/path into out (NUL-terminated). Returns bytes read, or -1. */
int net_get(const char *host, const char *path, char *out, int max) {
    WCHAR whost[128], wpath[512];
    HINET s = 0, c = 0, r = 0;
    int total = -1;
    if (!net_available()) return -1;
    widen(host, whost, 128);
    widen(path, wpath, 512);
    s = pOpen(L"Larksong-Classic/" VER_STRING, 0 /* default proxy */, NULL, NULL, 0);
    if (!s) goto done;
    if (pSetOption) {
        DWORD protos = 0x00000800 | 0x00000200 | 0x00000080; /* TLS 1.2, 1.1, 1.0 */
        pSetOption(s, 84 /* WINHTTP_OPTION_SECURE_PROTOCOLS */, &protos, sizeof(protos));
    }
    c = pConnect(s, whost, 443, 0);
    if (!c) goto done;
    r = pOpenRequest(c, L"GET", wpath, NULL, NULL, NULL, 0x00800000 /* WINHTTP_FLAG_SECURE */);
    if (!r) goto done;
    if (!pSend(r, NULL, 0, NULL, 0, 0, 0) || !pReceive(r, NULL)) goto done;
    if (pQuery) {
        DWORD code = 0, len = sizeof(code);
        if (pQuery(r, 19 | 0x20000000, NULL, &code, &len, NULL) && (code < 200 || code > 299)) goto done;
    }
    total = 0;
    for (;;) {
        DWORD got = 0;
        if (total >= max - 1) break;
        if (!pRead(r, out + total, (DWORD)(max - 1 - total), &got) || got == 0) break;
        total += (int)got;
    }
    out[total] = 0;
done:
    if (r) pClose(r);
    if (c) pClose(c);
    if (s) pClose(s);
    return total;
}

/* Percent-encodes text for a URL query. */
void url_encode(const char *s, char *out, int max) {
    static const char hex[] = "0123456789ABCDEF";
    int n = 0;
    for (; *s && n < max - 4; s++) {
        unsigned char ch = (unsigned char)*s;
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '.' || ch == '_') {
            out[n++] = (char)ch;
        } else {
            out[n++] = '%';
            out[n++] = hex[ch >> 4];
            out[n++] = hex[ch & 15];
        }
    }
    out[n] = 0;
}
