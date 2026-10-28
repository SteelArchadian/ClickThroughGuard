// ClickThroughGuard - Rainmeter plugin
//
// While loaded, makes Rainmeter skin windows truly click-through
// (WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE) whenever a
// fullscreen / borderless window is in front, or always (Mode=Always).
//
// Runs inside Rainmeter.exe. Each skin window is subclassed so that if
// Rainmeter itself tries to clear those bits (skin refresh, settings
// change) the change is rewritten on the spot - no gap where a click
// can land on a skin.
//
// Measure options:
//   Mode=Fullscreen | Always      (default Fullscreen)
//   Exclude=Config1|Config2\Sub   skins to leave alone (config names)
//
// Measure value: 1 = forcing click-through, 0 = idle.

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// ---------------------------------------------------------------
// Rainmeter API (resolved from Rainmeter.dll at runtime, no SDK lib)
// ---------------------------------------------------------------

typedef LPCWSTR(__stdcall* RmReadStringFn)(void*, LPCWSTR, LPCWSTR, BOOL);
typedef LPCWSTR(__stdcall* RmReplaceVariablesFn)(void*, LPCWSTR);
typedef void(__stdcall* RmLogFn)(void*, int, LPCWSTR);

static RmReadStringFn RmReadString = nullptr;
static RmReplaceVariablesFn RmReplaceVariables = nullptr;
static RmLogFn RmLog = nullptr;

static bool LoadRmApi()
{
    if (RmReadString) return true;
    HMODULE rm = GetModuleHandleW(L"Rainmeter.dll");
    if (!rm) return false;
    RmReadString = (RmReadStringFn)GetProcAddress(rm, "RmReadString");
    RmReplaceVariables = (RmReplaceVariablesFn)GetProcAddress(rm, "RmReplaceVariables");
    RmLog = (RmLogFn)GetProcAddress(rm, "RmLog");
    return RmReadString && RmReplaceVariables;
}

// ---------------------------------------------------------------
// Shared state (one guard for the whole Rainmeter process)
// ---------------------------------------------------------------

static const DWORD FORCE_BITS = WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE;
static const UINT_PTR SUBCLASS_ID = 0xC71C7;

struct Measure
{
    bool always = false;
    std::vector<std::wstring> exclude;   // lower-case config names
    std::wstring skinsPath;              // lower-case, trailing backslash
};

static std::vector<Measure*> g_measures;
static bool g_forcing = false;
static std::map<HWND, DWORD> g_added;    // bits this plugin added per window

static std::wstring Lower(std::wstring s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

static std::wstring Trim(const std::wstring& s)
{
    size_t a = s.find_first_not_of(L" \t\"");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\"");
    return s.substr(a, b - a + 1);
}

// Skin window title is the full .ini path; config = folder relative to Skins.
static std::wstring ConfigOf(HWND hwnd, const std::wstring& skinsPath)
{
    wchar_t title[MAX_PATH * 2] = {};
    GetWindowTextW(hwnd, title, _countof(title));
    std::wstring t = Lower(title);
    if (!skinsPath.empty() && t.compare(0, skinsPath.size(), skinsPath) == 0)
        t = t.substr(skinsPath.size());
    size_t slash = t.find_last_of(L'\\');
    return slash == std::wstring::npos ? t : t.substr(0, slash);
}

static bool IsExcluded(HWND hwnd)
{
    for (Measure* m : g_measures)
    {
        if (m->exclude.empty()) continue;
        std::wstring cfg = ConfigOf(hwnd, m->skinsPath);
        for (auto& e : m->exclude)
            if (cfg == e) return true;
    }
    return false;
}

static bool IsSkinWindow(HWND hwnd)
{
    wchar_t cls[64] = {};
    GetClassNameW(hwnd, cls, _countof(cls));
    if (wcscmp(cls, L"RainmeterMeterWindow") != 0) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid == GetCurrentProcessId();
}

// ---------------------------------------------------------------
// Subclass: keep forced bits set even if Rainmeter rewrites the style
// ---------------------------------------------------------------

static LRESULT CALLBACK GuardProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                  UINT_PTR id, DWORD_PTR)
{
    switch (msg)
    {
    case WM_STYLECHANGING:
        if (g_forcing && wp == (WPARAM)GWL_EXSTYLE)
        {
            auto it = g_added.find(hwnd);
            if (it != g_added.end())
                ((STYLESTRUCT*)lp)->styleNew |= it->second;
        }
        break;

    case WM_MOUSEACTIVATE:
        if (g_forcing && g_added.count(hwnd))
            return MA_NOACTIVATE;
        break;

    case WM_NCDESTROY:
        g_added.erase(hwnd);
        RemoveWindowSubclass(hwnd, GuardProc, id);
        break;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void Release(HWND hwnd, DWORD bits)
{
    if (!IsWindow(hwnd)) return;
    // Drop the subclass first so the style change isn't blocked.
    RemoveWindowSubclass(hwnd, GuardProc, SUBCLASS_ID);
    LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
    SetWindowLongW(hwnd, GWL_EXSTYLE, ex & ~bits);
}

static void ReleaseAll()
{
    auto copy = g_added;
    g_added.clear();
    for (auto& kv : copy) Release(kv.first, kv.second);
}

static BOOL CALLBACK ApplyEnum(HWND hwnd, LPARAM)
{
    if (!IsSkinWindow(hwnd)) return TRUE;

    bool tracked = g_added.count(hwnd) != 0;

    if (IsExcluded(hwnd))
    {
        if (tracked)
        {
            DWORD bits = g_added[hwnd];
            g_added.erase(hwnd);
            Release(hwnd, bits);
        }
        return TRUE;
    }

    LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
    DWORD missing = FORCE_BITS & ~(DWORD)ex;
    if (missing)
    {
        g_added[hwnd] |= missing;
        SetWindowLongW(hwnd, GWL_EXSTYLE, ex | missing);
    }
    else if (!tracked)
    {
        return TRUE;    // already click-through on its own; leave it alone
    }

    SetWindowSubclass(hwnd, GuardProc, SUBCLASS_ID, 0);
    return TRUE;
}

// ---------------------------------------------------------------
// Fullscreen detection
// ---------------------------------------------------------------

static bool FullscreenInFront()
{
    QUERY_USER_NOTIFICATION_STATE qs;
    if (SUCCEEDED(SHQueryUserNotificationState(&qs)) &&
        (qs == QUNS_RUNNING_D3D_FULL_SCREEN || qs == QUNS_PRESENTATION_MODE))
        return true;

    HWND fg = GetForegroundWindow();
    if (!fg || IsSkinWindow(fg)) return false;

    wchar_t cls[64] = {};
    GetClassNameW(fg, cls, _countof(cls));
    if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW") ||
        !wcscmp(cls, L"Shell_TrayWnd") || !wcscmp(cls, L"Shell_SecondaryTrayWnd"))
        return false;

    RECT wr;
    if (!GetWindowRect(fg, &wr)) return false;
    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfoW(MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST), &mi))
        return false;
    const RECT& r = mi.rcMonitor;
    return wr.left <= r.left && wr.top <= r.top &&
           wr.right >= r.right && wr.bottom >= r.bottom;
}

static void Tick()
{
    bool want = false;
    for (Measure* m : g_measures)
        if (m->always) { want = true; break; }
    if (!want) want = FullscreenInFront();

    if (want)
    {
        g_forcing = true;
        EnumWindows(ApplyEnum, 0);
    }
    else if (g_forcing)
    {
        g_forcing = false;
        ReleaseAll();
    }
}

// ---------------------------------------------------------------
// Plugin exports
// ---------------------------------------------------------------

#define PLUGIN_EXPORT extern "C" __declspec(dllexport)

PLUGIN_EXPORT void Initialize(void** data, void* rm)
{
    Measure* m = new Measure;
    *data = m;
    g_measures.push_back(m);
    if (!LoadRmApi() && RmLog)
        RmLog(rm, 1, L"ClickThroughGuard: could not load Rainmeter API");
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue)
{
    Measure* m = (Measure*)data;
    *maxValue = 1.0;
    if (!LoadRmApi()) return;

    m->always = _wcsicmp(RmReadString(rm, L"Mode", L"Fullscreen", TRUE), L"Always") == 0;

    m->skinsPath = Lower(RmReplaceVariables(rm, L"#SKINSPATH#"));
    if (!m->skinsPath.empty() && m->skinsPath.back() != L'\\')
        m->skinsPath += L'\\';

    m->exclude.clear();
    std::wstring list = RmReadString(rm, L"Exclude", L"", TRUE);
    size_t start = 0;
    while (start <= list.size())
    {
        size_t bar = list.find(L'|', start);
        std::wstring item = Trim(list.substr(start, bar == std::wstring::npos ? std::wstring::npos : bar - start));
        if (!item.empty()) m->exclude.push_back(Lower(item));
        if (bar == std::wstring::npos) break;
        start = bar + 1;
    }
}

PLUGIN_EXPORT double Update(void* data)
{
    Tick();
    return g_forcing ? 1.0 : 0.0;
}

PLUGIN_EXPORT void Finalize(void* data)
{
    Measure* m = (Measure*)data;
    g_measures.erase(std::remove(g_measures.begin(), g_measures.end(), m), g_measures.end());
    delete m;
    if (g_measures.empty())
    {
        g_forcing = false;
        ReleaseAll();
    }
}
