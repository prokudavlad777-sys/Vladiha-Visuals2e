// boosterik 1.12
// Єдина програма з трьома розділами: Візуалізація (приціл), Оптимізатор, Кольорокоректор.
// Зверху напис "boosterik 1.12" - по кліку відкривається меню розділів.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
using std::min;
using std::max;

#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <powrprof.h>
#include <shellapi.h>

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>

using namespace Gdiplus;

// ============================================================================
//  Константи та кольори
// ============================================================================
static const wchar_t* APP_TITLE = L"boosterik 1.12";
static const float WIN_W = 900.f, WIN_H = 616.f, HDR_H = 72.f;
static const float PI_F = 3.14159265358979f;

#define COL_BG     0xFF0A0A10u
#define COL_PANEL  0xFF12121Au
#define COL_PANEL2 0xFF1A1A25u
#define COL_BORDER 0xFF272736u
#define COL_TEXT   0xFFF2F4FAu
#define COL_MUTED  0xFF8B8FA3u
#define COL_ACC    0xFF5B8CFFu
#define COL_ACC2   0xFF8B6BFFu
#define COL_GREEN  0xFF3DDC84u
#define COL_RED    0xFFE5484Du

enum Page { PG_OPT = 0, PG_VIS = 1, PG_COL = 2 };

enum {
    ID_NONE = -1,
    ID_MENU_BTN = 1, ID_MIN, ID_CLOSE, ID_MENU_AWAY, ID_MENU_VIS, ID_MENU_OPT, ID_MENU_COL,
    ID_OPT_GO = 20, ID_OPT_RESTORE,
    ID_T0 = 30,            // 30..35 перемикачі оптимізатора
    ID_T_AUTO = 36,
    ID_VIS_ENABLE = 40, ID_VIS_FOLDER, ID_VIS_REFRESH,
    ID_ANIM0 = 44,         // 44..46
    ID_SW0 = 50,           // 50..57 кольори прицілу
    ID_SL_SIZE = 60, ID_SL_THICK, ID_SL_GAP, ID_SL_OPAC, ID_SL_SPEED,
    ID_SL_GAMMA = 70, ID_SL_SAT, ID_SL_EXP, ID_SL_CON, ID_SL_TEMP, ID_SL_HUE,
    ID_COL_RESET = 80, ID_COL_APPLY,
    ID_TILE0 = 1000
};

// ============================================================================
//  Глобальний стан
// ============================================================================
static HINSTANCE g_inst = nullptr;
static HWND g_hwnd = nullptr, g_ov = nullptr;
static float g_S = 1.f;
static Page g_page = PG_OPT;
static bool g_menuOpen = false;
static float g_mx = -1, g_my = -1;
static int g_hot = ID_NONE, g_press = ID_NONE, g_drag = ID_NONE;
static bool g_tracking = false;

struct Hit { RectF r; int id; };
static std::vector<Hit> g_hits;

static ULONG_PTR g_gdiplusToken = 0;
static FontFamily* g_ff = nullptr;
static FontFamily* g_mono = nullptr;
static Bitmap* g_logo = nullptr;

// ---- Приціл ----
struct Cross {
    bool enabled = false;
    int type = 1;
    float size = 48, thick = 2, gap = 6, opacity = 100, speed = 90;
    int anim = 0;   // 0 - немає, 1 - обертання, 2 - пульсація
    int color = 0;
};
static Cross g_cross;
static const DWORD g_swatch[8] = { 0xFF00FF66, 0xFFFFFFFF, 0xFFFF3B3B, 0xFF00E5FF,
                                   0xFFFFE600, 0xFFFF3DF2, 0xFFFF8A00, 0xFF4D7CFF };
struct CustomCH { std::wstring name; Bitmap* bmp; };
static std::vector<CustomCH> g_custom;
static int g_tileScroll = 0;

// ---- Кольорокоректор ----
struct ColorP { float gamma = 1.f, sat = 100.f, exposure = 0.f, contrast = 100.f, temp = 0.f, hue = 0.f; };
static ColorP g_col;
static bool g_sphereDirty = true;
static const int SPH = 240;
static uint32_t g_sphereBuf[SPH * SPH];
static Bitmap* g_sphere = nullptr;
static std::wstring g_colStatus = L"Не застосовано до екрана";
static float g_M[5][5];
static WORD g_origRamp[3][256];
static bool g_haveOrig = false;

// ---- Оптимізатор ----
struct Game { const wchar_t* name; std::vector<const wchar_t*> exes; };
static Game g_games[6] = {
    { L"Rust",           { L"RustClient.exe" } },
    { L"CS 2",           { L"cs2.exe" } },
    { L"ARK",            { L"ShooterGame.exe", L"ArkAscended.exe" } },
    { L"Minecraft",      { L"javaw.exe", L"Minecraft.Windows.exe" } },
    { L"World of Tanks", { L"WorldOfTanks.exe" } },
    { L"GTA V",          { L"GTA5.exe", L"GTA5_Enhanced.exe" } },
};
static const wchar_t* g_optNames[6] = {
    L"Високий пріоритет гри", L"Розподіл ядер CPU", L"Очищення пам'яті (RAM)",
    L"Максимальна продуктивність", L"Мережа: Nagle / ACK", L"Очистка тимчасових файлів" };
static bool g_opt[6] = { true, true, true, true, true, true };
static bool g_auto = false;
static volatile bool g_gameRun[6] = {};
static volatile bool g_gameOpt[6] = {};
static volatile LONG g_running = 0;
static volatile int g_progress = 0;
static volatile LONG g_dirty = 0;
static CRITICAL_SECTION g_cs;
static std::vector<std::wstring> g_log;
static std::wstring g_status = L"Готово до оптимізації";
static GUID g_prevScheme;
static bool g_havePrev = false;

// ============================================================================
//  Слайдери
// ============================================================================
struct SliderDef { int id; float* v; float mn, mx, step, def; const wchar_t* fmt; };
static SliderDef g_sl[] = {
    { ID_SL_SIZE,  &g_cross.size,    8,    200,  1,     48,  L"%.0f px" },
    { ID_SL_THICK, &g_cross.thick,   1,    10,   0.5f,  2,   L"%.1f px" },
    { ID_SL_GAP,   &g_cross.gap,     0,    60,   1,     6,   L"%.0f px" },
    { ID_SL_OPAC,  &g_cross.opacity, 10,   100,  1,     100, L"%.0f%%" },
    { ID_SL_SPEED, &g_cross.speed,   10,   720,  10,    90,  L"%.0f °/с" },
    { ID_SL_GAMMA, &g_col.gamma,     0.5f, 2.5f, 0.01f, 1,   L"%.2f" },
    { ID_SL_SAT,   &g_col.sat,       0,    200,  1,     100, L"%.0f%%" },
    { ID_SL_EXP,   &g_col.exposure,  -2,   2,    0.05f, 0,   L"%.2f EV" },
    { ID_SL_CON,   &g_col.contrast,  50,   150,  1,     100, L"%.0f%%" },
    { ID_SL_TEMP,  &g_col.temp,      -100, 100,  1,     0,   L"%.0f" },
    { ID_SL_HUE,   &g_col.hue,       -180, 180,  1,     0,   L"%.0f°" },
};
static SliderDef* FindSlider(int id) {
    for (auto& d : g_sl) if (d.id == id) return &d;
    return nullptr;
}

// ============================================================================
//  Утиліти
// ============================================================================
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static std::wstring ExeDir() {
    wchar_t b[MAX_PATH];
    GetModuleFileNameW(nullptr, b, MAX_PATH);
    std::wstring s = b;
    size_t p = s.find_last_of(L'\\');
    return p == std::wstring::npos ? s : s.substr(0, p);
}

static void Log(const std::wstring& s) {
    SYSTEMTIME t; GetLocalTime(&t);
    wchar_t b[24];
    swprintf(b, 24, L"[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    EnterCriticalSection(&g_cs);
    g_log.push_back(std::wstring(b) + s);
    if (g_log.size() > 200) g_log.erase(g_log.begin());
    LeaveCriticalSection(&g_cs);
    InterlockedExchange(&g_dirty, 1);
}
static void SetStatus(const std::wstring& s) {
    EnterCriticalSection(&g_cs);
    g_status = s;
    LeaveCriticalSection(&g_cs);
    InterlockedExchange(&g_dirty, 1);
}

// ---- налаштування (ini у %APPDATA%\Boosterik) ----
static std::wstring g_ini;
static int RI(const wchar_t* k, int d) { return (int)GetPrivateProfileIntW(L"boosterik", k, d, g_ini.c_str()); }
static void WI(const wchar_t* k, int v) {
    wchar_t b[32]; swprintf(b, 32, L"%d", v);
    WritePrivateProfileStringW(L"boosterik", k, b, g_ini.c_str());
}
static float RF(const wchar_t* k, float d) { return RI(k, (int)lroundf(d * 100.f)) / 100.f; }
static void WF(const wchar_t* k, float v) { WI(k, (int)lroundf(v * 100.f)); }

static void LoadSettings() {
    wchar_t b[MAX_PATH];
    GetEnvironmentVariableW(L"APPDATA", b, MAX_PATH);
    std::wstring d = std::wstring(b) + L"\\Boosterik";
    CreateDirectoryW(d.c_str(), nullptr);
    g_ini = d + L"\\settings.ini";
    for (int i = 0; i < 6; i++) { wchar_t k[16]; swprintf(k, 16, L"opt%d", i); g_opt[i] = RI(k, 1) != 0; }
    g_auto = RI(L"auto", 0) != 0;
    g_cross.enabled = RI(L"ch_on", 0) != 0;
    g_cross.type = RI(L"ch_type", 1);
    g_cross.anim = RI(L"ch_anim", 0);
    g_cross.color = RI(L"ch_color", 0);
    for (auto& s : g_sl) { wchar_t k[16]; swprintf(k, 16, L"sl%d", s.id); *s.v = clampf(RF(k, s.def), s.mn, s.mx); }
    if (g_cross.color < 0 || g_cross.color > 7) g_cross.color = 0;
    if (g_cross.anim < 0 || g_cross.anim > 2) g_cross.anim = 0;
}
static void SaveSettings() {
    for (int i = 0; i < 6; i++) { wchar_t k[16]; swprintf(k, 16, L"opt%d", i); WI(k, g_opt[i]); }
    WI(L"auto", g_auto);
    WI(L"ch_on", g_cross.enabled);
    WI(L"ch_type", g_cross.type);
    WI(L"ch_anim", g_cross.anim);
    WI(L"ch_color", g_cross.color);
    for (auto& s : g_sl) { wchar_t k[16]; swprintf(k, 16, L"sl%d", s.id); WF(k, *s.v); }
}

// ============================================================================
//  GDI+ помічники
// ============================================================================
static Font* GetFont(float size, bool bold) {
    static std::map<int, Font*> m;
    int k = (int)(size * 10) * 2 + (bold ? 1 : 0);
    auto it = m.find(k);
    if (it != m.end()) return it->second;
    Font* f = new Font(g_ff, size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    m[k] = f;
    return f;
}
static Font* GetMono(float size) {
    static std::map<int, Font*> m;
    int k = (int)(size * 10);
    auto it = m.find(k);
    if (it != m.end()) return it->second;
    Font* f = new Font(g_mono, size, FontStyleRegular, UnitPixel);
    m[k] = f;
    return f;
}

static void TxtR(Graphics& g, const std::wstring& s, RectF r, float size, DWORD col, bool bold = false,
                 StringAlignment ha = StringAlignmentNear, StringAlignment va = StringAlignmentCenter,
                 Font* font = nullptr) {
    StringFormat sf;
    sf.SetAlignment(ha);
    sf.SetLineAlignment(va);
    sf.SetTrimming(StringTrimmingEllipsisCharacter);
    sf.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush b(Color((ARGB)col));
    g.DrawString(s.c_str(), -1, font ? font : GetFont(size, bold), r, &sf, &b);
}
static void TxtW(Graphics& g, const std::wstring& s, RectF r, float size, DWORD col) {
    StringFormat sf;
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentNear);
    SolidBrush b(Color((ARGB)col));
    g.DrawString(s.c_str(), -1, GetFont(size, false), r, &sf, &b);
}

static void RRPath(GraphicsPath& p, RectF r, float rad) {
    float d = rad * 2;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    p.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    p.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    p.CloseFigure();
}
static void FillRR(Graphics& g, RectF r, float rad, DWORD col) {
    GraphicsPath p; RRPath(p, r, rad);
    SolidBrush b(Color((ARGB)col));
    g.FillPath(&b, &p);
}
static void FillRRGrad(Graphics& g, RectF r, float rad, DWORD c1, DWORD c2) {
    GraphicsPath p; RRPath(p, r, rad);
    LinearGradientBrush b(r, Color(c1), Color(c2), LinearGradientModeHorizontal);
    g.FillPath(&b, &p);
}
static void StrokeRR(Graphics& g, RectF r, float rad, DWORD col, float w = 1.f) {
    GraphicsPath p; RRPath(p, r, rad);
    Pen pen(Color((ARGB)col), w);
    g.DrawPath(&pen, &p);
}
static void DrawBolt(Graphics& g, float cx, float cy, float s, DWORD col) {
    PointF p[6] = { PointF(0.15f, -1.0f), PointF(-0.55f, 0.12f), PointF(-0.08f, 0.12f),
                    PointF(-0.25f, 1.0f), PointF(0.55f, -0.2f), PointF(0.08f, -0.2f) };
    for (auto& q : p) { q.X = cx + q.X * s; q.Y = cy + q.Y * s; }
    SolidBrush b(Color((ARGB)col));
    g.FillPolygon(&b, p, 6);
}

// ============================================================================
//  Віджети (immediate mode: хіт-регіони реєструються під час малювання)
// ============================================================================
static void AddHit(int id, RectF r) { Hit h; h.r = r; h.id = id; g_hits.push_back(h); }
static bool Hot(int id) { return g_hot == id || g_press == id; }
static int HitAt(float x, float y) {
    for (int i = (int)g_hits.size() - 1; i >= 0; --i)
        if (g_hits[i].r.Contains(x, y)) return g_hits[i].id;
    return ID_NONE;
}

static void Card(Graphics& g, RectF r, const wchar_t* title, const wchar_t* sub = nullptr) {
    FillRR(g, r, 16, COL_PANEL);
    StrokeRR(g, r, 16, COL_BORDER, 1);
    if (title) TxtR(g, title, RectF(r.X + 24, r.Y + 14, r.Width - 48, 26), 17, COL_TEXT, true);
    if (sub) TxtR(g, sub, RectF(r.X + 24, r.Y + 38, r.Width - 48, 20), 12, COL_MUTED);
}

static void Button(Graphics& g, int id, RectF r, const wchar_t* text, bool primary, float size = 14) {
    bool hot = Hot(id);
    if (primary) {
        FillRRGrad(g, r, 12, hot ? 0xFF7AA2FF : COL_ACC, hot ? 0xFFA48AFF : COL_ACC2);
    } else {
        FillRR(g, r, 12, hot ? 0xFF242433 : COL_PANEL2);
        StrokeRR(g, r, 12, hot ? 0xFF3A3A50 : COL_BORDER, 1);
    }
    TxtR(g, text, r, size, COL_TEXT, true, StringAlignmentCenter, StringAlignmentCenter);
    AddHit(id, r);
}

static void Toggle(Graphics& g, int id, RectF r, const wchar_t* label, bool on) {
    RectF sw(r.X, r.Y + (r.Height - 22) / 2, 40, 22);
    FillRR(g, sw, 11, on ? COL_ACC : 0xFF2C2C3B);
    float kx = on ? sw.X + 40 - 20 : sw.X + 2;
    SolidBrush kb(Color(0xFFFFFFFF));
    g.FillEllipse(&kb, kx, sw.Y + 2, 18.f, 18.f);
    TxtR(g, label, RectF(r.X + 50, r.Y, r.Width - 50, r.Height), 13, Hot(id) ? 0xFFFFFFFF : 0xFFD6D9E6);
    AddHit(id, r);
}

static void Slider(Graphics& g, int id, RectF r, const wchar_t* label) {
    SliderDef* d = FindSlider(id);
    if (!d) return;
    float t = (*d->v - d->mn) / (d->mx - d->mn);
    wchar_t buf[48];
    swprintf(buf, 48, d->fmt, *d->v);
    TxtR(g, label, RectF(r.X, r.Y, r.Width - 100, 18), 13, 0xFFD6D9E6);
    TxtR(g, buf, RectF(r.X + r.Width - 100, r.Y, 100, 18), 13, COL_TEXT, true, StringAlignmentFar);
    float ty = r.Y + 29;
    RectF track(r.X, ty - 3, r.Width, 6);
    FillRR(g, track, 3, 0xFF2C2C3B);
    float kx = r.X + t * r.Width;
    if (kx - r.X > 1) FillRRGrad(g, RectF(r.X, ty - 3, kx - r.X, 6), 3, COL_ACC, COL_ACC2);
    SolidBrush kb(Color(0xFFFFFFFF));
    float kr = (Hot(id) || g_drag == id) ? 9.f : 8.f;
    g.FillEllipse(&kb, kx - kr, ty - kr, kr * 2, kr * 2);
    AddHit(id, RectF(r.X - 8, r.Y + 18, r.Width + 16, 24));
}

// ============================================================================
//  Кольорова математика
// ============================================================================
typedef float M5[5][5];
static void Ident(M5 m) { memset(m, 0, sizeof(M5)); for (int i = 0; i < 5; i++) m[i][i] = 1.f; }
static void Mul(M5 out, const M5 a, const M5 b) {
    float t[5][5];
    for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) {
        float s = 0; for (int k = 0; k < 5; k++) s += a[i][k] * b[k][j];
        t[i][j] = s;
    }
    memcpy(out, t, sizeof(t));
}
// Рядкова конвенція: out = [r g b a 1] * M (так само, як у Magnification API)
static void BuildMatrix(const ColorP& p) {
    M5 m, t;
    Ident(m);
    // експозиція
    float e = powf(2.f, p.exposure);
    Ident(t); t[0][0] = t[1][1] = t[2][2] = e; Mul(m, m, t);
    // температура
    float k = p.temp / 100.f;
    Ident(t); t[0][0] = 1.f + 0.30f * k; t[2][2] = 1.f - 0.30f * k; Mul(m, m, t);
    // насиченість
    float s = p.sat / 100.f;
    const float lw[3] = { 0.3086f, 0.6094f, 0.0820f };
    Ident(t);
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) t[i][j] = (1.f - s) * lw[i] + (i == j ? s : 0.f);
    Mul(m, m, t);
    // тон (hue rotate)
    float a = p.hue * PI_F / 180.f, c = cosf(a), sn = sinf(a);
    float svg[3][3] = {
        { 0.213f + c * 0.787f - sn * 0.213f, 0.715f - c * 0.715f - sn * 0.715f, 0.072f - c * 0.072f + sn * 0.928f },
        { 0.213f - c * 0.213f + sn * 0.143f, 0.715f + c * 0.285f + sn * 0.140f, 0.072f - c * 0.072f - sn * 0.283f },
        { 0.213f - c * 0.213f - sn * 0.787f, 0.715f - c * 0.715f + sn * 0.715f, 0.072f + c * 0.928f + sn * 0.072f } };
    Ident(t);
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) t[j][i] = svg[i][j];
    Mul(m, m, t);
    // контраст
    float cc = p.contrast / 100.f;
    Ident(t); t[0][0] = t[1][1] = t[2][2] = cc; t[4][0] = t[4][1] = t[4][2] = 0.5f - 0.5f * cc;
    Mul(m, m, t);
    memcpy(g_M, m, sizeof(g_M));
}
static void ApplyCol(float& r, float& g, float& b) {
    float in[3] = { r, g, b }, o[3];
    for (int j = 0; j < 3; j++) {
        o[j] = in[0] * g_M[0][j] + in[1] * g_M[1][j] + in[2] * g_M[2][j] + g_M[4][j];
        o[j] = clampf(o[j], 0.f, 1.f);
        o[j] = powf(o[j], 1.f / g_col.gamma);
    }
    r = o[0]; g = o[1]; b = o[2];
}
static void HsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    h = h - floorf(h);
    float hh = h * 6.f; int i = (int)hh; float f = hh - i;
    float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
}
static void RenderSphere() {
    BuildMatrix(g_col);
    const float R = SPH * 0.5f - 4.f, cx = SPH * 0.5f, cy = SPH * 0.5f;
    float lx = -0.45f, ly = -0.55f, lz = 0.70f;
    float ll = sqrtf(lx * lx + ly * ly + lz * lz); lx /= ll; ly /= ll; lz /= ll;
    for (int y = 0; y < SPH; y++) for (int x = 0; x < SPH; x++) {
        float nx = (x + 0.5f - cx) / R, ny = (y + 0.5f - cy) / R;
        float d2 = nx * nx + ny * ny, d = sqrtf(d2);
        float aa = clampf((1.f - d) * R + 0.5f, 0.f, 1.f);
        if (aa <= 0.f) { g_sphereBuf[y * SPH + x] = 0; continue; }
        float nz = sqrtf(max(0.f, 1.f - d2));
        float hue = atan2f(ny, nx) / (2 * PI_F) + 0.5f;
        float sat = 0.35f + 0.65f * min(1.f, d);
        float r, g, b;
        HsvToRgb(hue, sat, 1.f, r, g, b);
        float nl = nx * lx + ny * ly + nz * lz;
        float diff = max(0.f, nl);
        float shade = 0.18f + 0.82f * diff;
        float rz = 2.f * nl * nz - lz;
        float spec = powf(max(0.f, rz), 32.f) * 0.55f;
        r = r * shade + spec; g = g * shade + spec; b = b * shade + spec;
        r = clampf(r, 0, 1); g = clampf(g, 0, 1); b = clampf(b, 0, 1);
        ApplyCol(r, g, b);
        uint32_t A = (uint32_t)(aa * 255.f + 0.5f);
        uint32_t R8 = (uint32_t)(r * aa * 255.f + 0.5f), G8 = (uint32_t)(g * aa * 255.f + 0.5f), B8 = (uint32_t)(b * aa * 255.f + 0.5f);
        g_sphereBuf[y * SPH + x] = (A << 24) | (R8 << 16) | (G8 << 8) | B8;
    }
}

// ---- застосування до екрана: Magnification API (динамічно) + гамма-рампа ----
struct MagEffect { float t[5][5]; };
typedef BOOL(WINAPI* PFN_MagInit)(void);
typedef BOOL(WINAPI* PFN_MagUninit)(void);
typedef BOOL(WINAPI* PFN_MagSetFS)(const MagEffect*);
static HMODULE g_magDll = nullptr;
static PFN_MagInit pMagInit = nullptr;
static PFN_MagUninit pMagUninit = nullptr;
static PFN_MagSetFS pMagSetFS = nullptr;
static bool g_magInited = false;

static bool MagSet(const M5 m) {
    if (!g_magDll) {
        g_magDll = LoadLibraryW(L"Magnification.dll");
        if (!g_magDll) return false;
        pMagInit = (PFN_MagInit)GetProcAddress(g_magDll, "MagInitialize");
        pMagUninit = (PFN_MagUninit)GetProcAddress(g_magDll, "MagUninitialize");
        pMagSetFS = (PFN_MagSetFS)GetProcAddress(g_magDll, "MagSetFullscreenColorEffect");
    }
    if (!pMagInit || !pMagSetFS) return false;
    if (!g_magInited) { if (!pMagInit()) return false; g_magInited = true; }
    MagEffect e;
    memcpy(e.t, m, sizeof(e.t));
    return pMagSetFS(&e) != FALSE;
}
static bool SetGammaRamp(float gamma) {
    WORD r[3][256];
    bool identity = fabsf(gamma - 1.f) < 0.001f;
    for (int i = 0; i < 256; i++) {
        WORD w;
        if (identity && g_haveOrig) { r[0][i] = g_origRamp[0][i]; r[1][i] = g_origRamp[1][i]; r[2][i] = g_origRamp[2][i]; continue; }
        float v = powf(i / 255.f, 1.f / gamma);
        w = (WORD)(clampf(v, 0.f, 1.f) * 65535.f + 0.5f);
        r[0][i] = r[1][i] = r[2][i] = w;
    }
    HDC dc = GetDC(nullptr);
    BOOL ok = SetDeviceGammaRamp(dc, r);
    ReleaseDC(nullptr, dc);
    return ok != FALSE;
}
static void ApplyScreen(bool reset) {
    ColorP p = reset ? ColorP() : g_col;
    BuildMatrix(p);
    bool okM = MagSet(g_M);
    float gm = reset ? 1.f : g_col.gamma;
    bool okG = SetGammaRamp(gm);
    if (reset) g_colStatus = L"Скинуто до стандартних значень";
    else if (okM && okG) g_colStatus = L"Застосовано до всього екрана";
    else if (okM) g_colStatus = L"Застосовано (гамма недоступна на цьому дисплеї)";
    else if (okG) g_colStatus = L"Застосовано лише гаму (Magnification API недоступний)";
    else g_colStatus = L"Не вдалося застосувати: система відхилила зміни";
    BuildMatrix(g_col);   // повертаємо матрицю прев'ю
    g_sphereDirty = true;
}

// ============================================================================
//  Приціл
// ============================================================================
static float CurAngle() {
    if (g_cross.anim != 1) return 0.f;
    double t = GetTickCount64() / 1000.0;
    return (float)fmod(t * g_cross.speed, 360.0);
}
static float CurPulse() {
    if (g_cross.anim != 2) return 1.f;
    double t = GetTickCount64() / 1000.0;
    return 1.f + 0.18f * (float)sin(t * 2 * PI_F * (0.5 + g_cross.speed / 360.0));
}

static void DrawCrosshair(Graphics& g, float cx, float cy, int type, const Cross& c, float angle, float pulse) {
    GraphicsState st = g.Save();
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.TranslateTransform(cx, cy);
    g.RotateTransform(angle);
    float r = c.size * 0.5f * pulse, th = c.thick, gp = c.gap;
    DWORD base = g_swatch[c.color & 7];
    BYTE a = (BYTE)(255.f * c.opacity / 100.f);
    Color col(a, (BYTE)((base >> 16) & 0xFF), (BYTE)((base >> 8) & 0xFF), (BYTE)(base & 0xFF));
    Pen pen(col, th);
    pen.SetStartCap(LineCapFlat);
    pen.SetEndCap(LineCapFlat);
    SolidBrush br(col);
    float in0 = min(gp, r - 1.f);
    switch (type) {
    case 0: g.FillEllipse(&br, -th * 1.5f, -th * 1.5f, th * 3.f, th * 3.f); break;
    case 1:
        g.DrawLine(&pen, -r, 0.f, r, 0.f);
        g.DrawLine(&pen, 0.f, -r, 0.f, r);
        break;
    case 2:
        g.DrawLine(&pen, -r, 0.f, -in0, 0.f); g.DrawLine(&pen, in0, 0.f, r, 0.f);
        g.DrawLine(&pen, 0.f, -r, 0.f, -in0); g.DrawLine(&pen, 0.f, in0, 0.f, r);
        break;
    case 3: g.DrawEllipse(&pen, -r, -r, r * 2, r * 2); break;
    case 4:
        g.DrawEllipse(&pen, -r, -r, r * 2, r * 2);
        g.FillEllipse(&br, -th * 1.2f, -th * 1.2f, th * 2.4f, th * 2.4f);
        break;
    case 5:
        g.DrawLine(&pen, -r, 0.f, -in0, 0.f); g.DrawLine(&pen, in0, 0.f, r, 0.f);
        g.DrawLine(&pen, 0.f, in0, 0.f, r);
        break;
    case 6:
        g.RotateTransform(45.f);
        g.DrawLine(&pen, -r, 0.f, -in0, 0.f); g.DrawLine(&pen, in0, 0.f, r, 0.f);
        g.DrawLine(&pen, 0.f, -r, 0.f, -in0); g.DrawLine(&pen, 0.f, in0, 0.f, r);
        break;
    case 7: {
        PointF p[4] = { PointF(0, -r), PointF(r, 0), PointF(0, r), PointF(-r, 0) };
        g.DrawPolygon(&pen, p, 4);
        break; }
    default: {
        int ci = type - 8;
        if (ci >= 0 && ci < (int)g_custom.size() && g_custom[ci].bmp) {
            Bitmap* bm = g_custom[ci].bmp;
            float w = (float)bm->GetWidth(), h = (float)bm->GetHeight();
            float s = (c.size * pulse) / max(w, h);
            ImageAttributes ia;
            ColorMatrix cm = { 1, 0, 0, 0, 0,  0, 1, 0, 0, 0,  0, 0, 1, 0, 0,  0, 0, 0, c.opacity / 100.f, 0,  0, 0, 0, 0, 1 };
            ia.SetColorMatrix(&cm, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            g.DrawImage(bm, RectF(-w * s / 2, -h * s / 2, w * s, h * s), 0.f, 0.f, w, h, UnitPixel, &ia);
        }
        break; }
    }
    g.Restore(st);
}

static void LoadCustom() {
    for (auto& c : g_custom) delete c.bmp;
    g_custom.clear();
    std::wstring root = ExeDir() + L"\\Custom";
    std::wstring dir = root + L"\\Crosshair";
    CreateDirectoryW(root.c_str(), nullptr);
    CreateDirectoryW(dir.c_str(), nullptr);
    const wchar_t* exts[] = { L"png", L"jpg", L"jpeg", L"bmp", L"gif" };
    for (auto ext : exts) {
        WIN32_FIND_DATAW fd;
        std::wstring pat = dir + L"\\*." + ext;
        HANDLE h = FindFirstFileW(pat.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring path = dir + L"\\" + fd.cFileName;
            Bitmap src(path.c_str());
            if (src.GetLastStatus() != Ok || src.GetWidth() == 0) continue;
            Bitmap* cl = src.Clone(0, 0, (INT)src.GetWidth(), (INT)src.GetHeight(), PixelFormat32bppPARGB);
            if (cl) { CustomCH c; c.name = fd.cFileName; c.bmp = cl; g_custom.push_back(c); }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (g_cross.type >= 8 + (int)g_custom.size()) g_cross.type = 1;
}

// ---- оверлей (прозоре вікно поверх усього, клікабельність наскрізь) ----
static const int OV_N = 256;
static void RenderOverlay() {
    if (!g_ov) return;
    HDC sdc = GetDC(nullptr);
    HDC mdc = CreateCompatibleDC(sdc);
    BITMAPINFO bi; memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = OV_N; bi.bmiHeader.biHeight = -OV_N;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbm = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (hbm && bits) {
        HGDIOBJ old = SelectObject(mdc, hbm);
        memset(bits, 0, OV_N * OV_N * 4);
        {
            Bitmap bmp(OV_N, OV_N, OV_N * 4, PixelFormat32bppPARGB, (BYTE*)bits);
            Graphics g(&bmp);
            DrawCrosshair(g, OV_N / 2.f, OV_N / 2.f, g_cross.type, g_cross, CurAngle(), CurPulse());
        }
        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
        POINT pt = { (sw - OV_N) / 2, (sh - OV_N) / 2 };
        SIZE sz = { OV_N, OV_N };
        POINT src = { 0, 0 };
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(g_ov, sdc, &pt, &sz, mdc, &src, 0, &bf, ULW_ALPHA);
        SelectObject(mdc, old);
    }
    if (hbm) DeleteObject(hbm);
    DeleteDC(mdc);
    ReleaseDC(nullptr, sdc);
}
static void OverlayRefresh() {
    if (!g_ov) return;
    if (!g_cross.enabled) {
        ShowWindow(g_ov, SW_HIDE);
        KillTimer(g_hwnd, 2);
        return;
    }
    if (!IsWindowVisible(g_ov)) ShowWindow(g_ov, SW_SHOWNOACTIVATE);
    RenderOverlay();
    if (g_cross.anim) SetTimer(g_hwnd, 2, 16, nullptr); else KillTimer(g_hwnd, 2);
}
static LRESULT CALLBACK OverlayProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCHITTEST) return HTTRANSPARENT;
    if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    return DefWindowProcW(h, m, w, l);
}

// ============================================================================
//  Оптимізатор
// ============================================================================
struct Proc { DWORD pid; std::wstring exe; };
static void ListProcs(std::vector<Proc>& out) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
    if (Process32FirstW(s, &pe)) do {
        Proc p; p.pid = pe.th32ProcessID; p.exe = pe.szExeFile;
        out.push_back(p);
    } while (Process32NextW(s, &pe));
    CloseHandle(s);
}
static int MatchGame(const std::wstring& exe) {
    for (int i = 0; i < 6; i++)
        for (auto e : g_games[i].exes)
            if (_wcsicmp(exe.c_str(), e) == 0) return i;
    return -1;
}

static bool SetDword(HKEY root, const wchar_t* sub, const wchar_t* name, DWORD v) {
    HKEY k;
    if (RegCreateKeyExW(root, sub, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    LONG r = RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

static ULONGLONG CleanDir(const std::wstring& dir) {
    ULONGLONG freed = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        std::wstring p = dir + L"\\" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            freed += CleanDir(p);
            RemoveDirectoryW(p.c_str());
        } else {
            ULONGLONG sz = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL);
            if (DeleteFileW(p.c_str())) freed += sz;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return freed;
}

static const GUID kHighPerf = { 0x8c5e7fda, 0xe8bf, 0x4a96, { 0x9a, 0x85, 0xa6, 0xe2, 0x7a, 0x1e, 0x4b, 0x96 } };

static DWORD WINAPI OptThread(LPVOID) {
    bool o[6]; for (int i = 0; i < 6; i++) o[i] = g_opt[i];
    g_progress = 0;
    SetStatus(L"Оптимізація…");
    Log(L"Запуск оптимізації");

    SYSTEM_INFO si; GetSystemInfo(&si);
    std::vector<Proc> procs; ListProcs(procs);
    std::vector<DWORD> gamePids;
    int found = 0;
    g_progress = 10;

    // 1. ігри: пріоритет + ядра
    for (auto& p : procs) {
        int gi = MatchGame(p.exe);
        if (gi < 0) continue;
        HANDLE h = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_INFORMATION, FALSE, p.pid);
        if (!h) { Log(std::wstring(L"Немає доступу до ") + g_games[gi].name); continue; }
        found++;
        gamePids.push_back(p.pid);
        Log(std::wstring(L"Знайдено гру: ") + g_games[gi].name + L" (PID " + std::to_wstring(p.pid) + L")");
        if (o[0]) {
            if (SetPriorityClass(h, HIGH_PRIORITY_CLASS)) Log(L"  Пріоритет → Високий");
            else Log(L"  Не вдалося змінити пріоритет");
        }
        if (o[1]) {
            DWORD_PTR pm = 0, sm = 0;
            if (GetProcessAffinityMask(h, &pm, &sm)) {
                DWORD_PTR nm = sm;
                if (si.dwNumberOfProcessors >= 8) nm &= ~(DWORD_PTR)1;   // ядро 0 лишаємо системі
                if (nm && SetProcessAffinityMask(h, nm)) {
                    wchar_t b[64]; swprintf(b, 64, L"  Ядра CPU: маска 0x%llX", (unsigned long long)nm);
                    Log(b);
                }
            }
        }
        g_gameOpt[gi] = true;
        CloseHandle(h);
    }
    if (!found) Log(L"Підтримуваних ігор не запущено — застосовую системні оптимізації");
    g_progress = 35;

    // 2. RAM
    if (o[2]) {
        MEMORYSTATUSEX ms; ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        ULONGLONG before = ms.ullAvailPhys;
        DWORD self = GetCurrentProcessId();
        for (auto& p : procs) {
            if (p.pid <= 4 || p.pid == self) continue;
            bool isGame = false;
            for (auto gp : gamePids) if (gp == p.pid) isGame = true;
            if (isGame) continue;
            HANDLE h = OpenProcess(PROCESS_SET_QUOTA | PROCESS_QUERY_INFORMATION, FALSE, p.pid);
            if (h) { EmptyWorkingSet(h); CloseHandle(h); }
        }
        Sleep(400);
        GlobalMemoryStatusEx(&ms);
        ULONGLONG freed = ms.ullAvailPhys > before ? ms.ullAvailPhys - before : 0;
        Log(L"Очищено RAM: " + std::to_wstring(freed / (1024 * 1024)) + L" МБ");
    }
    g_progress = 55;

    // 3. живлення та ігровий режим
    if (o[3]) {
        if (!g_havePrev) {
            GUID* pg = nullptr;
            if (PowerGetActiveScheme(nullptr, &pg) == ERROR_SUCCESS && pg) { g_prevScheme = *pg; g_havePrev = true; LocalFree(pg); }
        }
        if (PowerSetActiveScheme(nullptr, &kHighPerf) == ERROR_SUCCESS) Log(L"Схема живлення → Висока продуктивність");
        else Log(L"Схема «Висока продуктивність» недоступна на цьому ПК");
        SetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AutoGameModeEnabled", 1);
        SetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\GameBar", L"AllowAutoGameMode", 1);
        SetDword(HKEY_CURRENT_USER, L"System\\GameConfigStore", L"GameDVR_Enabled", 0);
        SetDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR", L"AppCaptureEnabled", 0);
        Log(L"Ігровий режим увімкнено, Game DVR вимкнено");
    }
    g_progress = 70;

    // 4. мережа
    if (o[4]) {
        HKEY k;
        int n = 0;
        const wchar_t* base = L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces";
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, base, 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) {
            wchar_t name[256]; DWORD idx = 0, len = 256;
            std::vector<std::wstring> subs;
            while (RegEnumKeyExW(k, idx++, name, &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) { subs.push_back(name); len = 256; }
            RegCloseKey(k);
            for (auto& s : subs) {
                HKEY ik;
                std::wstring path = std::wstring(base) + L"\\" + s;
                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &ik) == ERROR_SUCCESS) {
                    DWORD one = 1, zero = 0;
                    RegSetValueExW(ik, L"TcpAckFrequency", 0, REG_DWORD, (const BYTE*)&one, 4);
                    RegSetValueExW(ik, L"TCPNoDelay", 0, REG_DWORD, (const BYTE*)&one, 4);
                    RegSetValueExW(ik, L"TcpDelAckTicks", 0, REG_DWORD, (const BYTE*)&zero, 4);
                    RegCloseKey(ik);
                    n++;
                }
            }
        }
        Log(L"Мережа: оновлено інтерфейсів " + std::to_wstring(n) + L" (діє після перепідключення)");
    }
    g_progress = 85;

    // 5. тимчасові файли
    if (o[5]) {
        ULONGLONG freed = 0;
        wchar_t t[MAX_PATH];
        DWORD n = GetTempPathW(MAX_PATH, t);
        if (n > 0 && n < MAX_PATH) {
            std::wstring tp = t;
            if (!tp.empty() && tp.back() == L'\\') tp.pop_back();
            freed += CleanDir(tp);
        }
        wchar_t w[MAX_PATH];
        if (GetWindowsDirectoryW(w, MAX_PATH)) freed += CleanDir(std::wstring(w) + L"\\Temp");
        Log(L"Тимчасові файли: видалено " + std::to_wstring(freed / (1024 * 1024)) + L" МБ");
    }
    g_progress = 100;
    SetStatus(found ? L"Готово: ігри оптимізовано" : L"Готово: системні оптимізації застосовано");
    Log(L"Оптимізацію завершено");
    InterlockedExchange(&g_running, 0);
    return 0;
}
static void StartOpt() {
    if (InterlockedCompareExchange(&g_running, 1, 0) != 0) return;
    HANDLE t = CreateThread(nullptr, 0, OptThread, nullptr, 0, nullptr);
    if (t) CloseHandle(t); else InterlockedExchange(&g_running, 0);
}
static void DoRestore() {
    std::vector<Proc> pr; ListProcs(pr);
    int n = 0;
    for (auto& p : pr) {
        int gi = MatchGame(p.exe);
        if (gi < 0) continue;
        HANDLE h = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_INFORMATION, FALSE, p.pid);
        if (!h) continue;
        SetPriorityClass(h, NORMAL_PRIORITY_CLASS);
        DWORD_PTR pm = 0, sm = 0;
        if (GetProcessAffinityMask(h, &pm, &sm)) SetProcessAffinityMask(h, sm);
        CloseHandle(h);
        g_gameOpt[gi] = false;
        n++;
    }
    if (g_havePrev) { PowerSetActiveScheme(nullptr, &g_prevScheme); g_havePrev = false; }
    Log(L"Відновлено стандартні налаштування (ігор: " + std::to_wstring(n) + L")");
    SetStatus(L"Стандартні налаштування відновлено");
}
static void ScanGames() {
    std::vector<Proc> pr; ListProcs(pr);
    bool now[6] = {};
    for (auto& p : pr) { int gi = MatchGame(p.exe); if (gi >= 0) now[gi] = true; }
    bool startAuto = false;
    for (int i = 0; i < 6; i++) {
        if (now[i] != g_gameRun[i]) InterlockedExchange(&g_dirty, 1);
        g_gameRun[i] = now[i];
        if (!now[i]) g_gameOpt[i] = false;
        else if (g_auto && !g_gameOpt[i]) startAuto = true;
    }
    if (startAuto && !g_running) { Log(L"Автооптимізація: виявлено гру"); StartOpt(); }
}

// ============================================================================
//  Малювання сторінок
// ============================================================================
static const RectF R_MENUBTN(20, 12, 244, 48);
static const RectF R_MIN(WIN_W - 92, 19, 34, 34);
static const RectF R_CLOSE(WIN_W - 50, 19, 34, 34);

static const wchar_t* PageName() {
    switch (g_page) { case PG_VIS: return L"Візуалізація"; case PG_COL: return L"Кольорокоректор"; default: return L"Оптимізатор"; }
}

static void DrawMenuIcon(Graphics& g, int i, RectF r) {
    float cx = r.X + r.Width / 2, cy = r.Y + r.Height / 2;
    if (i == 0) {
        Pen p(Color(COL_TEXT), 1.6f);
        g.DrawEllipse(&p, cx - 7, cy - 7, 14.f, 14.f);
        g.DrawLine(&p, cx - 11, cy, cx - 3, cy); g.DrawLine(&p, cx + 3, cy, cx + 11, cy);
        g.DrawLine(&p, cx, cy - 11, cx, cy - 3); g.DrawLine(&p, cx, cy + 3, cx, cy + 11);
    } else if (i == 1) {
        DrawBolt(g, cx, cy, 10, COL_TEXT);
    } else {
        SolidBrush a(Color(COL_ACC)), b(Color(COL_ACC2));
        g.FillPie(&a, cx - 9, cy - 9, 18.f, 18.f, 90.f, 180.f);
        g.FillPie(&b, cx - 9, cy - 9, 18.f, 18.f, 270.f, 180.f);
    }
}

static void PaintHeader(Graphics& g) {
    // кнопка-меню "boosterik 1.12"
    RectF mb = R_MENUBTN;
    bool hot = Hot(ID_MENU_BTN) || g_menuOpen;
    FillRR(g, mb, 24, 0xFF000000);
    StrokeRR(g, mb, 24, hot ? 0xFF6A7090 : 0xFF2C2C3C, 1.3f);
    if (g_logo) {
        GraphicsPath cp; cp.AddEllipse(RectF(mb.X + 5, mb.Y + 4, 40, 40));
        GraphicsState st = g.Save();
        g.SetClip(&cp, CombineModeReplace);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.DrawImage(g_logo, RectF(mb.X + 5, mb.Y + 4, 40, 40));
        g.Restore(st);
    } else {
        DrawBolt(g, mb.X + 25, mb.Y + 24, 14, COL_TEXT);
    }
    RectF tr(mb.X + 54, mb.Y, 150, mb.Height);
    const float off[4][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1} };
    for (auto& o : off) TxtR(g, APP_TITLE, RectF(tr.X + o[0], tr.Y + o[1], tr.Width, tr.Height), 19, 0x30FFFFFF);
    TxtR(g, APP_TITLE, tr, 19, 0xFFFFFFFF);
    // шеврон
    {
        Pen p(Color(0xFFB8BCD0), 2.f);
        float cx = mb.GetRight() - 24, cy = mb.Y + 24;
        float dy = g_menuOpen ? -3.f : 3.f;
        g.DrawLine(&p, cx - 5, cy - dy * 0.0f - dy, cx, cy + dy);
        g.DrawLine(&p, cx, cy + dy, cx + 5, cy - dy);
    }
    AddHit(ID_MENU_BTN, mb);

    TxtR(g, std::wstring(L"›  ") + PageName(), RectF(284, 12, 300, 48), 15, COL_MUTED);

    // згорнути / закрити
    if (Hot(ID_MIN)) FillRR(g, R_MIN, 17, COL_PANEL2);
    Pen p(Color(COL_TEXT), 1.6f);
    g.DrawLine(&p, R_MIN.X + 11, R_MIN.Y + 17, R_MIN.X + 23, R_MIN.Y + 17);
    AddHit(ID_MIN, R_MIN);
    if (Hot(ID_CLOSE)) FillRR(g, R_CLOSE, 17, COL_RED);
    g.DrawLine(&p, R_CLOSE.X + 11, R_CLOSE.Y + 11, R_CLOSE.X + 23, R_CLOSE.Y + 23);
    g.DrawLine(&p, R_CLOSE.X + 23, R_CLOSE.Y + 11, R_CLOSE.X + 11, R_CLOSE.Y + 23);
    AddHit(ID_CLOSE, R_CLOSE);

    Pen sep(Color(0xFF1C1C28), 1.f);
    g.DrawLine(&sep, 0.f, HDR_H, WIN_W, HDR_H);
}

static void PaintMenu(Graphics& g) {
    if (!g_menuOpen) return;
    AddHit(ID_MENU_AWAY, RectF(0, HDR_H, WIN_W, WIN_H - HDR_H));
    RectF panel(20, 66, 280, 3 * 54 + 18);
    FillRR(g, panel, 14, 0xFF0F0F17);
    StrokeRR(g, panel, 14, 0xFF34344A, 1.2f);
    struct It { int id; const wchar_t* t; const wchar_t* d; Page pg; } items[3] = {
        { ID_MENU_VIS, L"Візуалізація", L"Приціл-оверлей на екрані", PG_VIS },
        { ID_MENU_OPT, L"Оптимізатор", L"Бустер FPS для ігор", PG_OPT },
        { ID_MENU_COL, L"Кольорокоректор", L"Гама, насиченість, температура", PG_COL } };
    for (int i = 0; i < 3; i++) {
        RectF r(28.f, 75.f + i * 54.f, 264.f, 50.f);
        bool hot = Hot(items[i].id), cur = (g_page == items[i].pg);
        if (hot) FillRR(g, r, 10, 0xFF1D1D2B);
        else if (cur) FillRR(g, r, 10, 0xFF171722);
        RectF ic(r.X + 8, r.Y + 9, 32, 32);
        FillRR(g, ic, 9, 0xFF22222F);
        DrawMenuIcon(g, i == 0 ? 0 : (i == 1 ? 1 : 2), ic);
        TxtR(g, items[i].t, RectF(r.X + 50, r.Y + 6, r.Width - 70, 22), 15, COL_TEXT, true);
        TxtR(g, items[i].d, RectF(r.X + 50, r.Y + 26, r.Width - 70, 18), 11.5f, COL_MUTED);
        if (cur) { SolidBrush b(Color(COL_ACC)); g.FillEllipse(&b, r.GetRight() - 18, r.Y + 21, 8.f, 8.f); }
        AddHit(items[i].id, r);
    }
}

static void PaintOpt(Graphics& g) {
    // --- Центр керування ---
    RectF c1(20, 88, 520, 310);
    Card(g, c1, L"Центр керування", L"Оптимізація ігор в один клік");
    RectF go(44, 156, 472, 60);
    bool running = g_running != 0;
    Button(g, ID_OPT_GO, go, running ? L"" : L"Оптимізація в один клік", true, 16);
    if (running) {
        RectF pr(go.X + 10, go.Y + go.Height - 12, go.Width - 20, 4);
        FillRR(g, pr, 2, 0x55000000);
        float w = pr.Width * (g_progress / 100.f);
        if (w > 2) FillRR(g, RectF(pr.X, pr.Y, w, pr.Height), 2, 0xFFFFFFFF);
        TxtR(g, std::wstring(L"Оптимізація…  ") + std::to_wstring((int)g_progress) + L"%", RectF(go.X, go.Y, go.Width, go.Height - 8), 16, COL_TEXT, true, StringAlignmentCenter);
    } else {
        DrawBolt(g, go.X + 118, go.Y + 30, 11, COL_TEXT);
    }
    std::wstring st;
    EnterCriticalSection(&g_cs); st = g_status; LeaveCriticalSection(&g_cs);
    TxtR(g, st, RectF(44, 222, 340, 28), 13, COL_MUTED);
    Button(g, ID_OPT_RESTORE, RectF(406, 222, 110, 28), L"Відновити", false, 12);
    for (int i = 0; i < 6; i++) {
        RectF r(44.f + (i % 2) * 244.f, 266.f + (i / 2) * 42.f, 236.f, 34.f);
        Toggle(g, ID_T0 + i, r, g_optNames[i], g_opt[i]);
    }

    // --- Ігри ---
    RectF c2(560, 88, 320, 310);
    Card(g, c2, L"Ігри", L"Стан виявлення процесів");
    for (int i = 0; i < 6; i++) {
        RectF r(584.f, 156.f + i * 32.f, 272.f, 30.f);
        bool run = g_gameRun[i], opt = g_gameOpt[i];
        SolidBrush dot(Color(run ? (opt ? COL_ACC : COL_GREEN) : 0xFF3A3A4A));
        g.FillEllipse(&dot, r.X, r.Y + 11, 8.f, 8.f);
        TxtR(g, g_games[i].name, RectF(r.X + 18, r.Y, 150, r.Height), 14, run ? COL_TEXT : 0xFFB0B4C6, run);
        TxtR(g, run ? (opt ? L"Оптимізовано" : L"Запущено") : L"Очікує", RectF(r.X + 150, r.Y, r.Width - 150, r.Height), 12, run ? (opt ? COL_ACC : COL_GREEN) : COL_MUTED, false, StringAlignmentFar);
    }
    Toggle(g, ID_T_AUTO, RectF(584, 354, 272, 30), L"Автооптимізація при запуску гри", g_auto);

    // --- Журнал ---
    RectF c3(20, 414, 860, 186);
    Card(g, c3, L"Журнал");
    std::vector<std::wstring> lines;
    EnterCriticalSection(&g_cs);
    size_t n = g_log.size();
    for (size_t i = (n > 7 ? n - 7 : 0); i < n; i++) lines.push_back(g_log[i]);
    LeaveCriticalSection(&g_cs);
    if (lines.empty()) TxtR(g, L"Подій ще немає. Натисни «Оптимізація в один клік».", RectF(44, 460, 800, 20), 12, COL_MUTED);
    for (size_t i = 0; i < lines.size(); i++)
        TxtR(g, lines[i], RectF(44, 458 + i * 18.f, 812, 18), 12, 0xFFC4C8DA, false, StringAlignmentNear, StringAlignmentCenter, GetMono(12));
}

static void PaintVis(Graphics& g) {
    // --- ліва картка: вибір прицілу ---
    RectF c1(20, 88, 420, 512);
    Card(g, c1, L"Приціл", L"Вбудовані та власні приціли");
    Toggle(g, ID_VIS_ENABLE, RectF(44, 156, 372, 30), L"Показувати приціл на екрані", g_cross.enabled);

    int total = 8 + (int)g_custom.size();
    int rows = (total + 3) / 4;
    int maxScroll = max(0, rows - 4);
    g_tileScroll = max(0, min(g_tileScroll, maxScroll));
    RectF area(44, 196, 372, 348);
    GraphicsState st = g.Save();
    g.SetClip(area, CombineModeReplace);
    for (int i = 0; i < total; i++) {
        int row = i / 4 - g_tileScroll, col = i % 4;
        if (row < 0 || row >= 4) continue;
        RectF t(area.X + col * 98.f, area.Y + row * 90.f, 78, 78);
        bool sel = (g_cross.type == i), hot = Hot(ID_TILE0 + i);
        FillRR(g, t, 12, hot ? 0xFF222232 : COL_PANEL2);
        StrokeRR(g, t, 12, sel ? COL_ACC : (hot ? 0xFF3A3A50 : COL_BORDER), sel ? 2.f : 1.f);
        Cross tmp = g_cross;
        tmp.size = 40; tmp.thick = 2.5f; tmp.gap = 5; tmp.opacity = 100;
        if (i >= 8) tmp.size = 54;
        DrawCrosshair(g, t.X + 39, t.Y + 39, i, tmp, 0.f, 1.f);
        AddHit(ID_TILE0 + i, t);
    }
    g.Restore(st);
    if (rows > 4) {
        float th = area.Height * 4.f / rows;
        float ty = area.Y + (area.Height - th) * (maxScroll ? (float)g_tileScroll / maxScroll : 0.f);
        FillRR(g, RectF(area.GetRight() + 2, ty, 3, th), 1.5f, 0xFF3A3A50);
    }
    Button(g, ID_VIS_FOLDER, RectF(44, 556, 180, 34), L"Відкрити папку Custom", false, 12);
    Button(g, ID_VIS_REFRESH, RectF(234, 556, 182, 34), L"Оновити список", false, 12);

    // --- права картка: налаштування ---
    RectF c2(460, 88, 420, 512);
    Card(g, c2, L"Налаштування");
    RectF pv(484, 138, 372, 120);
    FillRR(g, pv, 12, 0xFF0B0B11);
    StrokeRR(g, pv, 12, COL_BORDER, 1);
    {
        GraphicsState s2 = g.Save();
        g.SetClip(pv, CombineModeReplace);
        Pen gp(Color(0x14FFFFFF), 1.f);
        for (float x = pv.X; x < pv.GetRight(); x += 24) g.DrawLine(&gp, x, pv.Y, x, pv.GetBottom());
        for (float y = pv.Y; y < pv.GetBottom(); y += 24) g.DrawLine(&gp, pv.X, y, pv.GetRight(), y);
        DrawCrosshair(g, pv.X + pv.Width / 2, pv.Y + pv.Height / 2, g_cross.type, g_cross, CurAngle(), CurPulse());
        g.Restore(s2);
    }
    TxtR(g, L"Колір", RectF(484, 266, 200, 20), 13, 0xFFD6D9E6);
    for (int i = 0; i < 8; i++) {
        float x = 484 + i * 36.f;
        RectF r(x, 290, 28, 28);
        SolidBrush b(Color((ARGB)g_swatch[i]));
        g.FillEllipse(&b, r);
        if (g_cross.color == i) { Pen p(Color(0xFFFFFFFF), 2.f); g.DrawEllipse(&p, x - 3, 287.f, 34.f, 34.f); }
        else if (Hot(ID_SW0 + i)) { Pen p(Color(0x88FFFFFF), 1.5f); g.DrawEllipse(&p, x - 3, 287.f, 34.f, 34.f); }
        AddHit(ID_SW0 + i, RectF(x - 3, 287, 34, 34));
    }
    const wchar_t* an[3] = { L"Без анімації", L"Обертання", L"Пульсація" };
    for (int i = 0; i < 3; i++) {
        RectF r(484 + i * 126.f, 330, 120, 32);
        bool sel = g_cross.anim == i;
        if (sel) FillRRGrad(g, r, 10, COL_ACC, COL_ACC2);
        else { FillRR(g, r, 10, Hot(ID_ANIM0 + i) ? 0xFF242433 : COL_PANEL2); StrokeRR(g, r, 10, COL_BORDER, 1); }
        TxtR(g, an[i], r, 12.5f, COL_TEXT, true, StringAlignmentCenter);
        AddHit(ID_ANIM0 + i, r);
    }
    const wchar_t* sl[5] = { L"Розмір", L"Товщина", L"Проміжок", L"Непрозорість", L"Швидкість анімації" };
    for (int i = 0; i < 5; i++) Slider(g, ID_SL_SIZE + i, RectF(484, 376 + i * 44.f, 372, 40), sl[i]);
}

static void PaintCol(Graphics& g) {
    if (g_sphereDirty) { RenderSphere(); g_sphereDirty = false; }
    RectF c1(20, 88, 520, 512);
    Card(g, c1, L"Кольорокоректор PRO", L"Налаштуй картинку під себе");
    const wchar_t* sl[6] = { L"Гама", L"Насиченість", L"Експозиція", L"Контраст", L"Температура (холодна ↔ тепла)", L"Тон" };
    for (int i = 0; i < 6; i++) Slider(g, ID_SL_GAMMA + i, RectF(44, 158 + i * 58.f, 472, 44), sl[i]);
    Button(g, ID_COL_RESET, RectF(44, 508, 140, 40), L"Скинути", false, 14);
    Button(g, ID_COL_APPLY, RectF(196, 508, 320, 40), L"Застосувати до екрана", true, 14);
    TxtR(g, g_colStatus, RectF(44, 556, 472, 24), 12.5f, COL_MUTED);

    RectF c2(560, 88, 320, 512);
    Card(g, c2, L"Колірна сфера", L"Попередній перегляд");
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    if (g_sphere) g.DrawImage(g_sphere, RectF(600, 150, (float)SPH, (float)SPH));
    TxtR(g, L"Градієнт сірого", RectF(584, 396, 272, 20), 12.5f, 0xFFD6D9E6);
    for (int i = 0; i < 64; i++) {
        float v = i / 63.f, r = v, gg = v, b = v;
        ApplyCol(r, gg, b);
        SolidBrush br(Color(0xFF, (BYTE)(r * 255 + .5f), (BYTE)(gg * 255 + .5f), (BYTE)(b * 255 + .5f)));
        g.FillRectangle(&br, 584.f + i * 272.f / 64.f, 420.f, 272.f / 64.f + 0.6f, 20.f);
    }
    TxtR(g, L"Палітра", RectF(584, 450, 272, 20), 12.5f, 0xFFD6D9E6);
    const float pal[8][3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 0}, {0.87f, 0.68f, 0.56f}, {0.45f, 0.65f, 0.90f} };
    for (int i = 0; i < 8; i++) {
        float r = pal[i][0], gg = pal[i][1], b = pal[i][2];
        ApplyCol(r, gg, b);
        SolidBrush br(Color(0xFF, (BYTE)(r * 255 + .5f), (BYTE)(gg * 255 + .5f), (BYTE)(b * 255 + .5f)));
        g.FillRectangle(&br, 584.f + i * 34.f, 474.f, 34.f, 30.f);
    }
    TxtW(g, L"Перегляд оновлюється одразу. «Застосувати» змінює колір усього екрана; ігри в ексклюзивному повноекранному режимі можуть це ігнорувати — використовуй віконний режим без рамки.",
         RectF(584, 516, 272, 70), 11.5f, COL_MUTED);
}

// ============================================================================
//  Back buffer + головний цикл малювання
// ============================================================================
static HDC g_bbDC = nullptr;
static HBITMAP g_bbBmp = nullptr, g_bbOld = nullptr;
static int g_bbW = 0, g_bbH = 0;

static void PaintAll(Graphics& g) {
    g.Clear(Color(COL_BG));
    StrokeRR(g, RectF(0.5f, 0.5f, WIN_W - 1, WIN_H - 1), 14, 0xFF22222E, 1.f);
    switch (g_page) {
    case PG_OPT: PaintOpt(g); break;
    case PG_VIS: PaintVis(g); break;
    case PG_COL: PaintCol(g); break;
    }
    PaintHeader(g);
    PaintMenu(g);
}

static void RenderFrame() {
    if (!g_bbDC) return;
    Graphics g(g_bbDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.ScaleTransform(g_S, g_S);
    g_hits.clear();
    PaintAll(g);
}

// ============================================================================
//  Події
// ============================================================================
static void OnSliderChanged(int id) {
    if (id >= ID_SL_GAMMA && id <= ID_SL_HUE) g_sphereDirty = true;
    else OverlayRefresh();
}
static void SliderSet(int id, float mx) {
    SliderDef* d = FindSlider(id);
    if (!d) return;
    float tx = 0, tw = 1;
    for (auto& h : g_hits) if (h.id == id) { tx = h.r.X + 8; tw = h.r.Width - 16; }
    float t = clampf((mx - tx) / tw, 0.f, 1.f);
    float v = d->mn + t * (d->mx - d->mn);
    v = roundf(v / d->step) * d->step;
    v = clampf(v, d->mn, d->mx);
    *d->v = v;
    OnSliderChanged(id);
}
static bool IsSliderId(int id) { return FindSlider(id) != nullptr; }

static void OnClick(int id) {
    if (id >= ID_TILE0) { g_cross.type = id - ID_TILE0; OverlayRefresh(); return; }
    if (id >= ID_T0 && id < ID_T0 + 6) { g_opt[id - ID_T0] = !g_opt[id - ID_T0]; return; }
    if (id >= ID_SW0 && id < ID_SW0 + 8) { g_cross.color = id - ID_SW0; OverlayRefresh(); return; }
    if (id >= ID_ANIM0 && id < ID_ANIM0 + 3) { g_cross.anim = id - ID_ANIM0; OverlayRefresh(); return; }
    switch (id) {
    case ID_MENU_BTN: g_menuOpen = !g_menuOpen; break;
    case ID_MENU_AWAY: g_menuOpen = false; break;
    case ID_MENU_VIS: g_page = PG_VIS; g_menuOpen = false; break;
    case ID_MENU_OPT: g_page = PG_OPT; g_menuOpen = false; break;
    case ID_MENU_COL: g_page = PG_COL; g_menuOpen = false; g_sphereDirty = true; break;
    case ID_MIN: ShowWindow(g_hwnd, SW_MINIMIZE); break;
    case ID_CLOSE: DestroyWindow(g_hwnd); break;
    case ID_OPT_GO: StartOpt(); break;
    case ID_OPT_RESTORE: DoRestore(); break;
    case ID_T_AUTO: g_auto = !g_auto; break;
    case ID_VIS_ENABLE: g_cross.enabled = !g_cross.enabled; OverlayRefresh(); break;
    case ID_VIS_FOLDER: {
        LoadCustom();
        std::wstring d = ExeDir() + L"\\Custom\\Crosshair";
        ShellExecuteW(nullptr, L"open", d.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break; }
    case ID_VIS_REFRESH: LoadCustom(); OverlayRefresh(); break;
    case ID_COL_RESET:
        g_col = ColorP();
        ApplyScreen(true);
        break;
    case ID_COL_APPLY: ApplyScreen(false); break;
    }
}

static bool InHeaderControl(float x, float y) {
    return R_MENUBTN.Contains(x, y) || R_MIN.Contains(x, y) || R_CLOSE.Contains(x, y);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (!IsIconic(h)) {
            RenderFrame();
            BitBlt(dc, 0, 0, g_bbW, g_bbH, g_bbDC, 0, 0, SRCCOPY);
            int nh = HitAt(g_mx, g_my);
            if (nh != g_hot) { g_hot = nh; InvalidateRect(h, nullptr, FALSE); }
        }
        EndPaint(h, &ps);
        return 0; }
    case WM_NCHITTEST: {
        POINT p = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        ScreenToClient(h, &p);
        float x = p.x / g_S, y = p.y / g_S;
        if (y >= 0 && y < HDR_H && x >= 0 && x < WIN_W && !InHeaderControl(x, y)) return HTCAPTION;
        return HTCLIENT; }
    case WM_NCRBUTTONUP: return 0;
    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT) {
            SetCursor(LoadCursor(nullptr, (g_hot != ID_NONE && g_hot != ID_MENU_AWAY) ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE: {
        g_mx = GET_X_LPARAM(l) / g_S; g_my = GET_Y_LPARAM(l) / g_S;
        if (!g_tracking) {
            TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, h, 0 };
            TrackMouseEvent(&t); g_tracking = true;
        }
        if (g_drag != ID_NONE) SliderSet(g_drag, g_mx);
        else g_hot = HitAt(g_mx, g_my);
        InvalidateRect(h, nullptr, FALSE);
        return 0; }
    case WM_MOUSELEAVE:
        g_tracking = false; g_mx = g_my = -1; g_hot = ID_NONE;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        g_mx = GET_X_LPARAM(l) / g_S; g_my = GET_Y_LPARAM(l) / g_S;
        int id = HitAt(g_mx, g_my);
        if (IsSliderId(id)) { g_drag = id; SetCapture(h); SliderSet(id, g_mx); }
        else g_press = id;
        InvalidateRect(h, nullptr, FALSE);
        return 0; }
    case WM_LBUTTONUP: {
        g_mx = GET_X_LPARAM(l) / g_S; g_my = GET_Y_LPARAM(l) / g_S;
        if (g_drag != ID_NONE) { g_drag = ID_NONE; ReleaseCapture(); }
        else {
            int id = HitAt(g_mx, g_my);
            int pr = g_press;
            g_press = ID_NONE;
            if (id != ID_NONE && id == pr) OnClick(id);
        }
        g_press = ID_NONE;
        InvalidateRect(h, nullptr, FALSE);
        return 0; }
    case WM_MOUSEWHEEL: {
        POINT p = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        ScreenToClient(h, &p);
        float x = p.x / g_S, y = p.y / g_S;
        if (g_page == PG_VIS && x >= 44 && x <= 420 && y >= 196 && y <= 544) {
            int d = GET_WHEEL_DELTA_WPARAM(w);
            g_tileScroll += d > 0 ? -1 : 1;
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0; }
    case WM_TIMER:
        if (w == 1) {
            static int tick = 0;
            tick++;
            if (tick % 30 == 0) {
                ScanGames();
                if (g_cross.enabled && g_ov) SetWindowPos(g_ov, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            if (InterlockedExchange(&g_dirty, 0) || g_running) InvalidateRect(h, nullptr, FALSE);
        } else if (w == 2) {
            RenderOverlay();
            if (g_page == PG_VIS && !IsIconic(h)) InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(h, 1); KillTimer(h, 2);
        SaveSettings();
        ApplyScreen(true);
        if (g_magInited && pMagUninit) pMagUninit();
        if (g_ov) DestroyWindow(g_ov);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ============================================================================
//  Точка входу
// ============================================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    g_inst = hInst;
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"Boosterik.SingleInstance.1.12");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND ex = FindWindowW(L"BoosterikWnd", nullptr);
        if (ex) { ShowWindow(ex, SW_RESTORE); SetForegroundWindow(ex); }
        return 0;
    }
    InitializeCriticalSection(&g_cs);

    GdiplusStartupInput gsi;
    GdiplusStartup(&g_gdiplusToken, &gsi, nullptr);
    g_ff = new FontFamily(L"Segoe UI");
    if (!g_ff->IsAvailable()) { delete g_ff; g_ff = FontFamily::GenericSansSerif()->Clone(); }
    g_mono = new FontFamily(L"Consolas");
    if (!g_mono->IsAvailable()) { delete g_mono; g_mono = FontFamily::GenericMonospace()->Clone(); }

    LoadSettings();
    BuildMatrix(g_col);

    // оригінальна гамма-рампа (щоб вміти відновити)
    {
        HDC dc = GetDC(nullptr);
        g_haveOrig = GetDeviceGammaRamp(dc, g_origRamp) != FALSE;
        ReleaseDC(nullptr, dc);
    }
    g_sphere = new Bitmap(SPH, SPH, SPH * 4, PixelFormat32bppPARGB, (BYTE*)g_sphereBuf);

    // логотип з вшитої іконки
    {
        HICON ic = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR);
        if (ic) { g_logo = Bitmap::FromHICON(ic); DestroyIcon(ic); if (g_logo && g_logo->GetLastStatus() != Ok) { delete g_logo; g_logo = nullptr; } }
    }
    LoadCustom();
    ScanGames();

    // масштаб під DPI
    HDC sdc = GetDC(nullptr);
    g_S = GetDeviceCaps(sdc, LOGPIXELSX) / 96.f;
    ReleaseDC(nullptr, sdc);
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    float maxS = min((wa.right - wa.left) / WIN_W, (wa.bottom - wa.top) / WIN_H);
    if (g_S > maxS) g_S = maxS * 0.98f;
    int pw = (int)(WIN_W * g_S), ph = (int)(WIN_H * g_S);

    WNDCLASSEXW wc; memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wc.lpszClassName = L"BoosterikWnd";
    RegisterClassExW(&wc);

    WNDCLASSEXW oc; memset(&oc, 0, sizeof(oc));
    oc.cbSize = sizeof(oc);
    oc.lpfnWndProc = OverlayProc;
    oc.hInstance = hInst;
    oc.lpszClassName = L"BoosterikCrosshair";
    RegisterClassExW(&oc);

    int x = wa.left + ((wa.right - wa.left) - pw) / 2;
    int y = wa.top + ((wa.bottom - wa.top) - ph) / 2;
    g_hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"BoosterikWnd", APP_TITLE,
                             WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU, x, y, pw, ph, nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) return 1;
    SetWindowRgn(g_hwnd, CreateRoundRectRgn(0, 0, pw + 1, ph + 1, (int)(28 * g_S), (int)(28 * g_S)), TRUE);

    // back buffer
    {
        HDC wdc = GetDC(g_hwnd);
        g_bbDC = CreateCompatibleDC(wdc);
        BITMAPINFO bi; memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = pw; bi.bmiHeader.biHeight = -ph;
        bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        g_bbBmp = CreateDIBSection(wdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        g_bbOld = (HBITMAP)SelectObject(g_bbDC, g_bbBmp);
        g_bbW = pw; g_bbH = ph;
        ReleaseDC(g_hwnd, wdc);
    }

    g_ov = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                           L"BoosterikCrosshair", L"", WS_POPUP, 0, 0, OV_N, OV_N, nullptr, nullptr, hInst, nullptr);

    SetTimer(g_hwnd, 1, 50, nullptr);
    ShowWindow(g_hwnd, nShow);
    UpdateWindow(g_hwnd);
    OverlayRefresh();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    for (auto& c : g_custom) delete c.bmp;
    delete g_sphere;
    delete g_logo;
    GdiplusShutdown(g_gdiplusToken);
    if (mtx) CloseHandle(mtx);
    return (int)msg.wParam;
}
