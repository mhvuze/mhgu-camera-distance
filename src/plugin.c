// Camera Distance: how far the quest camera stays behind the hunter and behind a Prowler, as a Forge plugin for MHGU.
//
// The quest camera (uQuestCamera) reads its numbers from camera\param\questcamera in quest.arc every frame, through
// one getter (camera, index): index 0 is the hunter's distance (490 in the game's file), 2 a Prowler's (360). Mods
// that edit that file need a copy of quest.arc per distance and a restart; this plugin hooks the getter instead and
// scales those two values, so a change shows at once and still applies on top of an edited file.
//
// Settings: Options > Mod Options > Camera Distance when Purple-lib is installed, and Forge's menu (Insert > Camera
// Distance) on MHGU Recomp. They are saved next to the game's save, as save_0/camdist.ini, which can also be edited by
// hand.
//
// Addresses are offsets into the game's main module (MHGU 1.4.0), added to forge_mem_getMainTextAddr() at runtime.
#include "forge.h"
#include "purplelib.h"

#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "dev"
#endif

// ================================================================== game

// uQuestCamera: one of its numbers (camera, index) -> s0. It asks the camera's parameter resource (camera +0x94,
// the loaded questcamera file) through its vfunc +0x4c. Only uQuestCamera's own code calls it.
#define CAM_PARAM 0x1EADC
#define PARAM_HUNTER_DISTANCE 0   // the update picks index 2 instead while the player is riding as a Prowler
#define PARAM_PROWLER_DISTANCE 2  // (and 1 / 3, the heights, the same way)

// ================================================================== settings

#define PCT_MIN 30
#define PCT_MAX 300
#define PCT_STEP 5
#define PCT_COUNT ((PCT_MAX - PCT_MIN) / PCT_STEP + 1)
#define PCT_DEFAULT 100

enum { HUNTER, PROWLER, KINDS };
static const char *const KEYS[KINDS] = {"hunter", "prowler"};

// The percentages, as the settings windows show them. get/set run inside the game's update: they only touch these.
static volatile int g_pct[KINDS] = {PCT_DEFAULT, PCT_DEFAULT};
static volatile bool g_save_pending;
static u32 g_save_delay;  // frames left before an overlay change is saved (a slider moves every frame)

static int clamp_pct(int v) {
    if (v < PCT_MIN) return PCT_MIN;
    if (v > PCT_MAX) return PCT_MAX;
    return v;
}

// The compiler may emit memset/memcpy calls; there is no C library to provide them
void *memset(void *d, int c, u32 n) {
    u8 *p = d;
    while (n--) *p++ = (u8)c;
    return d;
}
void *memcpy(void *d, const void *s, u32 n) {
    u8 *p = d;
    const u8 *q = s;
    while (n--) *p++ = *q++;
    return d;
}

// ---------------------------------------------------------------- the settings file

// In the game's save data, next to the save (save_0/camdist.ini), as Forge's own and other plugins' settings are. The
// save data is mounted for each read or write and unmounted (which commits) right after, as Forge's API asks.
#define FILE_NAME "camdist.ini"
static char g_path[64];
static bool g_loaded;  // the file was read, or the save data had none

// Mounts the save data and makes g_path; false when it can't be mounted (yet)
static bool mount(void) {
    if (!forge_fs_mountSaveData()) return false;
    const char *mp = forge_fs_getSaveDataMountPoint();
    u32 i = 0;
    for (; mp && mp[i] && i < sizeof g_path - sizeof ":/" FILE_NAME; i++) g_path[i] = mp[i];
    for (const char *t = ":/" FILE_NAME; *t; t++) g_path[i++] = *t;
    g_path[i] = 0;
    return true;
}

static bool is_space(char c) { return c == ' ' || c == '\t'; }

// "hunter = 120" lines; anything else (comments, unknown keys, bad numbers) is skipped
static void parse(const char *s, const char *end) {
    while (s < end) {
        const char *line = s;
        while (s < end && *s != '\n') s++;
        const char *eol = s++;
        while (line < eol && is_space(*line)) line++;
        for (int k = 0; k < KINDS; k++) {
            const char *p = line, *key = KEYS[k];
            while (*key && p < eol && *p == *key) p++, key++;
            if (*key) continue;
            while (p < eol && is_space(*p)) p++;
            if (p == eol || *p++ != '=') continue;
            while (p < eol && is_space(*p)) p++;
            int v = 0, digits = 0;
            while (p < eol && *p >= '0' && *p <= '9' && digits < 6) v = v * 10 + (*p++ - '0'), digits++;
            if (digits) g_pct[k] = clamp_pct(v);
        }
    }
}

// Reads the settings file; false when the save data couldn't be mounted (tried again later)
static bool load(void) {
    static char buf[1024];
    FsFile f;
    s64 size = 0;
    if (!mount()) return false;
    u32 n = 0;
    bool found = fs_open(&f, g_path, FS_READ) == 0;
    if (found) {
        if (fs_size(&size, f) == 0 && size > 0) {
            n = size < (s64)sizeof buf ? (u32)size : sizeof buf;
            if (fs_read(f, 0, buf, n) != 0) n = 0;
        }
        fs_close(f);
    }
    forge_fs_unmountSaveData();
    parse(buf, buf + n);
    if (found)
        forge_log(LOG_INFO, "camera-distance: settings loaded (hunter %d%%, prowler %d%%)", g_pct[HUNTER], g_pct[PROWLER]);
    else
        forge_log(LOG_INFO, "camera-distance: no " FILE_NAME " yet, both distances at 100%%");
    return true;
}

static char *put(char *q, const char *s) {
    while (*s) *q++ = *s++;
    return q;
}

// A number in decimal without dividing (division would need libgcc)
static char *put_u(char *q, u32 v) {
    static const u32 tens[] = {100000, 10000, 1000, 100, 10, 1};
    bool started = false;
    for (u32 i = 0; i < sizeof tens / sizeof *tens; i++) {
        char d = '0';
        while (v >= tens[i]) v -= tens[i], d++;
        if (d != '0' || started || tens[i] == 1) *q++ = d, started = true;
    }
    return q;
}

static void save(void) {
    static char buf[512];
    char *q = put(buf, "; Camera Distance " PLUGIN_VERSION ": the quest camera's distance, in percent of the game's (");
    q = put_u(q, PCT_MIN);
    q = put(q, " to ");
    q = put_u(q, PCT_MAX);
    q = put(q, ").\n");
    for (int k = 0; k < KINDS; k++) {
        q = put(q, KEYS[k]);
        q = put(q, " = ");
        q = put_u(q, (u32)g_pct[k]);
        q = put(q, "\n");
    }
    u32 n = (u32)(q - buf);

    FsFile f;
    if (!mount()) {
        forge_log(LOG_WARN, "camera-distance: could not mount the save data, settings not saved");
        return;
    }
    bool ok = fs_open(&f, g_path, FS_WRITE | FS_APPEND) == 0;
    if (!ok) {
        fs_create(g_path, 0);
        ok = fs_open(&f, g_path, FS_WRITE | FS_APPEND) == 0;
    }
    if (ok) {
        static const FsWriteOption flush = {1};
        ok = fs_set_size(f, n) == 0 && fs_write(f, 0, buf, n, &flush) == 0;
        fs_close(f);
    }
    forge_fs_unmountSaveData();
    forge_log(ok ? LOG_INFO : LOG_WARN, "camera-distance: %s (hunter %d%%, prowler %d%%)",
              ok ? "settings saved" : "settings NOT saved", g_pct[HUNTER], g_pct[PROWLER]);
}

// ================================================================== the camera

static u32 g_main;
static ForgeHook g_param_hook;
static bool g_hooked;

typedef float (*ParamFn)(u32 camera, u32 index);

static float param_hook(u32 camera, u32 index) {
    float v = ((ParamFn)g_param_hook.original)(camera, index);
    if (index == PARAM_HUNTER_DISTANCE) return v * (float)g_pct[HUNTER] * 0.01f;
    if (index == PARAM_PROWLER_DISTANCE) return v * (float)g_pct[PROWLER] * 0.01f;
    return v;
}

// ================================================================== Purple-lib: Options > Mod Options

static const PurpleLibApi *g_api;

static const char *const RESET[] = {"Reset"};

static const char *title(void *ctx, u32 language) {
    (void)ctx, (void)language;
    return "Camera Distance";
}

static bool row(void *ctx, u32 language, u32 index, PurpleLibOptionRow *out) {
    (void)ctx, (void)language;
    if (index < KINDS) {
        out->name = index == HUNTER ? "Hunter" : "Prowler";
        out->help = index == HUNTER ? "How far the camera stays behind your hunter."
                                    : "How far the camera stays behind your Prowler.";
        out->kind = PURPLELIB_ROW_NUMBERS;
        out->value_count = PCT_COUNT;
        out->first = PCT_MIN;
        out->step = PCT_STEP;
        out->suffix = "%";
        out->flags = PURPLELIB_ROW_NO_WRAP;
        return true;
    }
    if (index == KINDS) {
        out->name = "Defaults";
        out->help = "Puts both distances back to 100%.";
        out->kind = PURPLELIB_ROW_ACTION;
        out->labels = RESET;
        out->label_count = 1;
        return true;
    }
    return false;
}

// value <-> percentage without dividing: value = (pct - PCT_MIN) / PCT_STEP
static u32 get(void *ctx, u32 index) {
    (void)ctx;
    if (index >= KINDS) return 0;
    u32 value = 0;
    for (int p = PCT_MIN + PCT_STEP; p <= g_pct[index] && value + 1 < PCT_COUNT; p += PCT_STEP) value++;
    return value;
}

static void set(void *ctx, u32 index, u32 value) {
    (void)ctx;
    if (index < KINDS && value < PCT_COUNT) g_pct[index] = PCT_MIN + (int)value * PCT_STEP;
}

static void action(void *ctx, u32 index) {
    (void)ctx;
    if (index != KINDS) return;
    g_pct[HUNTER] = g_pct[PROWLER] = PCT_DEFAULT;
    g_save_pending = true;
}

static void closed(void *ctx) {
    (void)ctx;
    g_save_pending = true;
}

static const char *const PAGES[] = {
    "Hunter and Prowler set how far the\r\ncamera stays behind you on quests,\r\nin percent of the game's distance.",
    "Changes show at once. They are saved\r\nto camdist.ini next to your save when\r\nyou close the window.",
};
static const char *help_title(void *ctx, u32 language) {
    (void)ctx, (void)language;
    return "Camera Distance";
}
static const char *help_page(void *ctx, u32 language, u32 index) {
    (void)ctx, (void)language;
    return index < sizeof PAGES / sizeof *PAGES ? PAGES[index] : NULL;
}

static const PurpleLibOptions OPTIONS = {
    sizeof(PurpleLibOptions), NULL, title, KINDS + 1, row, get, set, closed, NULL, action,
};
static const PurpleLibHelp SET_HELP = {
    sizeof(PurpleLibHelp), NULL, 0, sizeof PAGES / sizeof *PAGES, help_title, help_page, NULL,
};

EXPORT void purplelib_on_register(const PurpleLibApi *api) {
    if (!api || api->version < PURPLELIB_API_VERSION) {
        forge_log(LOG_WARN, "camera-distance: Purple-lib is too old, no Mod Options page");
        return;
    }
    g_api = api;
    bool options = api->add_options(&OPTIONS);
    if (options) api->add_set_help(&OPTIONS, &SET_HELP);
    forge_log(options ? LOG_INFO : LOG_WARN, "camera-distance: %s",
              options ? "settings under Options > Mod Options" : "Purple-lib refused the Mod Options page");
}

// ================================================================== Forge

EXPORT void forge_onLoad(ForgePluginInfo *info) {
    static const char name[] = "Camera Distance " PLUGIN_VERSION;
    if (!info) return;
    info->major = 1, info->minor = 5, info->patch = 1;  // (the Forge API version it was written for)
    memset(info->name, 0, sizeof info->name);
    for (u32 i = 0; i < sizeof name; i++) info->name[i] = name[i];
}

EXPORT void forge_onInit(void) {
    g_main = forge_mem_getMainTextAddr();
    g_loaded = load();
    u32 original = 0;  // (non-null asks Forge for a trampoline to the original)
    forge_hook_create(&g_param_hook, g_main + CAM_PARAM, (u32)param_hook, &original);
    g_hooked = g_param_hook.original != 0;
    forge_log(g_hooked ? LOG_INFO : LOG_ERROR, "camera-distance: %s",
              g_hooked ? "quest camera hooked" : "quest camera hook FAILED, the game's distance stays");
}

EXPORT void forge_onUpdate(float dt) {
    (void)dt;
    static u32 retry;
    if (!g_loaded && ++retry >= 60) retry = 0, g_loaded = load();  // the save data wasn't mountable at init
    if (g_save_delay && !--g_save_delay) g_save_pending = true;
    if (g_save_pending) {
        g_save_pending = false;
        save();
    }
}

EXPORT void forge_onImGuiInit(void *ctx, void *alloc, void *freefn) {
    if (!ctx || !alloc || !freefn) return;
    igSetCurrentContext(ctx);
    igSetAllocatorFunctions(alloc, freefn, NULL);
}

// Forge's menu (Insert on MHGU Recomp): the same settings, for players without Purple-lib
EXPORT void forge_onImGuiRender(void) {
    if (!igTreeNode_Str("Camera Distance###camera-distance")) return;
    if (!g_hooked) igTextUnformatted("Hook failed: the game's distance stays (see the log).", NULL);
    for (int k = 0; k < KINDS; k++) {
        int v = g_pct[k];
        if (igSliderInt(k == HUNTER ? "Hunter###cd-hunter" : "Prowler###cd-prowler", &v, PCT_MIN, PCT_MAX, "%d%%", 0)) {
            g_pct[k] = clamp_pct(v);
            g_save_delay = 60;
        }
    }
    igTextUnformatted("100% is the game's distance. Saved to camdist.ini next to your save.", NULL);
    igTreePop();
}
