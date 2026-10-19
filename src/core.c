#include "core.h"

HINSTANCE g_inst;
Settings S;
Alarm A[MAX_ALARMS];

const short SIN60[60] = {0,105,208,309,407,500,588,669,743,809,866,914,951,978,995,1000,995,978,951,914,866,809,743,669,588,500,407,309,208,105,0,-105,-208,-309,-407,-500,-588,-669,-743,-809,-866,-914,-951,-978,-995,-1000,-995,-978,-951,-914,-866,-809,-743,-669,-588,-500,-407,-309,-208,-105};

/* Standard UTC offsets in minutes, with the daylight-saving rule each city follows. */
const City CITIES[] = {
    {"Honolulu", -600, DST_NONE}, {"Anchorage", -540, DST_US}, {"Los Angeles", -480, DST_US},
    {"Vancouver", -480, DST_US}, {"Denver", -420, DST_US}, {"Phoenix", -420, DST_NONE},
    {"Chicago", -360, DST_US}, {"Mexico City", -360, DST_NONE}, {"New York", -300, DST_US},
    {"Toronto", -300, DST_US}, {"Halifax", -240, DST_US}, {"Sao Paulo", -180, DST_NONE},
    {"Buenos Aires", -180, DST_NONE}, {"Reykjavik", 0, DST_NONE}, {"London", 0, DST_EU},
    {"Dublin", 0, DST_EU}, {"Lisbon", 0, DST_EU}, {"Paris", 60, DST_EU}, {"Berlin", 60, DST_EU},
    {"Madrid", 60, DST_EU}, {"Rome", 60, DST_EU}, {"Amsterdam", 60, DST_EU}, {"Stockholm", 60, DST_EU},
    {"Lagos", 60, DST_NONE}, {"Athens", 120, DST_EU}, {"Helsinki", 120, DST_EU}, {"Johannesburg", 120, DST_NONE},
    {"Cairo", 120, DST_NONE}, {"Istanbul", 180, DST_NONE}, {"Moscow", 180, DST_NONE}, {"Nairobi", 180, DST_NONE},
    {"Dubai", 240, DST_NONE}, {"Karachi", 300, DST_NONE}, {"Mumbai", 330, DST_NONE}, {"Delhi", 330, DST_NONE},
    {"Dhaka", 360, DST_NONE}, {"Bangkok", 420, DST_NONE}, {"Jakarta", 420, DST_NONE}, {"Singapore", 480, DST_NONE},
    {"Hong Kong", 480, DST_NONE}, {"Beijing", 480, DST_NONE}, {"Perth", 480, DST_NONE}, {"Manila", 480, DST_NONE},
    {"Seoul", 540, DST_NONE}, {"Tokyo", 540, DST_NONE}, {"Brisbane", 600, DST_NONE}, {"Sydney", 600, DST_AU},
    {"Melbourne", 600, DST_AU}, {"Auckland", 720, DST_NZ},
};
const int CITY_COUNT = sizeof(CITIES) / sizeof(CITIES[0]);

/* ---------- tiny libc ---------- */

void *memset(void *d, int c, size_t n) {
    unsigned char *p = (unsigned char *)d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}

void *memcpy(void *d, const void *s, size_t n) {
    unsigned char *p = (unsigned char *)d;
    const unsigned char *q = (const unsigned char *)s;
    while (n--) *p++ = *q++;
    return d;
}

int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }

void str_copy(char *d, const char *s, int n) {
    int i = 0;
    if (n <= 0) return;
    while (s[i] && i < n - 1) { d[i] = s[i]; i++; }
    d[i] = 0;
}

void str_cat(char *d, const char *s, int n) {
    int l = str_len(d);
    if (l < n) str_copy(d + l, s, n - l);
}

int str_starts(const char *s, const char *p) {
    while (*p) { if (*s++ != *p++) return 0; }
    return 1;
}

const char *str_find(const char *hay, const char *needle) {
    int n = str_len(needle);
    if (!n) return hay;
    for (; *hay; hay++) {
        int i = 0;
        while (i < n && hay[i] == needle[i]) i++;
        if (i == n) return hay;
    }
    return 0;
}

int parse_int(const char *s) {
    int neg = 0, v = 0;
    while (*s == ' ' || *s == '"') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return neg ? -v : v;
}

/* Parses a decimal number into an integer scaled by `scale` (10, 100, 10000 ...), rounded. */
int parse_fixed(const char *s, int scale) {
    int neg = 0, whole = 0, frac = 0, fracScale = 1;
    while (*s == ' ' || *s == '"' || *s == ':') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { whole = whole * 10 + (*s - '0'); s++; }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            if (fracScale < scale * 10) { frac = frac * 10 + (*s - '0'); fracScale *= 10; }
            s++;
        }
    }
    {
        int v = whole * scale + (frac * scale * 10 / fracScale + 5) / 10;
        return neg ? -v : v;
    }
}

int version_compare(const char *a, const char *b) {
    int i;
    for (i = 0; i < 3; i++) {
        int x = parse_int(a), y = parse_int(b);
        if (x != y) return x < y ? -1 : 1;
        while (*a && *a != '.') a++;
        while (*b && *b != '.') b++;
        if (*a) a++;
        if (*b) b++;
    }
    return 0;
}

/* ---------- calendar math (days since 1970-01-01) ---------- */

int days_from_civil(int y, int m, int d) {
    int era, yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void civil_from_days(int z, int *y, int *m, int *d) {
    int era, doe, yoe, doy, mp;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    *y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp + (mp < 10 ? 3 : -9);
    if (*m <= 2) (*y)++;
}

int weekday_from_days(int days) {
    int w = (days + 4) % 7; /* 1970-01-01 was a Thursday */
    return w < 0 ? w + 7 : w;
}

int local_stamp(void) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    return days_from_civil(t.wYear, t.wMonth, t.wDay) * 1440 + t.wHour * 60 + t.wMinute;
}

int utc_stamp(void) {
    SYSTEMTIME t;
    GetSystemTime(&t);
    return days_from_civil(t.wYear, t.wMonth, t.wDay) * 1440 + t.wHour * 60 + t.wMinute;
}

/* Day number of the nth (1-based) Sunday of a month; n = -1 for the last Sunday. */
static int nth_sunday(int y, int m, int n) {
    if (n > 0) {
        int first = days_from_civil(y, m, 1);
        int w = weekday_from_days(first);
        return first + (7 - w) % 7 + (n - 1) * 7;
    } else {
        int nm = m == 12 ? 1 : m + 1, ny = m == 12 ? y + 1 : y;
        int last = days_from_civil(ny, nm, 1) - 1;
        return last - weekday_from_days(last);
    }
}

/* Is daylight saving time in effect at UTC stamp `u` for a city's rule? */
static int in_dst(int rule, int stdOffset, int u) {
    int y, m, d, start, end;
    civil_from_days(u / 1440, &y, &m, &d);
    switch (rule) {
    case DST_US: /* 2:00 local, 2nd Sunday of March to 1st Sunday of November */
        start = nth_sunday(y, 3, 2) * 1440 + 120 - stdOffset;
        end = nth_sunday(y, 11, 1) * 1440 + 120 - (stdOffset + 60);
        return u >= start && u < end;
    case DST_EU: /* 01:00 UTC, last Sunday of March to last Sunday of October */
        start = nth_sunday(y, 3, -1) * 1440 + 60;
        end = nth_sunday(y, 10, -1) * 1440 + 60;
        return u >= start && u < end;
    case DST_AU: /* southern: 2:00 std, 1st Sunday of October to 3:00 DST 1st Sunday of April */
        start = nth_sunday(y, 10, 1) * 1440 + 120 - stdOffset;
        end = nth_sunday(y, 4, 1) * 1440 + 180 - (stdOffset + 60);
        return u >= start || u < end;
    case DST_NZ: /* last Sunday of September to 1st Sunday of April */
        start = nth_sunday(y, 9, -1) * 1440 + 120 - stdOffset;
        end = nth_sunday(y, 4, 1) * 1440 + 180 - (stdOffset + 60);
        return u >= start || u < end;
    }
    return 0;
}

void zone_time(int city, int *hour, int *minute, int *dayDelta) {
    const City *c = &CITIES[city];
    int u = utc_stamp();
    int t = u + c->offset + (in_dst(c->rule, c->offset, u) ? 60 : 0);
    int localDay = local_stamp() / 1440;
    int mod = t % 1440;
    if (mod < 0) mod += 1440;
    *hour = mod / 60;
    *minute = mod % 60;
    *dayDelta = (t - mod) / 1440 - localDay;
}

void format_hm(char *out, int hour, int minute, int use24) {
    if (use24) wsprintfA(out, "%02d:%02d", hour, minute);
    else wsprintfA(out, "%d:%02d", hour % 12 == 0 ? 12 : hour % 12, minute);
}

void format_time(char *out, int hour, int minute, int use24) {
    format_hm(out, hour, minute, use24);
    if (!use24) str_cat(out, hour < 12 ? " AM" : " PM", 16);
}

static const char *DAY3[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

void days_text(char *out, int days) {
    int i;
    out[0] = 0;
    if (days == 0) { str_copy(out, "Once", 32); return; }
    if (days == 0x7F) { str_copy(out, "Every day", 32); return; }
    if (days == 0x3E) { str_copy(out, "Weekdays", 32); return; }
    if (days == 0x41) { str_copy(out, "Weekends", 32); return; }
    for (i = 0; i < 7; i++) {
        if (days & (1 << i)) {
            if (out[0]) str_cat(out, " ", 40);
            str_cat(out, DAY3[i], 40);
        }
    }
}

/* ---------- platform ---------- */

static OSVERSIONINFOA g_ver;

static void ver_init(void) {
    if (g_ver.dwOSVersionInfoSize) return;
    g_ver.dwOSVersionInfoSize = sizeof(g_ver);
    GetVersionExA(&g_ver);
}

int os_major(void) { ver_init(); return (int)g_ver.dwMajorVersion; }
int os_minor(void) { ver_init(); return (int)g_ver.dwMinorVersion; }
int os_is_nt(void) { ver_init(); return g_ver.dwPlatformId == VER_PLATFORM_WIN32_NT; }
int os_at_least_win7(void) {
    return os_is_nt() && (os_major() > 6 || (os_major() == 6 && os_minor() >= 1));
}

void os_name(char *out) {
    int maj = os_major(), min = os_minor();
    ver_init();
    if (!os_is_nt()) {
        if (min < 10) str_copy(out, "Windows 95", 32);
        else if (min < 90) str_copy(out, "Windows 98", 32);
        else str_copy(out, "Windows Me", 32);
        return;
    }
    if (maj == 4) str_copy(out, "Windows NT 4.0", 32);
    else if (maj == 5 && min == 0) str_copy(out, "Windows 2000", 32);
    else if (maj == 5) str_copy(out, "Windows XP", 32);
    else if (maj == 6 && min == 0) str_copy(out, "Windows Vista", 32);
    else if (maj == 6 && min == 1) str_copy(out, "Windows 7", 32);
    else if (maj == 6 && min == 2) str_copy(out, "Windows 8", 32);
    else if (maj == 6 && min == 3) str_copy(out, "Windows 8.1", 32);
    else if (maj == 10 && (g_ver.dwBuildNumber & 0xFFFF) >= 22000) str_copy(out, "Windows 11", 32);
    else if (maj == 10) str_copy(out, "Windows 10", 32);
    else wsprintfA(out, "Windows %d.%d", maj, min);
}

int file_exists(const char *path) {
    return path[0] && GetFileAttributesA(path) != (DWORD)-1;
}

#define RUN_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Run"

void autostart_set(int on) {
    HKEY k;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (on) {
        char exe[MAX_PATH], cmd[MAX_PATH + 16];
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        wsprintfA(cmd, "\"%s\" /tray", exe);
        RegSetValueExA(k, APP_NAME, 0, REG_SZ, (const BYTE *)cmd, (DWORD)(str_len(cmd) + 1));
    } else {
        RegDeleteValueA(k, APP_NAME);
    }
    RegCloseKey(k);
}

int autostart_get(void) {
    HKEY k;
    DWORD type, size = 0;
    int ok;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) return 0;
    ok = RegQueryValueExA(k, APP_NAME, NULL, &type, NULL, &size) == ERROR_SUCCESS;
    RegCloseKey(k);
    return ok;
}

/* ---------- storage: an INI file in %APPDATA%\Daybreak Classic (or next to the program) ---------- */

static char g_ini[MAX_PATH];

void store_init(void) {
    char dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("APPDATA", dir, MAX_PATH);
    if (n > 0 && n < MAX_PATH - 40) {
        str_cat(dir, "\\Daybreak Classic", MAX_PATH);
        CreateDirectoryA(dir, NULL);
    } else {
        /* Windows 95/98 without user profiles: keep settings beside the program. */
        int i;
        GetModuleFileNameA(NULL, dir, MAX_PATH);
        for (i = str_len(dir) - 1; i > 0 && dir[i] != '\\'; i--) {}
        dir[i] = 0;
    }
    str_copy(g_ini, dir, MAX_PATH);
    str_cat(g_ini, "\\daybreak.ini", MAX_PATH);
}

static int ini_int(const char *sec, const char *key, int def) {
    return (int)GetPrivateProfileIntA(sec, key, def, g_ini);
}

static void ini_str(const char *sec, const char *key, char *out, int n) {
    GetPrivateProfileStringA(sec, key, "", out, (DWORD)n, g_ini);
}

static void ini_put_int(const char *sec, const char *key, int v) {
    char buf[16];
    wsprintfA(buf, "%d", v);
    WritePrivateProfileStringA(sec, key, buf, g_ini);
}

static void ini_put_str(const char *sec, const char *key, const char *v) {
    WritePrivateProfileStringA(sec, key, v, g_ini);
}

void store_load(void) {
    int i;
    char buf[128];
    const char *p;
    S.theme = ini_int("settings", "theme", TH_CLASSIC);
    if (S.theme < 0 || S.theme >= TH_COUNT) S.theme = TH_CLASSIC;
    S.use24 = ini_int("settings", "use24", 0);
    S.showSec = ini_int("settings", "showSeconds", 1);
    S.celsius = ini_int("settings", "celsius", 0);
    S.bedOn = ini_int("settings", "bedOn", 0);
    S.bedH = ini_int("settings", "bedHour", 23);
    S.bedM = ini_int("settings", "bedMinute", 0);
    S.wakeH = ini_int("settings", "wakeHour", 6);
    S.wakeM = ini_int("settings", "wakeMinute", 30);
    S.remind = ini_int("settings", "remind", 30);
    S.bedDays = ini_int("settings", "bedDays", 0x1F); /* Sun-Thu */
    S.bedLink = ini_int("settings", "bedLink", 0);
    S.bedAlarm = ini_int("settings", "bedAlarm", -1);
    S.trayHint = ini_int("settings", "trayHint", 0);
    ini_str("settings", "city", S.city, sizeof(S.city));
    S.haveLoc = ini_int("settings", "haveLoc", 0);
    ini_str("settings", "lat", buf, sizeof(buf)); S.latE4 = parse_int(buf);
    ini_str("settings", "lon", buf, sizeof(buf)); S.lonE4 = parse_int(buf);
    ini_str("settings", "timerSound", S.timerSound, MAX_PATH);

    S.zoneCount = 0;
    ini_str("settings", "zones", buf, sizeof(buf));
    if (!buf[0] && !ini_int("settings", "zonesSet", 0)) {
        str_copy(buf, "14,44", sizeof(buf)); /* London, Tokyo */
    }
    p = buf;
    while (*p && S.zoneCount < MAX_ZONES) {
        int z = parse_int(p);
        if (z >= 0 && z < CITY_COUNT) S.zones[S.zoneCount++] = z;
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }

    for (i = 0; i < MAX_ALARMS; i++) {
        char sec[16];
        Alarm *a = &A[i];
        wsprintfA(sec, "alarm%d", i);
        a->used = ini_int(sec, "used", 0);
        if (!a->used) continue;
        a->hour = ini_int(sec, "hour", 7);
        a->minute = ini_int(sec, "minute", 0);
        a->days = ini_int(sec, "days", 0);
        a->enabled = ini_int(sec, "enabled", 1);
        a->snooze = ini_int(sec, "snooze", 10);
        a->gradual = ini_int(sec, "gradual", 1);
        a->ramp = ini_int(sec, "ramp", 60);
        a->skipUntil = ini_int(sec, "skip", 0);
        ini_str(sec, "label", a->label, sizeof(a->label));
        ini_str(sec, "sound", a->sound, MAX_PATH);
    }
}

void store_save(void) {
    int i;
    char buf[128];
    ini_put_int("settings", "theme", S.theme);
    ini_put_int("settings", "use24", S.use24);
    ini_put_int("settings", "showSeconds", S.showSec);
    ini_put_int("settings", "celsius", S.celsius);
    ini_put_int("settings", "bedOn", S.bedOn);
    ini_put_int("settings", "bedHour", S.bedH);
    ini_put_int("settings", "bedMinute", S.bedM);
    ini_put_int("settings", "wakeHour", S.wakeH);
    ini_put_int("settings", "wakeMinute", S.wakeM);
    ini_put_int("settings", "remind", S.remind);
    ini_put_int("settings", "bedDays", S.bedDays);
    ini_put_int("settings", "bedLink", S.bedLink);
    ini_put_int("settings", "bedAlarm", S.bedAlarm);
    ini_put_int("settings", "trayHint", S.trayHint);
    ini_put_str("settings", "city", S.city);
    ini_put_int("settings", "haveLoc", S.haveLoc);
    ini_put_int("settings", "lat", S.latE4);
    ini_put_int("settings", "lon", S.lonE4);
    ini_put_str("settings", "timerSound", S.timerSound);
    buf[0] = 0;
    for (i = 0; i < S.zoneCount; i++) {
        char n[8];
        wsprintfA(n, i ? ",%d" : "%d", S.zones[i]);
        str_cat(buf, n, sizeof(buf));
    }
    ini_put_str("settings", "zones", buf);
    ini_put_int("settings", "zonesSet", 1);

    for (i = 0; i < MAX_ALARMS; i++) {
        char sec[16];
        Alarm *a = &A[i];
        wsprintfA(sec, "alarm%d", i);
        if (!a->used) {
            WritePrivateProfileStringA(sec, NULL, NULL, g_ini); /* remove the section */
            continue;
        }
        ini_put_int(sec, "used", 1);
        ini_put_int(sec, "hour", a->hour);
        ini_put_int(sec, "minute", a->minute);
        ini_put_int(sec, "days", a->days);
        ini_put_int(sec, "enabled", a->enabled);
        ini_put_int(sec, "snooze", a->snooze);
        ini_put_int(sec, "gradual", a->gradual);
        ini_put_int(sec, "ramp", a->ramp);
        ini_put_int(sec, "skip", a->skipUntil);
        ini_put_str(sec, "label", a->label);
        ini_put_str(sec, "sound", a->sound);
    }
    WritePrivateProfileStringA(NULL, NULL, NULL, g_ini); /* flush (Windows 9x caches INI files) */
}

int alarm_new(void) {
    int i;
    for (i = 0; i < MAX_ALARMS; i++) {
        if (!A[i].used) {
            memset(&A[i], 0, sizeof(Alarm));
            A[i].used = 1;
            A[i].hour = 7;
            A[i].minute = 0;
            A[i].days = 0x3E;
            A[i].enabled = 1;
            A[i].snooze = 10;
            A[i].gradual = 1;
            A[i].ramp = 60;
            return i;
        }
    }
    return -1;
}
