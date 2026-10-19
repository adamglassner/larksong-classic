#include "core.h"

/* A 2-second "beep beep beep beep" built in memory, so Larksong works without any sound files. */
static const signed char SINE64[64] = {0,12,23,35,46,57,67,76,85,93,100,106,111,115,118,119,120,119,118,115,111,106,100,93,85,76,67,57,46,35,23,12,0,-12,-23,-35,-46,-57,-67,-76,-85,-93,-100,-106,-111,-115,-118,-119,-120,-119,-118,-115,-111,-106,-100,-93,-85,-76,-67,-57,-46,-35,-23,-12};

#define TONE_RATE 22050
#define TONE_SAMPLES (TONE_RATE * 2)
static unsigned char *g_tone; /* RIFF/WAVE in memory */

static void put32(unsigned char *p, DWORD v) { p[0] = (BYTE)v; p[1] = (BYTE)(v >> 8); p[2] = (BYTE)(v >> 16); p[3] = (BYTE)(v >> 24); }
static void put16(unsigned char *p, WORD v) { p[0] = (BYTE)v; p[1] = (BYTE)(v >> 8); }

static void tone_build(void) {
    int i, b;
    unsigned char *d;
    if (g_tone) return;
    g_tone = (unsigned char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 44 + TONE_SAMPLES);
    if (!g_tone) return;
    memcpy(g_tone, "RIFF", 4); put32(g_tone + 4, 36 + TONE_SAMPLES);
    memcpy(g_tone + 8, "WAVEfmt ", 8); put32(g_tone + 16, 16);
    put16(g_tone + 20, 1); put16(g_tone + 22, 1);                /* PCM, mono */
    put32(g_tone + 24, TONE_RATE); put32(g_tone + 28, TONE_RATE); /* 8-bit */
    put16(g_tone + 32, 1); put16(g_tone + 34, 8);
    memcpy(g_tone + 36, "data", 4); put32(g_tone + 40, TONE_SAMPLES);
    d = g_tone + 44;
    for (i = 0; i < TONE_SAMPLES; i++) d[i] = 128;
    for (b = 0; b < 4; b++) {
        int start = b * (TONE_RATE * 22 / 100);
        int len = TONE_RATE * 13 / 100;
        unsigned phase = 0, step = (unsigned)(1047UL * 65536UL / TONE_RATE * 64UL); /* ~1047 Hz */
        for (i = 0; i < len; i++) {
            int env = i < 200 ? i * 256 / 200 : (len - i < 200 ? (len - i) * 256 / 200 : 256);
            int v = SINE64[(phase >> 16) & 63] * env / 256;
            d[start + i] = (unsigned char)(128 + v);
            phase += step;
        }
    }
}

/* ---- volume ---- */
static DWORD g_savedVol;
static int g_volSaved;

void sound_set_volume(int permille) {
    DWORD v;
    if (!g_volSaved) {
        if (waveOutGetVolume((HWAVEOUT)0, &g_savedVol) == MMSYSERR_NOERROR) g_volSaved = 1;
    }
    if (permille < 15) permille = 15;
    if (permille > 1000) permille = 1000;
    /* Squared so the loudness rises evenly. */
    v = (DWORD)(65535UL * (DWORD)permille / 1000UL * (DWORD)permille / 1000UL);
    waveOutSetVolume((HWAVEOUT)0, (v & 0xFFFF) | (v << 16));
}

static void sound_restore_volume(void) {
    if (g_volSaved) {
        waveOutSetVolume((HWAVEOUT)0, g_savedVol);
        g_volSaved = 0;
    }
}

/* Starts a looping sound: the given WAV file, or the built-in tone. */
void sound_start(const char *wav) {
    BOOL ok = FALSE;
    PlaySoundA(NULL, NULL, 0);
    if (wav && file_exists(wav)) {
        ok = PlaySoundA(wav, NULL, SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
    }
    if (!ok) {
        tone_build();
        if (g_tone) PlaySoundA((LPCSTR)g_tone, NULL, SND_MEMORY | SND_ASYNC | SND_LOOP);
        else MessageBeep(MB_ICONEXCLAMATION);
    }
}

void sound_stop(void) {
    PlaySoundA(NULL, NULL, 0);
    sound_restore_volume();
}

/* Plays a sound once (for "Test"), at full volume. */
void sound_test(const char *wav) {
    if (wav && file_exists(wav) && PlaySoundA(wav, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) return;
    tone_build();
    if (g_tone) PlaySoundA((LPCSTR)g_tone, NULL, SND_MEMORY | SND_ASYNC);
}
