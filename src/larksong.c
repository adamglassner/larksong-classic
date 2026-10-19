#include "core.h"

/* sound.c / net.c */
void sound_start(const char *wav);
void sound_stop(void);
void sound_test(const char *wav);
void sound_set_volume(int permille);
int net_available(void);
int net_get(const char *host, const char *path, char *out, int max);
void url_encode(const char *s, char *out, int max);

/* ---------- ids & messages ---------- */
#define WM_TRAY      (WM_APP + 1)
#define WM_REMINDER  (WM_APP + 2)
#define WM_WEATHER   (WM_APP + 3)
#define WM_UPDATE    (WM_APP + 4)

#define TIMER_TICK   1
#define TIMER_RING   2

enum { TAB_CLOCK, TAB_ALARMS, TAB_STOP, TAB_TIMER, TAB_SETTINGS, TAB_COUNT };

#define ID_NAV0        200 /* ..204 */
#define ID_ZONES       300
#define ID_WREFRESH    301
#define ID_ALIST       310
#define ID_AADD        311
#define ID_AEDIT       312
#define ID_ADEL        313
#define ID_ATOGGLE     314
#define ID_ABED        315
#define ID_ASKIP       316
#define ID_ANEXT       317
#define ID_SWSTART     320
#define ID_SWLAP       321
#define ID_SWRESET     322
#define ID_SWLIST      323
#define ID_TMIN        330
#define ID_TMINSPIN    331
#define ID_TSEC        332
#define ID_TSECSPIN    333
#define ID_TSET        334
#define ID_TSTART      335
#define ID_TRESET      336
#define ID_TPRESET0    340 /* ..345 */
#define ID_TSOUND      350
#define ID_TSOUNDBTN   351
#define ID_TSOUNDDEF   352
#define ID_THEME       360
#define ID_USE24       361
#define ID_SHOWSEC     362
#define ID_AUTOSTART   363
#define ID_CELSIUS     364
#define ID_CITY        365
#define ID_CITYSET     366
#define ID_WSTATUS     367
#define ID_UPDCHECK    368
#define ID_UPDOPEN     369
#define ID_UPDSTATUS   370
#define ID_ABOUT       371
#define ID_BACKUP      372
#define ID_RESTORE     373
#define ID_CREDITS     374
#define ID_CHANGES     375
#define ID_SNOOZE      400
#define ID_DISMISS     401
#define ID_TRAY_OPEN   500
#define ID_TRAY_NEW    501
#define ID_TRAY_EXIT   502
#define ID_TRAY_SKIP   503

static const char MAIN_CLASS[] = "LarksongClassicMain";
static const char PANEL_CLASS[] = "LarksongClassicPanel";
static const char RING_CLASS[] = "LarksongClassicRing";
static const char RELEASES_URL[] = "https://github.com/adamglassner/larksong-classic/releases";

static HWND g_main, g_panel, g_ring;
static int g_tab;
static HFONT g_ui, g_big, g_huge, g_small, g_bold;
static HBRUSH g_bgBrush, g_panelBrush;
static NOTIFYICONDATAA g_nid;
static int g_lastStamp;
static int g_snoozeAt[MAX_ALARMS];

/* stopwatch / timer (milliseconds via GetTickCount) */
static int g_swRunning;
static DWORD g_swStart, g_swBase;
static DWORD g_laps[MAX_LAPS];
static int g_lapCount;
static int g_tmRunning;
static DWORD g_tmEnd, g_tmTotal = 300000, g_tmLeft = 300000;

/* weather / update results (filled by worker threads) */
static volatile int g_wxOk, g_wxTemp, g_wxHigh, g_wxLow, g_wxCode, g_wxBusy;
static char g_wxPlace[64];
static char g_wxError[96];
static char g_updText[128];
static char g_updVersion[24];

/* controls on each page */
#define MAX_CTL 64
static HWND g_ctl[MAX_CTL];
static int g_ctlTab[MAX_CTL];
static int g_nctl;
static HWND g_nav[TAB_COUNT];

/* ringing */
static int g_ringKind;   /* 0 alarm, 1 snooze, 2 timer, 3 test */
static int g_ringAlarm;
static DWORD g_ringStart;
static int g_ringGradual, g_ringRamp;

/* ---------- theme ---------- */
typedef struct {
    COLORREF bg, panel, text, muted, accent, face, hand, sec, ledOn, ledOff;
} Palette;

static Palette P;

static void theme_apply(void) {
    switch (S.theme) {
    case TH_DARK:
        P.bg = RGB(32, 32, 32); P.panel = RGB(43, 43, 43); P.text = RGB(240, 240, 240); P.muted = RGB(170, 170, 170);
        P.accent = RGB(96, 205, 255); P.face = RGB(52, 52, 52); P.hand = RGB(240, 240, 240); P.sec = RGB(96, 205, 255);
        break;
    case TH_TERMINAL:
        P.bg = RGB(0, 0, 0); P.panel = RGB(3, 13, 5); P.text = RGB(51, 255, 102); P.muted = RGB(34, 170, 76);
        P.accent = RGB(124, 255, 158); P.face = RGB(0, 0, 0); P.hand = RGB(51, 255, 102); P.sec = RGB(182, 255, 61);
        break;
    case TH_RETRO:
        P.bg = RGB(30, 25, 20); P.panel = RGB(43, 35, 27); P.text = RGB(242, 226, 196); P.muted = RGB(184, 166, 135);
        P.accent = RGB(242, 181, 68); P.face = RGB(242, 226, 196); P.hand = RGB(43, 35, 27); P.sec = RGB(179, 64, 42);
        break;
    case TH_RADIO:
        P.bg = RGB(31, 26, 23); P.panel = RGB(10, 6, 6); P.text = RGB(233, 220, 196); P.muted = RGB(154, 143, 132);
        P.accent = RGB(255, 45, 79); P.face = RGB(10, 6, 6); P.hand = RGB(233, 220, 196); P.sec = RGB(255, 45, 79);
        break;
    default:
        P.bg = GetSysColor(COLOR_BTNFACE); P.panel = GetSysColor(COLOR_WINDOW); P.text = GetSysColor(COLOR_WINDOWTEXT);
        P.muted = GetSysColor(COLOR_GRAYTEXT); P.accent = RGB(0, 0, 160); P.face = RGB(255, 255, 255);
        P.hand = RGB(0, 0, 0); P.sec = RGB(200, 0, 0);
        break;
    }
    P.ledOn = RGB(255, 45, 79);
    P.ledOff = RGB(43, 11, 18);
    if (g_bgBrush) DeleteObject(g_bgBrush);
    if (g_panelBrush) DeleteObject(g_panelBrush);
    g_bgBrush = CreateSolidBrush(P.bg);
    g_panelBrush = CreateSolidBrush(P.panel);
}

static HFONT make_font(int height, int weight, const char *face) {
    return CreateFontA(-height, 0, 0, 0, weight, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

static void fonts_apply(void) {
    const char *face = S.theme == TH_TERMINAL ? "Courier New" : (S.theme == TH_RETRO ? "Impact" : "Arial");
    if (g_big) DeleteObject(g_big);
    if (g_huge) DeleteObject(g_huge);
    if (g_small) DeleteObject(g_small);
    if (g_bold) DeleteObject(g_bold);
    g_huge = make_font(52, FW_BOLD, face);
    g_big = make_font(30, FW_BOLD, face);
    g_bold = make_font(14, FW_BOLD, face);
    g_small = make_font(13, FW_NORMAL, S.theme == TH_TERMINAL ? "Courier New" : "Arial");
}

/* ---------- small drawing helpers ---------- */

static void text_at(HDC dc, int x, int y, const char *s, HFONT f, COLORREF c) {
    HGDIOBJ old = SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    TextOutA(dc, x, y, s, str_len(s));
    SelectObject(dc, old);
}

static int text_width(HDC dc, const char *s, HFONT f) {
    SIZE sz;
    HGDIOBJ old = SelectObject(dc, f);
    GetTextExtentPoint32A(dc, s, str_len(s), &sz);
    SelectObject(dc, old);
    return sz.cx;
}

static void line(HDC dc, int x1, int y1, int x2, int y2, int width, COLORREF c) {
    HPEN pen = CreatePen(PS_SOLID, width, c);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

static void fill_rect(HDC dc, int x, int y, int w, int h, COLORREF c) {
    RECT r;
    HBRUSH b = CreateSolidBrush(c);
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    FillRect(dc, &r, b);
    DeleteObject(b);
}

static void circle(HDC dc, int cx, int cy, int r, COLORREF fill, COLORREF edge, int edgeW) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, edgeW, edge);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pen);
    Ellipse(dc, cx - r, cy - r, cx + r + 1, cy + r + 1);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(pen);
}

/* point on the dial: index 0..59 clockwise from 12, distance r */
static void dial_pt(int cx, int cy, int idx, int r, int *x, int *y) {
    idx = ((idx % 60) + 60) % 60;
    *x = cx + SIN60[idx] * r / 1000;
    *y = cy - SIN60[(idx + 15) % 60] * r / 1000;
}

static void draw_analog(HDC dc, int cx, int cy, int r, int h, int m, int s) {
    int i, x1, y1, x2, y2;
    COLORREF rim = S.theme == TH_RADIO ? RGB(58, 52, 48) : (S.theme == TH_CLASSIC ? RGB(128, 128, 128) : (S.theme == TH_RETRO ? RGB(200, 150, 62) : P.muted));
    circle(dc, cx, cy, r, P.face, rim, S.theme == TH_RADIO ? 6 : (S.theme == TH_RETRO ? 7 : 2));
    if (S.theme == TH_RADIO) circle(dc, cx, cy, r * 82 / 100, P.face, RGB(194, 86, 43), 1);
    for (i = 0; i < 60; i++) {
        int major = i % 5 == 0;
        if (S.theme == TH_TERMINAL) {
            int sz = major ? 4 : 1;
            dial_pt(cx, cy, i, r * 85 / 100, &x1, &y1);
            fill_rect(dc, x1 - sz / 2, y1 - sz / 2, sz + 1, sz + 1, major ? P.hand : P.muted);
        } else if (major) {
            dial_pt(cx, cy, i, r * 90 / 100, &x1, &y1);
            dial_pt(cx, cy, i, r * (i % 15 == 0 ? 74 : 80) / 100, &x2, &y2);
            line(dc, x1, y1, x2, y2, i % 15 == 0 ? 3 : 2, P.hand);
        }
    }
    dial_pt(cx, cy, (h % 12) * 5 + m / 12, r * 50 / 100, &x2, &y2);
    line(dc, cx, cy, x2, y2, r / 14 + 2, P.hand);
    dial_pt(cx, cy, m, r * 75 / 100, &x2, &y2);
    line(dc, cx, cy, x2, y2, r / 22 + 2, P.hand);
    dial_pt(cx, cy, s + 30, r * 15 / 100, &x1, &y1);
    dial_pt(cx, cy, s, r * 82 / 100, &x2, &y2);
    line(dc, x1, y1, x2, y2, 2, P.sec);
    circle(dc, cx, cy, r / 18 + 2, P.sec, P.sec, 1);
}

/* Seven-segment LED digits for the Radio theme. h = digit height. Returns width drawn. */
static const char *SEGS[11] = {"abcdef", "bc", "abged", "abgcd", "fgbc", "afgcd", "afgedc", "abc", "abcdefg", "abcdfg", ""};

static void seg_poly(HDC dc, int ox, int oy, int h, int x, int y, int w, int hh) {
    POINT pt[4];
    /* units: digit is 50 x 90; slant to the right going up */
    pt[0].x = ox + (x + (90 - y) / 10) * h / 90;          pt[0].y = oy + y * h / 90;
    pt[1].x = ox + (x + w + (90 - y) / 10) * h / 90;      pt[1].y = oy + y * h / 90;
    pt[2].x = ox + (x + w + (90 - y - hh) / 10) * h / 90; pt[2].y = oy + (y + hh) * h / 90;
    pt[3].x = ox + (x + (90 - y - hh) / 10) * h / 90;     pt[3].y = oy + (y + hh) * h / 90;
    Polygon(dc, pt, 4);
}

static int draw_led(HDC dc, int x, int y, int h, const char *text) {
    HBRUSH on = CreateSolidBrush(P.ledOn), off = CreateSolidBrush(P.ledOff);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    HGDIOBJ oldBrush = SelectObject(dc, off);
    int startX = x;
    for (; *text; text++) {
        char ch = *text;
        if (ch == ':') {
            SelectObject(dc, on);
            Ellipse(dc, x + h * 6 / 90, y + h * 26 / 90, x + h * 14 / 90, y + h * 34 / 90);
            Ellipse(dc, x + h * 3 / 90, y + h * 58 / 90, x + h * 11 / 90, y + h * 66 / 90);
            x += h * 18 / 90;
        } else {
            const char *lit = (ch >= '0' && ch <= '9') ? SEGS[ch - '0'] : SEGS[10];
            const char *segs = "abcdefg";
            int i;
            for (i = 0; i < 7; i++) {
                const char *q = lit;
                int isOn = 0;
                while (*q) { if (*q == segs[i]) isOn = 1; q++; }
                SelectObject(dc, isOn ? on : off);
                switch (segs[i]) {
                case 'a': seg_poly(dc, x, y, h, 10, 2, 30, 7); break;
                case 'b': seg_poly(dc, x, y, h, 41, 10, 7, 32); break;
                case 'c': seg_poly(dc, x, y, h, 41, 48, 7, 32); break;
                case 'd': seg_poly(dc, x, y, h, 10, 81, 30, 7); break;
                case 'e': seg_poly(dc, x, y, h, 2, 48, 7, 32); break;
                case 'f': seg_poly(dc, x, y, h, 2, 10, 7, 32); break;
                case 'g': seg_poly(dc, x, y, h, 10, 41, 30, 7); break;
                }
            }
            x += h * 60 / 90;
        }
    }
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(on);
    DeleteObject(off);
    return x - startX;
}

/* A retro flip-clock tile with a split line across the middle. Returns its width. */
static int flip_tile(HDC dc, int x, int y, const char *text, int small) {
    HFONT f = small ? g_big : g_huge;
    int tw = text_width(dc, text, f), w = tw + 14, h = small ? 40 : 60;
    HBRUSH b = CreateSolidBrush(RGB(43, 35, 27));
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(74, 61, 46));
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pen);
    RoundRect(dc, x, y, x + w, y + h, 10, 10);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(pen);
    text_at(dc, x + 7, y + (small ? 3 : 2), text, f, small ? P.accent : P.text);
    fill_rect(dc, x + 1, y + h / 2, w - 2, 2, RGB(30, 25, 20));
    return w;
}

static const char *weather_text(int code) {
    if (code == 0) return "Clear";
    if (code == 1) return "Mostly clear";
    if (code == 2) return "Partly cloudy";
    if (code == 3) return "Overcast";
    if (code == 45 || code == 48) return "Fog";
    if (code >= 51 && code <= 57) return "Drizzle";
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return "Rain";
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return "Snow";
    if (code >= 95) return "Thunderstorms";
    return "";
}

/* The local minute stamp when alarm i next rings, leaving out a ring dismissed early; -1 if none. */
static int alarm_next_stamp(int i) {
    int d, now = local_stamp();
    if (!A[i].used || !A[i].enabled) return -1;
    for (d = 0; d < 16; d++) {
        int day = now / 1440 + d;
        int t = day * 1440 + A[i].hour * 60 + A[i].minute;
        if (t <= now || t <= A[i].skipUntil) continue;
        if (A[i].days && !(A[i].days & (1 << weekday_from_days(day)))) continue;
        return t;
    }
    return -1;
}

/* The alarm that rings next, or -1. */
static int soonest_alarm(void) {
    int i, best = -1, bestT = 0;
    for (i = 0; i < MAX_ALARMS; i++) {
        int t = alarm_next_stamp(i);
        if (t >= 0 && (best < 0 || t < bestT)) { best = i; bestT = t; }
    }
    return best;
}

static int next_alarm_minutes(void) {
    int i = soonest_alarm();
    return i < 0 ? -1 : alarm_next_stamp(i) - local_stamp();
}

/* "Tue 6:00 AM" for a local minute stamp. */
static void stamp_text(char *out, int stamp) {
    static const char *DN[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char t[16];
    format_time(t, (stamp % 1440) / 60, stamp % 60, S.use24);
    wsprintfA(out, "%s %s", DN[weekday_from_days(stamp / 1440)], t);
}

/* ---------- panel painting ---------- */

static void paint_panel(HWND hwnd, HDC dc) {
    RECT rc;
    SYSTEMTIME t;
    char buf[160], buf2[64];
    int w, h;
    GetClientRect(hwnd, &rc);
    w = rc.right; h = rc.bottom;
    fill_rect(dc, 0, 0, w, h, P.panel);
    if (S.theme == TH_RADIO) {
        int yy;
        /* woodgrain strip */
        for (yy = 0; yy < 14; yy += 4) fill_rect(dc, 0, yy, w, 3, yy % 8 ? RGB(110, 72, 43) : RGB(90, 58, 34));
        fill_rect(dc, 0, 16, w, 2, RGB(194, 86, 43));
    }
    GetLocalTime(&t);

    if (g_tab == TAB_CLOCK) {
        int i, top = S.theme == TH_RADIO ? 24 : 12;
        draw_analog(dc, 105, top + 95, 90, t.wHour, t.wMinute, t.wSecond);
        format_hm(buf, t.wHour, t.wMinute, S.use24);
        if (S.showSec) { wsprintfA(buf2, ":%02d", t.wSecond); str_cat(buf, buf2, sizeof(buf)); }
        if (S.theme == TH_RADIO) {
            int lw = draw_led(dc, 218, top + 16, 48, buf);
            if (!S.use24) text_at(dc, 222 + lw, top + 46, t.wHour < 12 ? "AM" : "PM", g_bold, P.muted);
        } else if (S.theme == TH_RETRO) {
            int x = 216, hh = S.use24 ? t.wHour : (t.wHour % 12 == 0 ? 12 : t.wHour % 12);
            wsprintfA(buf2, "%02d", hh);
            x += flip_tile(dc, x, top + 12, buf2, 0) + 6;
            wsprintfA(buf2, "%02d", t.wMinute);
            x += flip_tile(dc, x, top + 12, buf2, 0) + 6;
            if (S.showSec) { wsprintfA(buf2, "%02d", t.wSecond); x += flip_tile(dc, x, top + 30, buf2, 1) + 6; }
            if (!S.use24) text_at(dc, x, top + 46, t.wHour < 12 ? "AM" : "PM", g_bold, P.accent);
        } else {
            if (S.theme == TH_TERMINAL) text_at(dc, 218, top, "C:\\> TIME", g_small, P.muted);
            text_at(dc, 214, top + 14, buf, S.showSec ? g_big : g_huge, P.text);
            if (!S.use24) text_at(dc, 220 + text_width(dc, buf, S.showSec ? g_big : g_huge), top + (S.showSec ? 26 : 44), t.wHour < 12 ? "AM" : "PM", g_bold, P.muted);
        }
        {
            static const char *DAYS[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
            static const char *MONTHS[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
            wsprintfA(buf, "%s, %s %d, %d", DAYS[t.wDayOfWeek], MONTHS[t.wMonth - 1], t.wDay, t.wYear);
            text_at(dc, 216, top + 76, buf, g_small, P.muted);
        }
        {
            int n = next_alarm_minutes();
            if (n >= 0) {
                if (n >= 1440) wsprintfA(buf, "Next alarm in %d d %d h", n / 1440, (n % 1440) / 60);
                else wsprintfA(buf, "Next alarm in %d h %d min", n / 60, n % 60);
            } else {
                str_copy(buf, "No alarms on", sizeof(buf));
            }
            text_at(dc, 216, top + 98, buf, g_small, S.theme == TH_RADIO ? P.ledOn : P.accent);
        }
        /* weather */
        if (!net_available()) {
            str_copy(buf, "Weather: needs Windows 7 or later", sizeof(buf));
        } else if (!S.haveLoc) {
            str_copy(buf, "Weather: set your city in Settings", sizeof(buf));
        } else if (g_wxOk) {
            wsprintfA(buf, "%s: %d%s  %s  (H %d  L %d)", g_wxPlace, g_wxTemp, S.celsius ? "\xB0" "C" : "\xB0" "F", weather_text(g_wxCode), g_wxHigh, g_wxLow);
        } else if (g_wxBusy) {
            str_copy(buf, "Getting the weather...", sizeof(buf));
        } else {
            wsprintfA(buf, "Weather: %s", g_wxError[0] ? g_wxError : "not available right now");
        }
        text_at(dc, 216, top + 126, buf, g_small, P.text);

        /* world clocks: two columns */
        fill_rect(dc, 10, top + 196, w - 20, 1, P.muted);
        text_at(dc, 12, top + 202, S.theme == TH_TERMINAL ? "# world clocks" : "World clocks", g_bold, P.muted);
        if (S.zoneCount == 0) text_at(dc, 12, top + 224, "Click \"World clocks...\" to add cities.", g_small, P.muted);
        for (i = 0; i < S.zoneCount; i++) {
            int zh, zm, dd, col = i % 2, row = i / 2;
            int x = 12 + col * (w / 2), y = top + 224 + row * 22;
            zone_time(S.zones[i], &zh, &zm, &dd);
            format_time(buf2, zh, zm, S.use24);
            wsprintfA(buf, "%s  %s%s", CITIES[S.zones[i]].name, buf2, dd > 0 ? " +1" : (dd < 0 ? " -1" : ""));
            text_at(dc, x, y, buf, g_small, S.theme == TH_RADIO ? P.ledOn : P.text);
        }
    } else if (g_tab == TAB_ALARMS) {
        int n = next_alarm_minutes();
        int top = S.theme == TH_RADIO ? 22 : 8;
        if (n >= 0) {
            if (n >= 1440) wsprintfA(buf, "Next alarm in %d day(s) %d hr", n / 1440, (n % 1440) / 60);
            else wsprintfA(buf, "Next alarm in %d hr %d min", n / 60, n % 60);
        } else {
            str_copy(buf, "No alarms are on", sizeof(buf));
        }
        text_at(dc, 12, top, buf, g_bold, S.theme == TH_RADIO ? P.ledOn : P.text);
        if (S.bedOn) {
            format_time(buf2, S.bedH, S.bedM, S.use24);
            wsprintfA(buf, "Bedtime %s, reminder %d min before", buf2, S.remind);
        } else {
            str_copy(buf, "Bedtime reminder is off", sizeof(buf));
        }
        text_at(dc, 12, top + 20, buf, g_small, P.muted);
    } else if (g_tab == TAB_STOP || g_tab == TAB_TIMER) {
        DWORD ms;
        int top = S.theme == TH_RADIO ? 28 : 18;
        if (g_tab == TAB_STOP) {
            ms = g_swBase + (g_swRunning ? GetTickCount() - g_swStart : 0);
            wsprintfA(buf, "%02d:%02d", (int)(ms / 60000), (int)((ms / 1000) % 60));
            wsprintfA(buf2, "%02d", (int)((ms / 10) % 100));
        } else {
            ms = g_tmRunning ? (GetTickCount() < g_tmEnd ? g_tmEnd - GetTickCount() : 0) : g_tmLeft;
            ms += 999;
            if (ms / 3600000) wsprintfA(buf, "%d:%02d:%02d", (int)(ms / 3600000), (int)((ms / 60000) % 60), (int)((ms / 1000) % 60));
            else wsprintfA(buf, "%02d:%02d", (int)(ms / 60000), (int)((ms / 1000) % 60));
            buf2[0] = 0;
        }
        if (S.theme == TH_RADIO) {
            int lw = draw_led(dc, 20, top, 64, buf);
            if (buf2[0]) draw_led(dc, 30 + lw, top + 30, 34, buf2);
        } else {
            text_at(dc, 20, top - 6, buf, g_huge, P.text);
            if (buf2[0]) text_at(dc, 26 + text_width(dc, buf, g_huge), top + 18, buf2, g_big, P.accent);
        }
        if (g_tab == TAB_TIMER) {
            char tot[32];
            DWORD tt = g_tmTotal;
            wsprintfA(tot, "of %d:%02d", (int)(tt / 60000), (int)((tt / 1000) % 60));
            text_at(dc, w - 110, top + 30, tot, g_small, P.muted);
            /* progress bar */
            {
                DWORD left = g_tmRunning ? (GetTickCount() < g_tmEnd ? g_tmEnd - GetTickCount() : 0) : g_tmLeft;
                int bw = w - 40;
                int fillw = g_tmTotal ? (int)((left / 100) * (DWORD)bw / (g_tmTotal / 100 + 1)) : 0;
                fill_rect(dc, 20, h - 18, bw, 6, S.theme == TH_CLASSIC ? RGB(220, 220, 220) : P.bg);
                fill_rect(dc, 20, h - 18, fillw, 6, S.theme == TH_RADIO ? P.ledOn : P.accent);
            }
        } else {
            text_at(dc, w - 140, top + 30, g_swRunning ? "Running" : (g_swBase ? "Paused" : "Ready"), g_small, P.muted);
        }
    }
}

static LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        RECT rc;
        HDC dc = BeginPaint(hwnd, &ps);
        HDC mem;
        HBITMAP bmp;
        HGDIOBJ old;
        GetClientRect(hwnd, &rc);
        mem = CreateCompatibleDC(dc);
        bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        old = SelectObject(mem, bmp);
        paint_panel(hwnd, mem);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ---------- page controls ---------- */

static HWND add_ctl(int tab, const char *cls, const char *text, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExA(0, cls, text, WS_CHILD | style, x, y, w, h, g_main, (HMENU)(INT_PTR)id, g_inst, NULL);
    SendMessageA(c, WM_SETFONT, (WPARAM)g_ui, TRUE);
    if (g_nctl < MAX_CTL) { g_ctl[g_nctl] = c; g_ctlTab[g_nctl] = tab; g_nctl++; }
    return c;
}

static HWND ctl(int id) { return GetDlgItem(g_main, id); }

/* The line under the alarm list: when the next alarm rings, and any ring dismissed early. */
static void alarms_next_text(void) {
    char text[160], a[32], b[32];
    int i, now = local_stamp(), skipped = -1;
    for (i = 0; i < MAX_ALARMS; i++) {
        if (A[i].used && A[i].enabled && A[i].skipUntil > now && (skipped < 0 || A[i].skipUntil < A[skipped].skipUntil)) skipped = i;
    }
    i = soonest_alarm();
    if (i < 0) str_copy(text, "No alarms on.", sizeof(text));
    else {
        stamp_text(a, alarm_next_stamp(i));
        wsprintfA(text, "Next alarm: %s%s%s", a, A[i].label[0] ? " - " : "", A[i].label);
    }
    if (skipped >= 0) {
        stamp_text(b, A[skipped].skipUntil);
        str_cat(text, "   (dismissed ", sizeof(text));
        str_cat(text, b, sizeof(text));
        str_cat(text, ")", sizeof(text));
    }
    SetWindowTextA(ctl(ID_ANEXT), text);
}

static void alarms_refresh(void) {
    HWND lb = ctl(ID_ALIST);
    int i, sel = (int)SendMessageA(lb, LB_GETCURSEL, 0, 0);
    SendMessageA(lb, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < MAX_ALARMS; i++) {
        char t[16], d[40], line[160];
        int idx;
        if (!A[i].used) continue;
        format_time(t, A[i].hour, A[i].minute, S.use24);
        days_text(d, A[i].days);
        wsprintfA(line, "%s\t%s\t%s\t%s", t, !A[i].enabled ? "Off" : A[i].skipUntil > local_stamp() ? "Skip" : "On", d,
                  A[i].label[0] ? A[i].label : "Alarm");
        idx = (int)SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)line);
        SendMessageA(lb, LB_SETITEMDATA, (WPARAM)idx, (LPARAM)i);
    }
    if (sel >= 0) SendMessageA(lb, LB_SETCURSEL, (WPARAM)sel, 0);
    alarms_next_text();
    InvalidateRect(g_panel, NULL, FALSE);
}

static int alarms_selected(void) {
    HWND lb = ctl(ID_ALIST);
    int sel = (int)SendMessageA(lb, LB_GETCURSEL, 0, 0);
    if (sel < 0) return -1;
    return (int)SendMessageA(lb, LB_GETITEMDATA, (WPARAM)sel, 0);
}

static void laps_refresh(void) {
    HWND lb = ctl(ID_SWLIST);
    int i;
    DWORD total = 0;
    SendMessageA(lb, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < g_lapCount; i++) total += g_laps[i];
    for (i = g_lapCount - 1; i >= 0; i--) {
        char line[96];
        DWORD l = g_laps[i];
        wsprintfA(line, "Lap %d\t%02d:%02d.%02d\t%02d:%02d.%02d", i + 1,
                  (int)(l / 60000), (int)((l / 1000) % 60), (int)((l / 10) % 100),
                  (int)(total / 60000), (int)((total / 1000) % 60), (int)((total / 10) % 100));
        SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)line);
        total -= l;
    }
}

static void sound_label(char *out, const char *path) {
    int i, start = 0;
    if (!path[0]) { str_copy(out, "Larksong alarm (built in)", 64); return; }
    for (i = 0; path[i]; i++) if (path[i] == '\\') start = i + 1;
    str_copy(out, path + start, 64);
}

static void settings_refresh(void) {
    char buf[200], os[32];
    SendMessageA(ctl(ID_THEME), CB_SETCURSEL, (WPARAM)S.theme, 0);
    CheckDlgButton(g_main, ID_USE24, S.use24 ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(g_main, ID_SHOWSEC, S.showSec ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(g_main, ID_AUTOSTART, autostart_get() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(g_main, ID_CELSIUS, S.celsius ? BST_CHECKED : BST_UNCHECKED);
    SetWindowTextA(ctl(ID_CITY), S.city);
    if (!net_available()) {
        SetWindowTextA(ctl(ID_WSTATUS), "Weather and update checks need Windows 7 or later (with internet).");
        EnableWindow(ctl(ID_CITY), FALSE);
        EnableWindow(ctl(ID_CITYSET), FALSE);
        EnableWindow(ctl(ID_UPDCHECK), FALSE);
        SetWindowTextA(ctl(ID_UPDSTATUS), "To update, download the newest Larksong Classic on another computer.");
    } else if (S.haveLoc) {
        wsprintfA(buf, "Weather for %s.", S.city);
        SetWindowTextA(ctl(ID_WSTATUS), buf);
    } else {
        SetWindowTextA(ctl(ID_WSTATUS), "Type your city and click Set.");
    }
    if (g_updText[0]) SetWindowTextA(ctl(ID_UPDSTATUS), g_updText);
    os_name(os);
    wsprintfA(buf, "Larksong Classic %s by Adam Glassner, running on %s.\r\nSettings are saved in larksong.ini. Alarms ring while Larksong is running (it waits in the notification area when closed).", VER_STRING, os);
    SetWindowTextA(ctl(ID_ABOUT), buf);
    SetWindowTextA(ctl(ID_TSOUND), "");
    sound_label(buf, S.timerSound);
    SetWindowTextA(ctl(ID_TSOUND), buf);
}

static int panel_height(int tab) {
    switch (tab) {
    case TAB_CLOCK: return 320;
    case TAB_ALARMS: return S.theme == TH_RADIO ? 64 : 50;
    case TAB_STOP: case TAB_TIMER: return 120;
    }
    return 0;
}

static void show_tab(int tab) {
    int i, ph;
    g_tab = tab;
    for (i = 0; i < TAB_COUNT; i++) SendMessageA(g_nav[i], BM_SETCHECK, i == tab ? BST_CHECKED : BST_UNCHECKED, 0);
    for (i = 0; i < g_nctl; i++) ShowWindow(g_ctl[i], g_ctlTab[i] == tab ? SW_SHOW : SW_HIDE);
    ph = panel_height(tab);
    if (ph) {
        SetWindowPos(g_panel, NULL, 10, 46, 460, ph, SWP_NOZORDER);
        ShowWindow(g_panel, SW_SHOW);
        InvalidateRect(g_panel, NULL, FALSE);
    } else {
        ShowWindow(g_panel, SW_HIDE);
    }
    if (tab == TAB_ALARMS) alarms_refresh();
    if (tab == TAB_SETTINGS) settings_refresh();
    InvalidateRect(g_main, NULL, TRUE);
}

static void create_pages(void) {
    int i;
    static const char *NAV[TAB_COUNT] = {"Clock", "Alarms", "Stopwatch", "Timer", "Settings"};
    static const int PRESETS[6] = {1, 3, 5, 10, 15, 30};
    HWND c;
    for (i = 0; i < TAB_COUNT; i++) {
        g_nav[i] = CreateWindowExA(0, "BUTTON", NAV[i], WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | BS_PUSHLIKE | (i == 0 ? WS_GROUP : 0),
                                   10 + i * 92, 8, 88, 28, g_main, (HMENU)(INT_PTR)(ID_NAV0 + i), g_inst, NULL);
        SendMessageA(g_nav[i], WM_SETFONT, (WPARAM)g_ui, TRUE);
    }

    /* Clock */
    add_ctl(TAB_CLOCK, "BUTTON", "World clocks...", WS_TABSTOP | BS_PUSHBUTTON, 10, 376, 140, 28, ID_ZONES);
    add_ctl(TAB_CLOCK, "BUTTON", "Refresh weather", WS_TABSTOP | BS_PUSHBUTTON, 160, 376, 140, 28, ID_WREFRESH);

    /* Alarms */
    c = add_ctl(TAB_ALARMS, "LISTBOX", "", WS_TABSTOP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_USETABSTOPS | LBS_NOINTEGRALHEIGHT, 10, 118, 460, 312, ID_ALIST);
    add_ctl(TAB_ALARMS, "BUTTON", "Dis&miss next ring...", WS_TABSTOP | BS_PUSHBUTTON, 10, 438, 150, 28, ID_ASKIP);
    add_ctl(TAB_ALARMS, "STATIC", "", SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE, 170, 438, 300, 28, ID_ANEXT);
    {
        int stops[3] = {48, 76, 160};
        SendMessageA(c, LB_SETTABSTOPS, 3, (LPARAM)stops);
    }
    add_ctl(TAB_ALARMS, "BUTTON", "&Add...", WS_TABSTOP | BS_PUSHBUTTON, 10, 474, 86, 28, ID_AADD);
    add_ctl(TAB_ALARMS, "BUTTON", "&Edit...", WS_TABSTOP | BS_PUSHBUTTON, 102, 474, 86, 28, ID_AEDIT);
    add_ctl(TAB_ALARMS, "BUTTON", "&Delete", WS_TABSTOP | BS_PUSHBUTTON, 194, 474, 86, 28, ID_ADEL);
    add_ctl(TAB_ALARMS, "BUTTON", "On / &Off", WS_TABSTOP | BS_PUSHBUTTON, 286, 474, 86, 28, ID_ATOGGLE);
    add_ctl(TAB_ALARMS, "BUTTON", "&Bedtime...", WS_TABSTOP | BS_PUSHBUTTON, 378, 474, 92, 28, ID_ABED);

    /* Stopwatch */
    add_ctl(TAB_STOP, "BUTTON", "&Start", WS_TABSTOP | BS_PUSHBUTTON, 10, 176, 110, 30, ID_SWSTART);
    add_ctl(TAB_STOP, "BUTTON", "&Lap", WS_TABSTOP | BS_PUSHBUTTON, 130, 176, 110, 30, ID_SWLAP);
    add_ctl(TAB_STOP, "BUTTON", "&Reset", WS_TABSTOP | BS_PUSHBUTTON, 250, 176, 110, 30, ID_SWRESET);
    c = add_ctl(TAB_STOP, "LISTBOX", "", WS_BORDER | WS_VSCROLL | LBS_USETABSTOPS | LBS_NOINTEGRALHEIGHT, 10, 216, 460, 286, ID_SWLIST);
    {
        int stops[2] = {60, 140};
        SendMessageA(c, LB_SETTABSTOPS, 2, (LPARAM)stops);
    }

    /* Timer */
    add_ctl(TAB_TIMER, "STATIC", "Minutes:", SS_LEFT, 10, 182, 56, 18, -1);
    add_ctl(TAB_TIMER, "EDIT", "5", WS_TABSTOP | WS_BORDER | ES_NUMBER, 70, 178, 50, 22, ID_TMIN);
    c = CreateUpDownControl(WS_CHILD | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0, g_main, ID_TMINSPIN, g_inst, ctl(ID_TMIN), 999, 0, 5);
    if (g_nctl < MAX_CTL) { g_ctl[g_nctl] = c; g_ctlTab[g_nctl++] = TAB_TIMER; }
    add_ctl(TAB_TIMER, "STATIC", "Seconds:", SS_LEFT, 140, 182, 56, 18, -1);
    add_ctl(TAB_TIMER, "EDIT", "0", WS_TABSTOP | WS_BORDER | ES_NUMBER, 200, 178, 50, 22, ID_TSEC);
    c = CreateUpDownControl(WS_CHILD | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_WRAP, 0, 0, 0, 0, g_main, ID_TSECSPIN, g_inst, ctl(ID_TSEC), 59, 0, 0);
    if (g_nctl < MAX_CTL) { g_ctl[g_nctl] = c; g_ctlTab[g_nctl++] = TAB_TIMER; }
    add_ctl(TAB_TIMER, "BUTTON", "Set", WS_TABSTOP | BS_PUSHBUTTON, 280, 176, 70, 26, ID_TSET);
    add_ctl(TAB_TIMER, "BUTTON", "&Start", WS_TABSTOP | BS_PUSHBUTTON, 10, 216, 110, 30, ID_TSTART);
    add_ctl(TAB_TIMER, "BUTTON", "&Reset", WS_TABSTOP | BS_PUSHBUTTON, 130, 216, 110, 30, ID_TRESET);
    add_ctl(TAB_TIMER, "STATIC", "Quick start:", SS_LEFT, 10, 264, 100, 18, -1);
    for (i = 0; i < 6; i++) {
        char t[16];
        wsprintfA(t, "%d min", PRESETS[i]);
        add_ctl(TAB_TIMER, "BUTTON", t, WS_TABSTOP | BS_PUSHBUTTON, 10 + i * 76, 286, 70, 26, ID_TPRESET0 + i);
    }
    add_ctl(TAB_TIMER, "STATIC", "Timer sound:", SS_LEFT, 10, 332, 100, 18, -1);
    add_ctl(TAB_TIMER, "STATIC", "", SS_LEFT | SS_NOPREFIX, 110, 332, 200, 18, ID_TSOUND);
    add_ctl(TAB_TIMER, "BUTTON", "Browse...", WS_TABSTOP | BS_PUSHBUTTON, 314, 328, 76, 26, ID_TSOUNDBTN);
    add_ctl(TAB_TIMER, "BUTTON", "Default", WS_TABSTOP | BS_PUSHBUTTON, 394, 328, 76, 26, ID_TSOUNDDEF);

    /* Settings */
    add_ctl(TAB_SETTINGS, "STATIC", "Theme:", SS_LEFT, 10, 56, 100, 18, -1);
    c = add_ctl(TAB_SETTINGS, "COMBOBOX", "", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 120, 52, 180, 200, ID_THEME);
    SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"Classic");
    SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"Dark");
    SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"Terminal (green on black)");
    SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"Clock radio");
    SendMessageA(c, CB_ADDSTRING, 0, (LPARAM)"Retro");
    add_ctl(TAB_SETTINGS, "BUTTON", "Use 24-hour time", WS_TABSTOP | BS_AUTOCHECKBOX, 10, 88, 300, 20, ID_USE24);
    add_ctl(TAB_SETTINGS, "BUTTON", "Show seconds", WS_TABSTOP | BS_AUTOCHECKBOX, 10, 112, 300, 20, ID_SHOWSEC);
    add_ctl(TAB_SETTINGS, "BUTTON", "Start Larksong when Windows starts", WS_TABSTOP | BS_AUTOCHECKBOX, 10, 136, 360, 20, ID_AUTOSTART);
    add_ctl(TAB_SETTINGS, "BUTTON", "Show temperatures in Celsius", WS_TABSTOP | BS_AUTOCHECKBOX, 10, 160, 360, 20, ID_CELSIUS);
    add_ctl(TAB_SETTINGS, "STATIC", "Weather city:", SS_LEFT, 10, 202, 100, 18, -1);
    add_ctl(TAB_SETTINGS, "EDIT", "", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 120, 198, 250, 22, ID_CITY);
    add_ctl(TAB_SETTINGS, "BUTTON", "Set", WS_TABSTOP | BS_PUSHBUTTON, 380, 196, 70, 26, ID_CITYSET);
    add_ctl(TAB_SETTINGS, "STATIC", "", SS_LEFT | SS_NOPREFIX, 10, 228, 460, 34, ID_WSTATUS);
    add_ctl(TAB_SETTINGS, "BUTTON", "Check for &updates", WS_TABSTOP | BS_PUSHBUTTON, 10, 272, 150, 28, ID_UPDCHECK);
    add_ctl(TAB_SETTINGS, "BUTTON", "Download page", WS_TABSTOP | BS_PUSHBUTTON, 170, 272, 130, 28, ID_UPDOPEN);
    add_ctl(TAB_SETTINGS, "STATIC", "", SS_LEFT | SS_NOPREFIX, 10, 306, 460, 34, ID_UPDSTATUS);
    add_ctl(TAB_SETTINGS, "BUTTON", "&Back up alarms...", WS_TABSTOP | BS_PUSHBUTTON, 10, 350, 150, 28, ID_BACKUP);
    add_ctl(TAB_SETTINGS, "BUTTON", "R&estore alarms...", WS_TABSTOP | BS_PUSHBUTTON, 170, 350, 150, 28, ID_RESTORE);
    add_ctl(TAB_SETTINGS, "BUTTON", "&Credits...", WS_TABSTOP | BS_PUSHBUTTON, 10, 388, 150, 28, ID_CREDITS);
    add_ctl(TAB_SETTINGS, "BUTTON", "&What's new...", WS_TABSTOP | BS_PUSHBUTTON, 170, 388, 150, 28, ID_CHANGES);
    add_ctl(TAB_SETTINGS, "STATIC", "", SS_LEFT | SS_NOPREFIX, 10, 426, 460, 76, ID_ABOUT);
}

/* ---------- the alarm engine ---------- */

static void ring_open(int kind, int alarmIndex);

static void check_minute(int stamp) {
    int i, day = stamp / 1440, mod = stamp % 1440, dow = weekday_from_days(day);
    for (i = 0; i < MAX_ALARMS; i++) {
        Alarm *a = &A[i];
        if (!a->used || !a->enabled) continue;
        if (a->hour * 60 + a->minute != mod) continue;
        if (stamp <= a->skipUntil) continue; /* dismissed ahead of time */
        if (a->days && !(a->days & (1 << dow))) continue;
        if (!a->days) { a->enabled = 0; store_save(); }
        ring_open(0, i);
        if (g_tab == TAB_ALARMS) alarms_refresh();
    }
    for (i = 0; i < MAX_ALARMS; i++) {
        if (g_snoozeAt[i] && g_snoozeAt[i] == stamp) {
            g_snoozeAt[i] = 0;
            if (A[i].used) ring_open(1, i);
        }
    }
    if (S.bedOn) {
        /* Remind `remind` minutes before bedtime, on the nights that are ticked. */
        int bedStamp = stamp + S.remind;
        int bedDay = bedStamp / 1440;
        if (bedStamp % 1440 == S.bedH * 60 + S.bedM && (S.bedDays & (1 << weekday_from_days(bedDay)))) {
            PostMessageA(g_main, WM_REMINDER, 0, 0);
        }
    }
}

static void tick(void) {
    int now = local_stamp();
    if (!g_lastStamp) g_lastStamp = now;
    if (now != g_lastStamp) {
        int s = g_lastStamp + 1;
        if (now < g_lastStamp || now - g_lastStamp > 30) s = now; /* clock changed, or PC slept a long time */
        g_lastStamp = now;
        for (; s <= now; s++) check_minute(s);
        alarms_next_text();
    }
    if (g_tmRunning && GetTickCount() >= g_tmEnd) {
        g_tmRunning = 0;
        g_tmLeft = g_tmTotal;
        SetWindowTextA(ctl(ID_TSTART), "&Start");
        ring_open(2, -1);
    }
}

/* ---------- ringing window ---------- */

static void ring_close(int snooze) {
    if (snooze && (g_ringKind == 0 || g_ringKind == 1) && g_ringAlarm >= 0 && A[g_ringAlarm].used) {
        int at = local_stamp() + A[g_ringAlarm].snooze;
        g_snoozeAt[g_ringAlarm] = at;
    }
    sound_stop();
    if (g_ring) {
        KillTimer(g_ring, TIMER_RING);
        DestroyWindow(g_ring);
        g_ring = NULL;
    }
    InvalidateRect(g_panel, NULL, FALSE);
}

static void ring_paint(HWND hwnd, HDC dc) {
    RECT rc;
    SYSTEMTIME t;
    char buf[96];
    const char *title;
    GetClientRect(hwnd, &rc);
    fill_rect(dc, 0, 0, rc.right, rc.bottom, P.panel);
    if (S.theme == TH_RADIO) fill_rect(dc, 0, 0, rc.right, 6, RGB(110, 72, 43));
    GetLocalTime(&t);
    if (g_ringKind == 2) title = "Time's up";
    else if (g_ringKind == 3) title = "Test";
    else title = (g_ringAlarm >= 0 && A[g_ringAlarm].label[0]) ? A[g_ringAlarm].label : "Alarm";
    text_at(dc, 16, 12, title, g_bold, P.muted);
    format_hm(buf, t.wHour, t.wMinute, S.use24);
    if (S.theme == TH_RADIO) draw_led(dc, 16, 36, 50, buf);
    else text_at(dc, 14, 30, buf, g_huge, P.text);
    if (!S.use24) text_at(dc, 210, 70, t.wHour < 12 ? "AM" : "PM", g_bold, P.muted);
    if (g_ringGradual) {
        DWORD el = GetTickCount() - g_ringStart;
        int pm = g_ringRamp ? (int)(el / (DWORD)g_ringRamp) : 1000;
        if (pm > 1000) pm = 1000;
        wsprintfA(buf, "Volume rising: %d%%", pm / 10);
        text_at(dc, 16, 100, buf, g_small, P.muted);
    }
}

static LRESULT CALLBACK RingProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        ring_paint(hwnd, dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        if (wp == TIMER_RING) {
            DWORD el = GetTickCount() - g_ringStart;
            if (g_ringGradual && g_ringRamp) {
                int pm = (int)(el / (DWORD)g_ringRamp);
                sound_set_volume(pm > 1000 ? 1000 : pm);
            }
            if (el > 10UL * 60UL * 1000UL) { ring_close(0); return 0; } /* stop after 10 minutes */
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_SNOOZE) ring_close(1);
        else if (LOWORD(wp) == ID_DISMISS) ring_close(0);
        return 0;
    case WM_CLOSE:
        ring_close(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void ring_open(int kind, int alarmIndex) {
    RECT work;
    int w = 300, h = 170;
    HWND b;
    const char *sound = NULL;
    if (g_ring) ring_close(0);
    g_ringKind = kind;
    g_ringAlarm = alarmIndex;
    g_ringStart = GetTickCount();
    g_ringGradual = 0;
    g_ringRamp = 0;
    if (kind == 2) sound = S.timerSound;
    else if (alarmIndex >= 0) {
        sound = A[alarmIndex].sound;
        if (A[alarmIndex].gradual) {
            g_ringGradual = 1;
            g_ringRamp = A[alarmIndex].ramp; /* ms per permille == seconds */
        }
    }
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    g_ring = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, RING_CLASS, APP_NAME, WS_POPUP | WS_CAPTION | WS_SYSMENU,
                             work.right - w - 12, work.bottom - h - 12, w, h, NULL, NULL, g_inst, NULL);
    if (kind == 0 || kind == 1) {
        char t[24];
        wsprintfA(t, "&Snooze %d min", A[alarmIndex].snooze);
        b = CreateWindowExA(0, "BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 12, 106, 130, 28, g_ring, (HMENU)(INT_PTR)ID_SNOOZE, g_inst, NULL);
        SendMessageA(b, WM_SETFONT, (WPARAM)g_ui, TRUE);
    }
    b = CreateWindowExA(0, "BUTTON", kind == 2 ? "&Stop" : "&Dismiss", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                        150, 106, 130, 28, g_ring, (HMENU)(INT_PTR)ID_DISMISS, g_inst, NULL);
    SendMessageA(b, WM_SETFONT, (WPARAM)g_ui, TRUE);
    if (g_ringGradual) sound_set_volume(30);
    sound_start(sound);
    SetTimer(g_ring, TIMER_RING, 250, NULL);
    ShowWindow(g_ring, SW_SHOWNORMAL);
    SetForegroundWindow(g_ring);
    FlashWindow(g_ring, TRUE);
    SetFocus(b);
}

/* ---------- dialogs ---------- */

static int g_editIndex;
static Alarm g_edit;

static void set_time_fields(HWND dlg, int hourId, int hourSpin, int minId, int minSpin, int ampmId, int hour, int minute) {
    HWND amp = GetDlgItem(dlg, ampmId);
    SendMessageA(amp, CB_RESETCONTENT, 0, 0);
    SendMessageA(amp, CB_ADDSTRING, 0, (LPARAM)"AM");
    SendMessageA(amp, CB_ADDSTRING, 0, (LPARAM)"PM");
    if (S.use24) {
        SendDlgItemMessageA(dlg, hourSpin, UDM_SETRANGE, 0, MAKELONG(23, 0));
        ShowWindow(amp, SW_HIDE);
        SetDlgItemInt(dlg, hourId, (UINT)hour, FALSE);
    } else {
        SendDlgItemMessageA(dlg, hourSpin, UDM_SETRANGE, 0, MAKELONG(12, 1));
        SetDlgItemInt(dlg, hourId, (UINT)(hour % 12 == 0 ? 12 : hour % 12), FALSE);
        SendMessageA(amp, CB_SETCURSEL, hour >= 12 ? 1 : 0, 0);
    }
    SendDlgItemMessageA(dlg, minSpin, UDM_SETRANGE, 0, MAKELONG(59, 0));
    SetDlgItemInt(dlg, minId, (UINT)minute, FALSE);
}

static void get_time_fields(HWND dlg, int hourId, int minId, int ampmId, int *hour, int *minute) {
    int h = (int)GetDlgItemInt(dlg, hourId, NULL, FALSE);
    int m = (int)GetDlgItemInt(dlg, minId, NULL, FALSE);
    if (m < 0 || m > 59) m = 0;
    if (S.use24) {
        if (h < 0 || h > 23) h = 0;
    } else {
        int pm = (int)SendDlgItemMessageA(dlg, ampmId, CB_GETCURSEL, 0, 0) == 1;
        if (h < 1 || h > 12) h = 12;
        h = (h % 12) + (pm ? 12 : 0);
    }
    *hour = h;
    *minute = m;
}

static int pick_wav(HWND owner, char *path) {
    OPENFILENAMEA ofn;
    char file[MAX_PATH];
    file[0] = 0;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn); /* the Windows 95-sized structure (WINVER 0x0400) */
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = "Sound files (*.wav)\0*.wav\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Choose an alarm sound";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameA(&ofn)) return 0;
    str_copy(path, file, MAX_PATH);
    return 1;
}

static const int RAMPS[4] = {30, 60, 180, 300};
static const char *RAMP_TEXT[4] = {"30 sec", "1 min", "3 min", "5 min"};
static const int SNOOZES[5] = {5, 10, 15, 20, 30};

static BOOL CALLBACK AlarmDlg(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    int i;
    char buf[64];
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowTextA(dlg, g_editIndex < 0 ? "New alarm" : "Edit alarm");
        set_time_fields(dlg, IDC_HOUR, IDC_HOURSPIN, IDC_MINUTE, IDC_MINSPIN, IDC_AMPM, g_edit.hour, g_edit.minute);
        SetDlgItemTextA(dlg, IDC_LABEL, g_edit.label);
        SendDlgItemMessageA(dlg, IDC_LABEL, EM_LIMITTEXT, 40, 0);
        for (i = 0; i < 7; i++) CheckDlgButton(dlg, IDC_DAY0 + i, (g_edit.days & (1 << i)) ? BST_CHECKED : BST_UNCHECKED);
        sound_label(buf, g_edit.sound);
        SetDlgItemTextA(dlg, IDC_SOUNDNAME, buf);
        CheckDlgButton(dlg, IDC_GRADUAL, g_edit.gradual ? BST_CHECKED : BST_UNCHECKED);
        for (i = 0; i < 4; i++) {
            SendDlgItemMessageA(dlg, IDC_RAMP, CB_ADDSTRING, 0, (LPARAM)RAMP_TEXT[i]);
            if (RAMPS[i] == g_edit.ramp) SendDlgItemMessageA(dlg, IDC_RAMP, CB_SETCURSEL, (WPARAM)i, 0);
        }
        if (SendDlgItemMessageA(dlg, IDC_RAMP, CB_GETCURSEL, 0, 0) < 0) SendDlgItemMessageA(dlg, IDC_RAMP, CB_SETCURSEL, 1, 0);
        for (i = 0; i < 5; i++) {
            wsprintfA(buf, "%d minutes", SNOOZES[i]);
            SendDlgItemMessageA(dlg, IDC_SNOOZE, CB_ADDSTRING, 0, (LPARAM)buf);
            if (SNOOZES[i] == g_edit.snooze) SendDlgItemMessageA(dlg, IDC_SNOOZE, CB_SETCURSEL, (WPARAM)i, 0);
        }
        if (SendDlgItemMessageA(dlg, IDC_SNOOZE, CB_GETCURSEL, 0, 0) < 0) SendDlgItemMessageA(dlg, IDC_SNOOZE, CB_SETCURSEL, 1, 0);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BROWSE:
            if (pick_wav(dlg, g_edit.sound)) {
                sound_label(buf, g_edit.sound);
                SetDlgItemTextA(dlg, IDC_SOUNDNAME, buf);
            }
            return TRUE;
        case IDC_DEFSOUND:
            g_edit.sound[0] = 0;
            SetDlgItemTextA(dlg, IDC_SOUNDNAME, "Larksong alarm (built in)");
            return TRUE;
        case IDC_TEST:
            sound_test(g_edit.sound);
            return TRUE;
        case IDOK: {
            int h, m, days = 0, r, sn;
            get_time_fields(dlg, IDC_HOUR, IDC_MINUTE, IDC_AMPM, &h, &m);
            for (i = 0; i < 7; i++) if (IsDlgButtonChecked(dlg, IDC_DAY0 + i) == BST_CHECKED) days |= 1 << i;
            g_edit.hour = h;
            g_edit.minute = m;
            g_edit.days = days;
            GetDlgItemTextA(dlg, IDC_LABEL, g_edit.label, sizeof(g_edit.label));
            g_edit.gradual = IsDlgButtonChecked(dlg, IDC_GRADUAL) == BST_CHECKED;
            r = (int)SendDlgItemMessageA(dlg, IDC_RAMP, CB_GETCURSEL, 0, 0);
            g_edit.ramp = RAMPS[r >= 0 && r < 4 ? r : 1];
            sn = (int)SendDlgItemMessageA(dlg, IDC_SNOOZE, CB_GETCURSEL, 0, 0);
            g_edit.snooze = SNOOZES[sn >= 0 && sn < 5 ? sn : 1];
            g_edit.enabled = 1;
            PlaySoundA(NULL, NULL, 0);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            PlaySoundA(NULL, NULL, 0);
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

/*
 * Dismisses an alarm's next ring ahead of time, for when you're up before it.
 * With no alarm selected, it's the one that rings next. A one-time alarm is turned off;
 * a repeating one rings again on its next day. If the ring is already dismissed, offers to undo.
 */
static void alarm_dismiss(int i, int fromTray) {
    char msg[256], a1[32], a2[32];
    int t, keep, now = local_stamp();
    UINT box = MB_YESNO | MB_ICONQUESTION | (fromTray ? MB_SETFOREGROUND : 0);
    if (i < 0) i = soonest_alarm();
    if (i < 0 || !A[i].used || !A[i].enabled) {
        MessageBoxA(g_main, "There's no alarm on to dismiss.", APP_NAME, MB_OK | MB_ICONINFORMATION | (fromTray ? MB_SETFOREGROUND : 0));
        return;
    }
    if (A[i].skipUntil > now) {
        stamp_text(a1, A[i].skipUntil);
        wsprintfA(msg, "The %s alarm is already dismissed.\n\nRing at %s after all?", a1, a1);
        if (MessageBoxA(g_main, msg, APP_NAME, box) == IDYES) { A[i].skipUntil = 0; store_save(); alarms_refresh(); }
        return;
    }
    t = alarm_next_stamp(i);
    if (t < 0) return;
    stamp_text(a1, t);
    if (A[i].days) {
        keep = A[i].skipUntil;
        A[i].skipUntil = t;
        stamp_text(a2, alarm_next_stamp(i));
        A[i].skipUntil = keep;
        wsprintfA(msg, "Up already? Dismiss the %s alarm?\n\nIt will ring again %s.", a1, a2);
    } else {
        wsprintfA(msg, "Up already? Dismiss the %s alarm?\n\nIt only rings once, so it will be turned off.", a1);
    }
    if (MessageBoxA(g_main, msg, APP_NAME, box) != IDYES) return;
    if (A[i].days) A[i].skipUntil = t; else A[i].enabled = 0;
    store_save();
    alarms_refresh();
}

static void alarm_edit(int index) {
    int slot = index;
    if (index < 0) {
        slot = alarm_new();
        if (slot < 0) { MessageBoxA(g_main, "You can have up to 32 alarms.", APP_NAME, MB_OK | MB_ICONINFORMATION); return; }
        g_edit = A[slot];
        A[slot].used = 0; /* not saved until OK */
    } else {
        g_edit = A[index];
    }
    g_editIndex = index;
    if (DialogBoxParamA(g_inst, MAKEINTRESOURCEA(IDD_ALARM), g_main, (DLGPROC)AlarmDlg, 0) == IDOK) {
        g_edit.used = 1;
        g_edit.skipUntil = 0; /* editing clears an early dismissal */
        A[slot] = g_edit;
        g_snoozeAt[slot] = 0;
        store_save();
        alarms_refresh();
    }
}

static const int REMINDS[4] = {15, 30, 45, 60};

static BOOL CALLBACK BedDlg(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    int i;
    char buf[32];
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        CheckDlgButton(dlg, IDC_BEDON, S.bedOn ? BST_CHECKED : BST_UNCHECKED);
        set_time_fields(dlg, IDC_BEDHOUR, IDC_BEDHSPIN, IDC_BEDMIN, IDC_BEDMSPIN, IDC_BEDAMPM, S.bedH, S.bedM);
        set_time_fields(dlg, IDC_WAKEHOUR, IDC_WAKEHSPIN, IDC_WAKEMIN, IDC_WAKEMSPIN, IDC_WAKEAMPM, S.wakeH, S.wakeM);
        for (i = 0; i < 4; i++) {
            wsprintfA(buf, "%d minutes before", REMINDS[i]);
            SendDlgItemMessageA(dlg, IDC_REMIND, CB_ADDSTRING, 0, (LPARAM)buf);
            if (REMINDS[i] == S.remind) SendDlgItemMessageA(dlg, IDC_REMIND, CB_SETCURSEL, (WPARAM)i, 0);
        }
        if (SendDlgItemMessageA(dlg, IDC_REMIND, CB_GETCURSEL, 0, 0) < 0) SendDlgItemMessageA(dlg, IDC_REMIND, CB_SETCURSEL, 1, 0);
        for (i = 0; i < 7; i++) CheckDlgButton(dlg, IDC_BDAY0 + i, (S.bedDays & (1 << i)) ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dlg, IDC_BEDLINK, S.bedLink ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            int r, days = 0;
            S.bedOn = IsDlgButtonChecked(dlg, IDC_BEDON) == BST_CHECKED;
            get_time_fields(dlg, IDC_BEDHOUR, IDC_BEDMIN, IDC_BEDAMPM, &S.bedH, &S.bedM);
            get_time_fields(dlg, IDC_WAKEHOUR, IDC_WAKEMIN, IDC_WAKEAMPM, &S.wakeH, &S.wakeM);
            r = (int)SendDlgItemMessageA(dlg, IDC_REMIND, CB_GETCURSEL, 0, 0);
            S.remind = REMINDS[r >= 0 && r < 4 ? r : 1];
            for (i = 0; i < 7; i++) if (IsDlgButtonChecked(dlg, IDC_BDAY0 + i) == BST_CHECKED) days |= 1 << i;
            S.bedDays = days;
            S.bedLink = IsDlgButtonChecked(dlg, IDC_BEDLINK) == BST_CHECKED;
            if (S.bedLink) {
                /* Wake days follow the bed days, a day later when sleep crosses midnight. */
                int crosses = S.wakeH * 60 + S.wakeM <= S.bedH * 60 + S.bedM;
                int wakeDays = crosses ? (((days << 1) | (days >> 6)) & 0x7F) : days;
                int slot = (S.bedAlarm >= 0 && S.bedAlarm < MAX_ALARMS && A[S.bedAlarm].used) ? S.bedAlarm : alarm_new();
                if (slot >= 0) {
                    if (!A[slot].label[0]) str_copy(A[slot].label, "Wake up", sizeof(A[slot].label));
                    A[slot].hour = S.wakeH;
                    A[slot].minute = S.wakeM;
                    A[slot].days = wakeDays;
                    A[slot].enabled = S.bedOn;
                    S.bedAlarm = slot;
                }
            }
            store_save();
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) { EndDialog(dlg, IDCANCEL); return TRUE; }
        break;
    }
    return FALSE;
}

static BOOL CALLBACK ZonesDlg(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    int i;
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG: {
        HWND lb = GetDlgItem(dlg, IDC_ZONELIST);
        for (i = 0; i < CITY_COUNT; i++) {
            char buf[64];
            int off = CITIES[i].offset;
            wsprintfA(buf, "%s (UTC%c%d:%02d)", CITIES[i].name, off < 0 ? '-' : '+', (off < 0 ? -off : off) / 60, (off < 0 ? -off : off) % 60);
            SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)buf);
        }
        for (i = 0; i < S.zoneCount; i++) SendMessageA(lb, LB_SETSEL, TRUE, S.zones[i]);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            HWND lb = GetDlgItem(dlg, IDC_ZONELIST);
            S.zoneCount = 0;
            for (i = 0; i < CITY_COUNT && S.zoneCount < MAX_ZONES; i++) {
                if (SendMessageA(lb, LB_GETSEL, (WPARAM)i, 0) > 0) S.zones[S.zoneCount++] = i;
            }
            store_save();
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) { EndDialog(dlg, IDCANCEL); return TRUE; }
        break;
    }
    return FALSE;
}

/* ---------- weather & updates (Windows 7+, on worker threads) ---------- */

static char g_net[65536];

static const char *json_after(const char *json, const char *key) {
    const char *p = str_find(json, key);
    if (!p) return 0;
    p += str_len(key);
    while (*p == ' ' || *p == ':' || *p == '"' || *p == '[') p++;
    return p;
}

static void copy_json_string(const char *p, char *out, int n) {
    int i = 0;
    while (*p && *p != '"' && i < n - 1) out[i++] = *p++;
    out[i] = 0;
}

static DWORD WINAPI WeatherThread(LPVOID arg) {
    char path[400], lat[24], lon[24], q[160];
    const char *p;
    int geocode = (int)(INT_PTR)arg;
    g_wxBusy = 1;
    g_wxError[0] = 0;
    if (geocode) {
        url_encode(S.city, q, sizeof(q));
        wsprintfA(path, "/v1/search?name=%s&count=1&language=en&format=json", q);
        if (net_get("geocoding-api.open-meteo.com", path, g_net, sizeof(g_net)) <= 0) {
            str_copy(g_wxError, "couldn't reach the weather service", sizeof(g_wxError));
            goto done;
        }
        p = json_after(g_net, "\"latitude\"");
        if (!p) { str_copy(g_wxError, "that city wasn't found", sizeof(g_wxError)); S.haveLoc = 0; goto done; }
        S.latE4 = parse_fixed(p, 10000);
        p = json_after(g_net, "\"longitude\"");
        S.lonE4 = p ? parse_fixed(p, 10000) : 0;
        p = json_after(g_net, "\"name\"");
        if (p) copy_json_string(p, S.city, sizeof(S.city));
        S.haveLoc = 1;
        store_save();
    }
    if (!S.haveLoc) goto done;
    wsprintfA(lat, "%s%d.%04d", S.latE4 < 0 ? "-" : "", (S.latE4 < 0 ? -S.latE4 : S.latE4) / 10000, (S.latE4 < 0 ? -S.latE4 : S.latE4) % 10000);
    wsprintfA(lon, "%s%d.%04d", S.lonE4 < 0 ? "-" : "", (S.lonE4 < 0 ? -S.lonE4 : S.lonE4) / 10000, (S.lonE4 < 0 ? -S.lonE4 : S.lonE4) % 10000);
    wsprintfA(path, "/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,weather_code&daily=temperature_2m_max,temperature_2m_min&temperature_unit=%s&timezone=auto&forecast_days=1",
              lat, lon, S.celsius ? "celsius" : "fahrenheit");
    if (net_get("api.open-meteo.com", path, g_net, sizeof(g_net)) <= 0) {
        str_copy(g_wxError, "couldn't reach the weather service", sizeof(g_wxError));
        goto done;
    }
    p = str_find(g_net, "\"current\":");
    if (!p) { str_copy(g_wxError, "unexpected answer from the weather service", sizeof(g_wxError)); goto done; }
    {
        const char *t = json_after(p, "\"temperature_2m\"");
        const char *c = json_after(p, "\"weather_code\"");
        const char *hi = json_after(g_net, "\"temperature_2m_max\"");
        const char *lo = json_after(g_net, "\"temperature_2m_min\"");
        int v10;
        if (!t) goto done;
        v10 = parse_fixed(t, 10);
        g_wxTemp = (v10 >= 0 ? v10 + 5 : v10 - 5) / 10;
        g_wxCode = c ? parse_int(c) : -1;
        if (hi) { v10 = parse_fixed(hi, 10); g_wxHigh = (v10 >= 0 ? v10 + 5 : v10 - 5) / 10; }
        if (lo) { v10 = parse_fixed(lo, 10); g_wxLow = (v10 >= 0 ? v10 + 5 : v10 - 5) / 10; }
        str_copy(g_wxPlace, S.city, sizeof(g_wxPlace));
        g_wxOk = 1;
    }
done:
    g_wxBusy = 0;
    PostMessageA(g_main, WM_WEATHER, 0, 0);
    return 0;
}

static void weather_refresh(int geocode) {
    DWORD id;
    HANDLE h;
    if (!net_available() || g_wxBusy) return;
    if (!geocode && !S.haveLoc) return;
    g_wxBusy = 1;
    h = CreateThread(NULL, 0, WeatherThread, (LPVOID)(INT_PTR)geocode, 0, &id);
    if (h) CloseHandle(h); else g_wxBusy = 0;
}

static char g_upd[65536];

static DWORD WINAPI UpdateThread(LPVOID arg) {
    const char *p;
    int quiet = (int)(INT_PTR)arg;
    char best[24];
    best[0] = 0;
    if (net_get("api.github.com", "/repos/adamglassner/larksong-classic/releases?per_page=30", g_upd, sizeof(g_upd)) <= 0) {
        if (!quiet) str_copy(g_updText, "Couldn't check for updates. Check your internet connection.", sizeof(g_updText));
        PostMessageA(g_main, WM_UPDATE, (WPARAM)quiet, 0);
        return 0;
    }
    p = g_upd;
    while ((p = str_find(p, "\"tag_name\"")) != 0) {
        char tag[48];
        p = json_after(p, "\"tag_name\"");
        if (!p) break;
        copy_json_string(p, tag, sizeof(tag));
        if (str_starts(tag, "classic-v")) {
            const char *v = tag + 9;
            if (!best[0] || version_compare(v, best) > 0) str_copy(best, v, sizeof(best));
        }
    }
    if (best[0] && version_compare(best, VER_STRING) > 0) {
        str_copy(g_updVersion, best, sizeof(g_updVersion));
        wsprintfA(g_updText, "Larksong Classic %s is available. Click \"Download page\" to get it.", best);
    } else {
        g_updVersion[0] = 0;
        wsprintfA(g_updText, "You have the latest version (%s).", VER_STRING);
    }
    PostMessageA(g_main, WM_UPDATE, (WPARAM)quiet, 0);
    return 0;
}

static void update_check(int quiet) {
    DWORD id;
    HANDLE h;
    if (!net_available()) return;
    if (!quiet) SetWindowTextA(ctl(ID_UPDSTATUS), "Checking...");
    h = CreateThread(NULL, 0, UpdateThread, (LPVOID)(INT_PTR)quiet, 0, &id);
    if (h) CloseHandle(h);
}

/* ---------- backup & restore ----------
 * A small JSON file shared with the regular Larksong apps (Android, Windows 10/11, Linux),
 * so alarms can move between an old PC and a phone.
 */

typedef struct { char *buf; int len, cap; } Out;

static void out_add(Out *o, const char *s) {
    int n = str_len(s);
    if (o->len + n + 1 > o->cap) return;
    memcpy(o->buf + o->len, s, (size_t)n);
    o->len += n;
    o->buf[o->len] = 0;
}

static void out_json_str(Out *o, const char *s) {
    char tmp[4];
    out_add(o, "\"");
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') { tmp[0] = '\\'; tmp[1] = (char)ch; tmp[2] = 0; }
        else if (ch < 32) { tmp[0] = ' '; tmp[1] = 0; }
        else { tmp[0] = (char)ch; tmp[1] = 0; }
        out_add(o, tmp);
    }
    out_add(o, "\"");
}

static void out_days(Out *o, int bits) {
    /* Regular Larksong numbers days 1 = Monday ... 7 = Sunday. */
    int i, first = 1;
    char n[8];
    out_add(o, "[");
    for (i = 1; i <= 7; i++) {
        if (bits & (1 << (i % 7))) {
            wsprintfA(n, first ? "%d" : ", %d", i);
            out_add(o, n);
            first = 0;
        }
    }
    out_add(o, "]");
}

static int pick_file(HWND owner, char *path, int save) {
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = "Larksong backup (*.json)\0*.json\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "json";
    if (save) {
        ofn.lpstrTitle = "Back up alarms";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
        return GetSaveFileNameA(&ofn);
    }
    ofn.lpstrTitle = "Restore alarms";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    return GetOpenFileNameA(&ofn);
}

static void backup_save(HWND owner) {
    char path[MAX_PATH], line[400], name[64];
    SYSTEMTIME t;
    Out o;
    int i, first = 1;
    HANDLE f;
    DWORD written;
    GetLocalTime(&t);
    wsprintfA(path, "larksong-alarms-%04d-%02d-%02d.json", t.wYear, t.wMonth, t.wDay);
    if (!pick_file(owner, path, 1)) return;
    o.cap = 65536;
    o.len = 0;
    o.buf = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)o.cap);
    if (!o.buf) return;
    out_add(&o, "{\r\n  \"format\": \"larksong-backup\",\r\n  \"version\": 1,\r\n  \"app\": \"Larksong Classic " VER_STRING "\",\r\n  \"alarms\": [");
    for (i = 0; i < MAX_ALARMS; i++) {
        Alarm *a = &A[i];
        if (!a->used) continue;
        out_add(&o, first ? "\r\n    {" : ",\r\n    {");
        first = 0;
        wsprintfA(line, "\"id\": %d, \"hour\": %d, \"minute\": %d, \"days\": ", i + 1, a->hour, a->minute);
        out_add(&o, line);
        out_days(&o, a->days);
        out_add(&o, ", \"label\": ");
        out_json_str(&o, a->label);
        wsprintfA(line, ", \"enabled\": %s, \"vibrate\": true, \"gradual\": %s, \"ramp\": %d, \"snooze\": %d, \"soundUri\": ",
                  a->enabled ? "true" : "false", a->gradual ? "true" : "false", a->ramp, a->snooze);
        out_add(&o, line);
        out_json_str(&o, a->sound);
        out_add(&o, ", \"soundName\": ");
        sound_label(name, a->sound);
        out_json_str(&o, a->sound[0] ? name : "Default alarm");
        out_add(&o, "}");
    }
    out_add(&o, "\r\n  ],\r\n  \"bedtime\": {");
    wsprintfA(line, "\"enabled\": %s, \"bedHour\": %d, \"bedMinute\": %d, \"wakeHour\": %d, \"wakeMinute\": %d, \"remind\": %d, \"link\": %s, \"days\": ",
              S.bedOn ? "true" : "false", S.bedH, S.bedM, S.wakeH, S.wakeM, S.remind, S.bedLink ? "true" : "false");
    out_add(&o, line);
    out_days(&o, S.bedDays);
    out_add(&o, "}\r\n}\r\n");

    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE || !WriteFile(f, o.buf, (DWORD)o.len, &written, NULL) || written != (DWORD)o.len) {
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
        MessageBoxA(owner, "The backup couldn't be saved there. Try another folder.", APP_NAME, MB_OK | MB_ICONEXCLAMATION);
    } else {
        CloseHandle(f);
        MessageBoxA(owner, "Your alarms were backed up.\n\nYou can restore this file in Larksong Classic, or in Larksong on your phone or a newer PC.", APP_NAME, MB_OK | MB_ICONINFORMATION);
    }
    HeapFree(GetProcessHeap(), 0, o.buf);
}

/* Finds "key": in an object and returns the text after the colon (spaces skipped). */
static const char *json_val(const char *obj, const char *key) {
    char k[40];
    const char *p;
    wsprintfA(k, "\"%s\"", key);
    p = str_find(obj, k);
    if (!p) return 0;
    p += str_len(k);
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != ':') return 0;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static int json_bool(const char *obj, const char *key, int def) {
    const char *p = json_val(obj, key);
    if (!p) return def;
    return str_starts(p, "true") || (*p >= '1' && *p <= '9');
}

static int json_int(const char *obj, const char *key, int def) {
    const char *p = json_val(obj, key);
    return p ? parse_int(p) : def;
}

static void json_string(const char *obj, const char *key, char *out, int n) {
    const char *p = json_val(obj, key);
    int i = 0;
    out[0] = 0;
    if (!p || *p != '"') return;
    p++;
    while (*p && *p != '"' && i < n - 1) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'u') { out[i++] = '?'; p += 4; if (!*p) break; }
            else if (*p == 'n' || *p == 't' || *p == 'r') out[i++] = ' ';
            else out[i++] = *p;
            p++;
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = 0;
}

static int json_days(const char *obj, const char *key) {
    const char *p = json_val(obj, key);
    int bits = 0;
    if (!p || *p != '[') return 0;
    p++;
    while (*p && *p != ']') {
        if (*p >= '1' && *p <= '7') bits |= 1 << ((*p - '0') % 7);
        p++;
    }
    return bits;
}

static void backup_restore(HWND owner) {
    char path[MAX_PATH], msg[200];
    HANDLE f;
    DWORD size, got;
    char *buf, *p;
    int count = 0, added = 0, answer, i;
    path[0] = 0;
    if (!pick_file(owner, path, 0)) return;
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { MessageBoxA(owner, "That file couldn't be opened.", APP_NAME, MB_OK | MB_ICONEXCLAMATION); return; }
    size = GetFileSize(f, NULL);
    if (size == (DWORD)-1 || size > 512 * 1024) { CloseHandle(f); MessageBoxA(owner, "That doesn't look like a Larksong backup.", APP_NAME, MB_OK | MB_ICONEXCLAMATION); return; }
    buf = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size + 1);
    if (!buf) { CloseHandle(f); return; }
    ReadFile(f, buf, size, &got, NULL);
    CloseHandle(f);
    buf[got] = 0;

    p = (char *)json_val(buf, "alarms");
    if (!p || *p != '[') {
        MessageBoxA(owner, "That doesn't look like a Larksong backup.", APP_NAME, MB_OK | MB_ICONEXCLAMATION);
        HeapFree(GetProcessHeap(), 0, buf);
        return;
    }
    { /* count the alarms first */
        const char *q = p + 1;
        while (*q && *q != ']') { if (*q == '{') count++; q++; }
    }
    wsprintfA(msg, "This backup has %d alarm(s).\n\nReplace your current alarms with them?\n\nYes = replace, No = add them to your alarms.", count);
    answer = MessageBoxA(owner, msg, APP_NAME, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (answer == IDCANCEL) { HeapFree(GetProcessHeap(), 0, buf); return; }
    if (answer == IDYES) {
        for (i = 0; i < MAX_ALARMS; i++) { A[i].used = 0; g_snoozeAt[i] = 0; }
        S.bedAlarm = -1;
    }
    p++;
    while (*p && *p != ']') {
        char *start, *end, saved;
        int slot;
        if (*p != '{') { p++; continue; }
        start = p;
        end = start;
        while (*end && *end != '}') end++;
        if (!*end) break;
        saved = *end;
        *end = 0;
        slot = alarm_new();
        if (slot >= 0) {
            Alarm *a = &A[slot];
            a->hour = json_int(start, "hour", 7) % 24;
            a->minute = json_int(start, "minute", 0) % 60;
            a->days = json_days(start, "days");
            a->enabled = json_bool(start, "enabled", 1);
            a->gradual = json_bool(start, "gradual", 1);
            a->ramp = json_int(start, "ramp", 60);
            a->snooze = json_int(start, "snooze", 10);
            if (a->snooze < 1 || a->snooze > 60) a->snooze = 10;
            json_string(start, "label", a->label, sizeof(a->label));
            json_string(start, "soundUri", a->sound, MAX_PATH);
            /* Sounds from a phone or another PC won't exist here: use the built-in tone. */
            if (!(a->sound[0] && a->sound[1] == ':' && file_exists(a->sound))) a->sound[0] = 0;
            added++;
        }
        *end = saved;
        p = end + 1;
    }
    {
        const char *b = json_val(buf, "bedtime");
        if (b && *b == '{' && answer == IDYES) {
            S.bedOn = json_bool(b, "enabled", 0);
            S.bedH = json_int(b, "bedHour", 23) % 24;
            S.bedM = json_int(b, "bedMinute", 0) % 60;
            S.wakeH = json_int(b, "wakeHour", 6) % 24;
            S.wakeM = json_int(b, "wakeMinute", 30) % 60;
            S.remind = json_int(b, "remind", 30);
            S.bedDays = json_days(b, "days");
            S.bedLink = 0;
        }
    }
    store_save();
    HeapFree(GetProcessHeap(), 0, buf);
    wsprintfA(msg, "Restored %d alarm(s).", added);
    MessageBoxA(owner, msg, APP_NAME, MB_OK | MB_ICONINFORMATION);
}

/* ---------- tray ---------- */

static void tray_add(void) {
    memset(&g_nid, 0, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid); /* NOTIFYICONDATA as in Windows 95 */
    g_nid.hWnd = g_main;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = (HICON)LoadImageA(g_inst, MAKEINTRESOURCEA(IDI_APP), IMAGE_ICON, 16, 16, 0);
    str_copy(g_nid.szTip, APP_NAME, sizeof(g_nid.szTip));
    Shell_NotifyIconA(NIM_ADD, &g_nid);
}

static void tray_menu(void) {
    POINT pt;
    HMENU m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, ID_TRAY_OPEN, "&Open Larksong");
    AppendMenuA(m, MF_STRING, ID_TRAY_NEW, "&New alarm...");
    {
        int i = soonest_alarm();
        if (i >= 0 && alarm_next_stamp(i) - local_stamp() < 1440) {
            char item[64], when[32];
            stamp_text(when, alarm_next_stamp(i));
            wsprintfA(item, "&Dismiss next alarm (%s)...", when);
            AppendMenuA(m, MF_STRING, ID_TRAY_SKIP, item);
        }
    }
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, ID_TRAY_EXIT, "E&xit");
    SetMenuDefaultItem(m, ID_TRAY_OPEN, FALSE);
    GetCursorPos(&pt);
    SetForegroundWindow(g_main);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_main, NULL);
    PostMessageA(g_main, WM_NULL, 0, 0);
    DestroyMenu(m);
}

static void main_show(void) {
    ShowWindow(g_main, SW_SHOW);
    ShowWindow(g_main, SW_RESTORE);
    SetForegroundWindow(g_main);
}

/* ---------- main window ---------- */

static void theme_changed(void) {
    theme_apply();
    fonts_apply();
    show_tab(g_tab);
    InvalidateRect(g_main, NULL, TRUE);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    int id;
    switch (msg) {
    case WM_TIMER:
        if (wp == TIMER_TICK) {
            tick();
            if (g_tab != TAB_SETTINGS && IsWindowVisible(hwnd)) InvalidateRect(g_panel, NULL, FALSE);
        }
        return 0;

    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g_bgBrush);
        return 1;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetTextColor((HDC)wp, P.text);
        SetBkColor((HDC)wp, P.bg);
        return (LRESULT)g_bgBrush;
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
        if (S.theme == TH_CLASSIC) break;
        SetTextColor((HDC)wp, P.text);
        SetBkColor((HDC)wp, P.panel);
        return (LRESULT)g_panelBrush;

    case WM_TRAY:
        if (lp == WM_LBUTTONDBLCLK || lp == WM_LBUTTONUP) main_show();
        else if (lp == WM_RBUTTONUP) tray_menu();
        return 0;

    case WM_REMINDER: {
        char t[16], msgText[160];
        format_time(t, S.bedH, S.bedM, S.use24);
        wsprintfA(msgText, "Bedtime is in %d minutes (%s). Time to start winding down.", S.remind, t);
        MessageBeep(MB_ICONASTERISK);
        MessageBoxA(NULL, msgText, APP_NAME, MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
        return 0;
    }

    case WM_WEATHER:
        InvalidateRect(g_panel, NULL, FALSE);
        if (g_tab == TAB_SETTINGS) settings_refresh();
        if (g_wxError[0] && g_tab == TAB_SETTINGS) {
            char t[140];
            wsprintfA(t, "Weather: %s.", g_wxError);
            SetWindowTextA(ctl(ID_WSTATUS), t);
        }
        return 0;

    case WM_UPDATE:
        if (g_tab == TAB_SETTINGS) SetWindowTextA(ctl(ID_UPDSTATUS), g_updText);
        if (wp && g_updVersion[0]) {
            /* found quietly at startup: let the person know in the tray tooltip and title */
            char title[96];
            wsprintfA(title, "%s - update %s available", APP_NAME, g_updVersion);
            SetWindowTextA(g_main, title);
        }
        return 0;

    case WM_COMMAND:
        id = LOWORD(wp);
        if (id >= ID_NAV0 && id < ID_NAV0 + TAB_COUNT) { show_tab(id - ID_NAV0); return 0; }
        if (id >= ID_TPRESET0 && id < ID_TPRESET0 + 6) {
            static const int PRESETS[6] = {1, 3, 5, 10, 15, 30};
            g_tmRunning = 0;
            g_tmTotal = g_tmLeft = (DWORD)PRESETS[id - ID_TPRESET0] * 60000UL;
            SetWindowTextA(ctl(ID_TSTART), "&Start");
            InvalidateRect(g_panel, NULL, FALSE);
            return 0;
        }
        switch (id) {
        case ID_ZONES:
            DialogBoxParamA(g_inst, MAKEINTRESOURCEA(IDD_ZONES), hwnd, (DLGPROC)ZonesDlg, 0);
            InvalidateRect(g_panel, NULL, FALSE);
            break;
        case ID_WREFRESH:
            if (!net_available()) MessageBoxA(hwnd, "Weather needs Windows 7 or later with an internet connection.", APP_NAME, MB_OK | MB_ICONINFORMATION);
            else if (!S.haveLoc) { show_tab(TAB_SETTINGS); SetFocus(ctl(ID_CITY)); }
            else weather_refresh(0);
            break;
        case ID_ALIST:
            if (HIWORD(wp) == LBN_DBLCLK) alarm_edit(alarms_selected());
            break;
        case ID_AADD:
            alarm_edit(-1);
            break;
        case ID_AEDIT: {
            int i = alarms_selected();
            if (i >= 0) alarm_edit(i); else MessageBoxA(hwnd, "Select an alarm first.", APP_NAME, MB_OK | MB_ICONINFORMATION);
            break;
        }
        case ID_ADEL: {
            int i = alarms_selected();
            if (i >= 0 && MessageBoxA(hwnd, "Delete this alarm?", APP_NAME, MB_YESNO | MB_ICONQUESTION) == IDYES) {
                A[i].used = 0;
                g_snoozeAt[i] = 0;
                store_save();
                alarms_refresh();
            }
            break;
        }
        case ID_ATOGGLE: {
            int i = alarms_selected();
            if (i >= 0) { A[i].enabled = !A[i].enabled; A[i].skipUntil = 0; g_snoozeAt[i] = 0; store_save(); alarms_refresh(); }
            break;
        }
        case ID_ASKIP:
            alarm_dismiss(alarms_selected(), 0);
            break;
        case ID_TRAY_SKIP:
            alarm_dismiss(soonest_alarm(), 1);
            break;
        case ID_ABED:
            if (DialogBoxParamA(g_inst, MAKEINTRESOURCEA(IDD_BEDTIME), hwnd, (DLGPROC)BedDlg, 0) == IDOK) alarms_refresh();
            break;
        case ID_SWSTART:
            if (g_swRunning) { g_swBase += GetTickCount() - g_swStart; g_swRunning = 0; SetWindowTextA(ctl(ID_SWSTART), "&Start"); }
            else { g_swStart = GetTickCount(); g_swRunning = 1; SetWindowTextA(ctl(ID_SWSTART), "&Pause"); }
            break;
        case ID_SWLAP:
            if (g_swRunning && g_lapCount < MAX_LAPS) {
                DWORD total = g_swBase + (GetTickCount() - g_swStart), done = 0;
                int i;
                for (i = 0; i < g_lapCount; i++) done += g_laps[i];
                g_laps[g_lapCount++] = total - done;
                laps_refresh();
            }
            break;
        case ID_SWRESET:
            g_swRunning = 0; g_swBase = 0; g_lapCount = 0;
            SetWindowTextA(ctl(ID_SWSTART), "&Start");
            laps_refresh();
            InvalidateRect(g_panel, NULL, FALSE);
            break;
        case ID_TSET: {
            DWORD m = GetDlgItemInt(hwnd, ID_TMIN, NULL, FALSE), s = GetDlgItemInt(hwnd, ID_TSEC, NULL, FALSE);
            if (s > 59) s = 59;
            if (m * 60 + s > 0) {
                g_tmRunning = 0;
                g_tmTotal = g_tmLeft = (m * 60 + s) * 1000UL;
                SetWindowTextA(ctl(ID_TSTART), "&Start");
                InvalidateRect(g_panel, NULL, FALSE);
            }
            break;
        }
        case ID_TSTART:
            if (g_tmRunning) {
                DWORD now = GetTickCount();
                g_tmLeft = now < g_tmEnd ? g_tmEnd - now : 0;
                g_tmRunning = 0;
                SetWindowTextA(ctl(ID_TSTART), "&Start");
            } else {
                if (!g_tmLeft) g_tmLeft = g_tmTotal;
                g_tmEnd = GetTickCount() + g_tmLeft;
                g_tmRunning = 1;
                SetWindowTextA(ctl(ID_TSTART), "&Pause");
            }
            break;
        case ID_TRESET:
            g_tmRunning = 0;
            g_tmLeft = g_tmTotal;
            SetWindowTextA(ctl(ID_TSTART), "&Start");
            InvalidateRect(g_panel, NULL, FALSE);
            break;
        case ID_TSOUNDBTN:
            if (pick_wav(hwnd, S.timerSound)) { store_save(); settings_refresh(); }
            break;
        case ID_TSOUNDDEF:
            S.timerSound[0] = 0; store_save(); settings_refresh();
            break;
        case ID_THEME:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int t = (int)SendMessageA(ctl(ID_THEME), CB_GETCURSEL, 0, 0);
                if (t >= 0 && t < TH_COUNT) { S.theme = t; store_save(); theme_changed(); }
            }
            break;
        case ID_USE24:
            S.use24 = IsDlgButtonChecked(hwnd, ID_USE24) == BST_CHECKED; store_save();
            break;
        case ID_SHOWSEC:
            S.showSec = IsDlgButtonChecked(hwnd, ID_SHOWSEC) == BST_CHECKED; store_save();
            break;
        case ID_AUTOSTART:
            autostart_set(IsDlgButtonChecked(hwnd, ID_AUTOSTART) == BST_CHECKED);
            break;
        case ID_CELSIUS:
            S.celsius = IsDlgButtonChecked(hwnd, ID_CELSIUS) == BST_CHECKED; store_save();
            g_wxOk = 0;
            weather_refresh(0);
            break;
        case ID_CITYSET:
            GetWindowTextA(ctl(ID_CITY), S.city, sizeof(S.city));
            if (S.city[0]) {
                g_wxOk = 0;
                SetWindowTextA(ctl(ID_WSTATUS), "Looking up the city...");
                weather_refresh(1);
            }
            break;
        case ID_UPDCHECK:
            update_check(0);
            break;
        case ID_BACKUP:
            backup_save(hwnd);
            break;
        case ID_CREDITS:
            MessageBoxA(hwnd,
                "LARKSONG CLASSIC " VER_STRING "\n"
                "Created by Adam Glassner - The Glassnerds\n\n"
                "SOUNDS\n"
                "\"Alarm Clock\", the default alarm sound, by freesound_community\n"
                "on Pixabay (Pixabay Content License)\n\n"
                "WEATHER\n"
                "Open-Meteo (open-meteo.com), CC BY 4.0\n\n"
                "Thank you for using Larksong Classic.",
                "Credits", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_CHANGES:
            MessageBoxA(hwnd,
                "2.1.0\n"
                "  - Credits and What's new, in Settings\n\n"
                "2.0.1\n"
                "  - New default alarm sound: a classic alarm-clock ring\n\n"
                "2.0.0\n"
                "  - Daybreak Classic is now Larksong Classic, with a new icon\n"
                "  - Your alarms and settings come over automatically\n\n"
                "1.2.0\n"
                "  - Up early? Dismiss the next ring of an alarm\n\n"
                "1.1.0\n"
                "  - Retro theme, and back up and restore alarms\n\n"
                "1.0.0\n"
                "  - First version: alarms, stopwatch, timer, world clocks,\n"
                "    bedtime reminder and weather, for Windows 95 to 11",
                "What's new in Larksong Classic", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_RESTORE:
            backup_restore(hwnd);
            alarms_refresh();
            break;
        case ID_UPDOPEN:
            if (g_updVersion[0]) {
                char url[160];
                wsprintfA(url, "%s/tag/classic-v%s", RELEASES_URL, g_updVersion);
                ShellExecuteA(hwnd, "open", url, NULL, NULL, SW_SHOWNORMAL);
            } else {
                ShellExecuteA(hwnd, "open", RELEASES_URL, NULL, NULL, SW_SHOWNORMAL);
            }
            break;
        case ID_TRAY_OPEN:
            main_show();
            break;
        case ID_TRAY_NEW:
            main_show();
            show_tab(TAB_ALARMS);
            alarm_edit(-1);
            break;
        case ID_TRAY_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;

    case WM_CLOSE:
        /* Keep running in the notification area so alarms still ring. */
        ShowWindow(hwnd, SW_HIDE);
        if (!S.trayHint) {
            S.trayHint = 1;
            store_save();
            MessageBoxA(NULL, "Larksong is still running in the notification area (next to the clock) so your alarms will ring.\n\nTo quit, right-click its icon and choose Exit.",
                        APP_NAME, MB_OK | MB_ICONINFORMATION);
        }
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconA(NIM_DELETE, &g_nid);
        ring_close(0);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ---------- start-up ---------- */

static int run(HINSTANCE inst, const char *cmd, int show) {
    WNDCLASSA wc;
    MSG m;
    RECT r;
    HANDLE mutex;
    int startHidden = str_find(cmd, "/tray") != 0;

    g_inst = inst;
    mutex = CreateMutexA(NULL, FALSE, "LarksongClassicSingleInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowA(MAIN_CLASS, NULL);
        if (other) {
            ShowWindow(other, SW_SHOW);
            ShowWindow(other, SW_RESTORE);
            SetForegroundWindow(other);
        }
        return 0;
    }

    InitCommonControls();
    store_init();
    store_load();
    theme_apply();
    g_ui = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    fonts_apply();

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = MainProc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(IDI_APP));
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.lpszClassName = MAIN_CLASS;
    RegisterClassA(&wc);

    wc.lpfnWndProc = PanelProc;
    wc.hIcon = NULL;
    wc.lpszClassName = PANEL_CLASS;
    RegisterClassA(&wc);

    wc.lpfnWndProc = RingProc;
    wc.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(IDI_APP));
    wc.lpszClassName = RING_CLASS;
    RegisterClassA(&wc);

    r.left = 0; r.top = 0; r.right = 480; r.bottom = 514;
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    g_main = CreateWindowExA(0, MAIN_CLASS, APP_NAME, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    if (!g_main) return 1;
    g_panel = CreateWindowExA(WS_EX_CLIENTEDGE, PANEL_CLASS, "", WS_CHILD, 10, 46, 460, 320, g_main, NULL, inst, NULL);
    create_pages();
    show_tab(TAB_CLOCK);
    tray_add();
    SetTimer(g_main, TIMER_TICK, 100, NULL);
    if (!startHidden) {
        ShowWindow(g_main, show);
        UpdateWindow(g_main);
    }
    weather_refresh(0);
    update_check(1);

    while (GetMessageA(&m, NULL, 0, 0) > 0) {
        if (g_ring && IsDialogMessageA(g_ring, &m)) continue;
        if (IsWindowVisible(g_main) && IsDialogMessageA(g_main, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    if (mutex) CloseHandle(mutex);
    return (int)m.wParam;
}

/* Entry point (no C runtime). */
void __cdecl entry(void) {
    STARTUPINFOA si;
    const char *cmd = GetCommandLineA();
    int show;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    GetStartupInfoA(&si);
    show = (si.dwFlags & STARTF_USESHOWWINDOW) ? si.wShowWindow : SW_SHOWDEFAULT;
    ExitProcess((UINT)run(GetModuleHandleA(NULL), cmd, show));
}
