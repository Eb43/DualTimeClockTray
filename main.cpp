//DualTimeClockTray.exe
//the app shows second clock in windows tray. it uses two icons, since windows does not allow to enlarge icon size. use this code and make with minimal edits in the simplest coding style.

#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <shellapi.h>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <commctrl.h>
#include <winreg.h>
#include <vector>
#include <string>
#include <algorithm>

#define ICON_BASE_ID 1001
#define ICON_COUNT 3               
#define ICON_ID_CLOCK ICON_BASE_ID 
#define ICON_ID_HH (ICON_BASE_ID + 1)
#define ICON_ID_MM (ICON_BASE_ID + 2)
#define WM_TRAYICON (WM_USER + 1)

HWND hwnd;
UINT_PTR timerId;
NOTIFYICONDATA nids[ICON_COUNT] = {};
HICON icons[ICON_COUNT] = {};

typedef DWORD (WINAPI *EnumDynamicTimeZoneInformationFunc)(DWORD, PDYNAMIC_TIME_ZONE_INFORMATION);
EnumDynamicTimeZoneInformationFunc pEnumDynamicTimeZoneInformation = NULL;

typedef BOOL (WINAPI *SystemTimeToTzSpecificLocalTimeExFunc)(const DYNAMIC_TIME_ZONE_INFORMATION*, const SYSTEMTIME*, LPSYSTEMTIME);
SystemTimeToTzSpecificLocalTimeExFunc pSystemTimeToTzSpecificLocalTimeEx = NULL;

std::vector<DYNAMIC_TIME_ZONE_INFORMATION> timezones;
int selectedTzIndex = -1;
int trayIconMethod = 1; // 0 = Redraw, 1 = ThreeIcons

HICON CreateTrayIcon(const char* text, int iconSize) {
    HDC hdc = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, iconSize, iconSize);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);

    RECT rect = { 0, 0, iconSize, iconSize };
    FillRect(memDC, &rect, (HBRUSH)(COLOR_WINDOW + 1));
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, RGB(0, 0, 0));

    int fontSize = iconSize;
    int fontWidth = 0;
    HFONT font = CreateFont(fontSize, fontWidth, 0, 0, FW_BLACK, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Arial");

    HFONT oldFont = (HFONT)SelectObject(memDC, font);

    SIZE sz;
    GetTextExtentPoint32A(memDC, text, strlen(text), &sz);
    int x = (iconSize - sz.cx) / 2;
    int y = (iconSize - sz.cy) / 2;
    if (sz.cx > iconSize) x = -(sz.cx - iconSize) / 2;
    TextOutA(memDC, x, y, text, strlen(text));

    SelectObject(memDC, oldFont);
    DeleteObject(font);

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = bmp;
    ii.hbmMask = CreateBitmap(iconSize, iconSize, 1, 1, NULL);
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(ii.hbmMask);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
    ReleaseDC(NULL, hdc);
    return hIcon;
}

HICON CreateClockIcon(int iconSize) {
    // Для простоты используем символ "C" как часы
    return CreateTrayIcon("C", iconSize);
}

int GetTrayIconMethodFromRegistry() {
    HKEY hKey;
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\TrayClockApp", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "TrayIconFixMethod", NULL, NULL, (LPBYTE)&value, &size);
        RegCloseKey(hKey);
    }
    return (int)value;
}

void SaveTrayIconMethodToRegistry(int method) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\TrayClockApp", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD value = (DWORD)method;
        RegSetValueExA(hKey, "TrayIconFixMethod", 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

std::string GetSelectedTzKeyNameFromRegistry() {
    HKEY hKey;
    char buffer[128] = { 0 };
    DWORD size = sizeof(buffer);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\TrayClockApp", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "TimeZoneKeyName", NULL, NULL, (LPBYTE)buffer, &size);
        RegCloseKey(hKey);
    }
    return std::string(buffer);
}

void SaveSelectedTzKeyNameToRegistry(const std::wstring& keyName) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\TrayClockApp", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"TimeZoneKeyName", 0, REG_SZ, (const BYTE*)keyName.c_str(), (DWORD)((keyName.length() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
    }
}

int FindTimezoneIndexByKeyName(const std::wstring& keyName) {
    for (int i = 0; i < (int)timezones.size(); ++i) {
        if (wcscmp(timezones[i].TimeZoneKeyName, keyName.c_str()) == 0) return i;
    }
    return -1;
}

void InitTimezoneFunctions() {
    HMODULE hAdvapi = LoadLibraryA("Advapi32.dll");
    if (hAdvapi) {
        pEnumDynamicTimeZoneInformation = (EnumDynamicTimeZoneInformationFunc)GetProcAddress(hAdvapi, "EnumDynamicTimeZoneInformation");
        pSystemTimeToTzSpecificLocalTimeEx = (SystemTimeToTzSpecificLocalTimeExFunc)GetProcAddress(hAdvapi, "SystemTimeToTzSpecificLocalTimeEx");
    }
}

void LoadTimezones() {
    InitTimezoneFunctions();
    timezones.clear();

    if (pEnumDynamicTimeZoneInformation) {
        DWORD index = 0;
        DYNAMIC_TIME_ZONE_INFORMATION tz;
        while (pEnumDynamicTimeZoneInformation(index++, &tz) == ERROR_SUCCESS) {
            timezones.push_back(tz);
        }
    } else {
        // Manual UTC±XX:00 and ±XX:30 fallback for Windows 7
        for (int bias = -720; bias <= 840; bias += 30) { // -12:00 to +14:00, step 30 min
            int hours = bias / 60;
            int mins = abs(bias % 60);

            DYNAMIC_TIME_ZONE_INFORMATION tz = {};
            tz.Bias = -bias; // Bias is opposite of UTC offset

            wchar_t sign = (bias >= 0) ? L'+' : L'-';
            int absH = abs(hours);

            // Format: UTC+05:30
            swprintf(tz.TimeZoneKeyName, 32, L"UTC%c%02d:%02d", sign, absH, mins);
            swprintf(tz.StandardName, 32, L"UTC%c%02d:%02d", sign, absH, mins);

            timezones.push_back(tz);
        }
    }

    std::sort(timezones.begin(), timezones.end(), [](const auto& a, const auto& b) {
        return a.Bias < b.Bias;
    });

    std::string lastTzKey = GetSelectedTzKeyNameFromRegistry();
    std::wstring lastTzKeyW(lastTzKey.begin(), lastTzKey.end());
    selectedTzIndex = FindTimezoneIndexByKeyName(lastTzKeyW);
    if (selectedTzIndex < 0 && !timezones.empty()) selectedTzIndex = 0;

    trayIconMethod = GetTrayIconMethodFromRegistry();
}

std::string FormatTimezone(const DYNAMIC_TIME_ZONE_INFORMATION& tz) {
    int totalBias = -(tz.Bias + tz.StandardBias);
    int hours = abs(totalBias) / 60;
    int mins = abs(totalBias) % 60;
    char sign = (totalBias >= 0) ? '+' : '-';

    int len = WideCharToMultiByte(CP_ACP, 0, tz.TimeZoneKeyName, -1, NULL, 0, NULL, NULL);
    std::string tzName(len, '\0');
    WideCharToMultiByte(CP_ACP, 0, tz.TimeZoneKeyName, -1, &tzName[0], len, NULL, NULL);
    if (!tzName.empty() && tzName.back() == '\0') tzName.pop_back();

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "UTC%c%02d:%02d %s", sign, hours, mins, tzName.c_str());

    return std::string(buffer);
}

void GetTimeInSelectedTimezone(SYSTEMTIME& stOut) {
    SYSTEMTIME utc;
    GetSystemTime(&utc);

    if (selectedTzIndex >= 0 && selectedTzIndex < (int)timezones.size()) {
        const DYNAMIC_TIME_ZONE_INFORMATION& dynTz = timezones[selectedTzIndex];
        if (pSystemTimeToTzSpecificLocalTimeEx) {
            pSystemTimeToTzSpecificLocalTimeEx(&dynTz, &utc, &stOut);
        } else {
            // Fallback: use legacy API
            TIME_ZONE_INFORMATION tz = {};
            wcscpy(tz.StandardName, dynTz.StandardName);
            tz.Bias = dynTz.Bias;
            tz.StandardBias = dynTz.StandardBias;
            tz.DaylightBias = dynTz.DaylightBias;
            tz.StandardDate = dynTz.StandardDate;
            tz.DaylightDate = dynTz.DaylightDate;
            SystemTimeToTzSpecificLocalTime(&tz, &utc, &stOut);
        }
    } else {
        stOut = utc;
    }
}

void RemoveAllIcons() {
    for (int i = 0; i < ICON_COUNT; ++i) {
        Shell_NotifyIcon(NIM_DELETE, &nids[i]);
        if (icons[i]) {
            DestroyIcon(icons[i]);
            icons[i] = NULL;
        }
    }
}

std::string FormatTooltipText(const SYSTEMTIME& st, const DYNAMIC_TIME_ZONE_INFORMATION& tz) {
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", st.wHour, st.wMinute);

    const char* weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    char dateStr[32];
    snprintf(dateStr, sizeof(dateStr), "%s, %02d %s %04d",
             weekdays[st.wDayOfWeek], st.wDay, months[st.wMonth - 1], st.wYear);

    int totalBias = -(tz.Bias + tz.StandardBias);
    int hours = abs(totalBias) / 60;
    int mins = abs(totalBias) % 60;
    char sign = (totalBias >= 0) ? '+' : '-';

    char tzName[64] = "";
    WideCharToMultiByte(CP_ACP, 0, tz.TimeZoneKeyName, -1, tzName, sizeof(tzName), NULL, NULL);

    char offsetStr[128];
    snprintf(offsetStr, sizeof(offsetStr), "UTC%c%02d:%02d %s", sign, hours, mins, tzName);

    char tooltip[256];
    snprintf(tooltip, sizeof(tooltip), "%s\n%s\n%s", timeStr, dateStr, offsetStr);

    return std::string(tooltip);
}

void UpdateClockIcons() {
    SYSTEMTIME st;
    GetTimeInSelectedTimezone(st);
    char hh[4], mm[4];
    snprintf(hh, sizeof(hh), "%02d", st.wHour);
    snprintf(mm, sizeof(mm), "%02d", st.wMinute);
    int iconSize = GetSystemMetrics(SM_CXICON);

    if (trayIconMethod == 0) {
        // Redraw method
        HICON newHourIcon = CreateTrayIcon(hh, iconSize);
        HICON newMinuteIcon = CreateTrayIcon(mm, iconSize);

        // Удаляем старые иконки перед заменой
        if (icons[0]) {
            Shell_NotifyIcon(NIM_DELETE, &nids[0]);
            DestroyIcon(icons[0]);
            icons[0] = NULL;
        }
        if (icons[1]) {
            Shell_NotifyIcon(NIM_DELETE, &nids[1]);
            DestroyIcon(icons[1]);
            icons[1] = NULL;
        }

        icons[0] = newHourIcon;
        icons[1] = newMinuteIcon;

        std::string tooltip = FormatTooltipText(st, timezones[selectedTzIndex]);

        nids[0].hIcon = icons[0];
        strncpy(nids[0].szTip, tooltip.c_str(), sizeof(nids[0].szTip) - 1);
        nids[0].szTip[sizeof(nids[0].szTip) - 1] = '\0';
        Shell_NotifyIcon(NIM_ADD, &nids[0]);

        nids[1].hIcon = icons[1];
        strncpy(nids[1].szTip, tooltip.c_str(), sizeof(nids[1].szTip) - 1);
        nids[1].szTip[sizeof(nids[1].szTip) - 1] = '\0';
        Shell_NotifyIcon(NIM_ADD, &nids[1]);
    } else {
        // Three icons method
        HICON newClockIcon = CreateClockIcon(iconSize);
        HICON newHourIcon = CreateTrayIcon(hh, iconSize);
        HICON newMinuteIcon = CreateTrayIcon(mm, iconSize);

        if (icons[0]) DestroyIcon(icons[0]);
        if (icons[1]) DestroyIcon(icons[1]);
        if (icons[2]) DestroyIcon(icons[2]);

        icons[0] = newClockIcon;
        icons[1] = newHourIcon;
        icons[2] = newMinuteIcon;

        std::string tooltip = FormatTooltipText(st, timezones[selectedTzIndex]);

        nids[0].hIcon = icons[0];
        strncpy(nids[0].szTip, "Clock Symbol", sizeof(nids[0].szTip) - 1);
        nids[0].szTip[sizeof(nids[0].szTip) - 1] = '\0';

        nids[1].hIcon = icons[1];
        strncpy(nids[1].szTip, tooltip.c_str(), sizeof(nids[1].szTip) - 1);
        nids[1].szTip[sizeof(nids[1].szTip) - 1] = '\0';

        nids[2].hIcon = icons[2];
        strncpy(nids[2].szTip, tooltip.c_str(), sizeof(nids[2].szTip) - 1);
        nids[2].szTip[sizeof(nids[2].szTip) - 1] = '\0';

        Shell_NotifyIcon(NIM_MODIFY, &nids[0]);
        Shell_NotifyIcon(NIM_MODIFY, &nids[1]);
        Shell_NotifyIcon(NIM_MODIFY, &nids[2]);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
case WM_CREATE: {
    RemoveAllIcons();
    Sleep(100); // задержка, чтобы Explorer успел обработать удаление

    LoadTimezones();
    SYSTEMTIME st;
    GetTimeInSelectedTimezone(st);
    char hh[4], mm[4];
    snprintf(hh, sizeof(hh), "%02d", st.wHour);
    snprintf(mm, sizeof(mm), "%02d", st.wMinute);
    int iconSize = GetSystemMetrics(SM_CXICON);

    if (trayIconMethod == 0) {
        // Redraw method - только 2 иконки
        icons[0] = CreateTrayIcon(hh, iconSize);
        icons[1] = CreateTrayIcon(mm, iconSize);

        nids[0].cbSize = sizeof(NOTIFYICONDATA);
        nids[0].hWnd = hWnd;
        nids[0].uID = ICON_ID_HH;
        nids[0].uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
        nids[0].uCallbackMessage = WM_TRAYICON;
        nids[0].hIcon = icons[0];
        std::string tooltip = FormatTooltipText(st, timezones[selectedTzIndex]);
        strncpy(nids[0].szTip, tooltip.c_str(), sizeof(nids[0].szTip) - 1);
        nids[0].szTip[sizeof(nids[0].szTip) - 1] = '\0';

        nids[1].cbSize = sizeof(NOTIFYICONDATA);
        nids[1].hWnd = hWnd;
        nids[1].uID = ICON_ID_MM;
        nids[1].uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
        nids[1].uCallbackMessage = WM_TRAYICON;
        nids[1].hIcon = icons[1];
        strncpy(nids[1].szTip, tooltip.c_str(), sizeof(nids[1].szTip) - 1);
        nids[1].szTip[sizeof(nids[1].szTip) - 1] = '\0';

        Shell_NotifyIcon(NIM_ADD, &nids[0]);
        Shell_NotifyIcon(NIM_ADD, &nids[1]);
    } else {
        // Three icons method
        icons[0] = CreateClockIcon(iconSize);
        icons[1] = CreateTrayIcon(hh, iconSize);
        icons[2] = CreateTrayIcon(mm, iconSize);

        nids[0].cbSize = sizeof(NOTIFYICONDATA);
        nids[0].hWnd = hWnd;
        nids[0].uID = ICON_ID_CLOCK;
        nids[0].uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
        nids[0].uCallbackMessage = WM_TRAYICON;
        nids[0].hIcon = icons[0];
        strncpy(nids[0].szTip, "Clock Symbol", sizeof(nids[0].szTip) - 1);
        nids[0].szTip[sizeof(nids[0].szTip) - 1] = '\0';

        nids[1].cbSize = sizeof(NOTIFYICONDATA);
        nids[1].hWnd = hWnd;
        nids[1].uID = ICON_ID_HH;
        nids[1].uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
        nids[1].uCallbackMessage = WM_TRAYICON;
        nids[1].hIcon = icons[1];
        std::string tooltip = FormatTooltipText(st, timezones[selectedTzIndex]);
        strncpy(nids[1].szTip, tooltip.c_str(), sizeof(nids[1].szTip) - 1);
        nids[1].szTip[sizeof(nids[1].szTip) - 1] = '\0';

        nids[2].cbSize = sizeof(NOTIFYICONDATA);
        nids[2].hWnd = hWnd;
        nids[2].uID = ICON_ID_MM;
        nids[2].uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
        nids[2].uCallbackMessage = WM_TRAYICON;
        nids[2].hIcon = icons[2];
        strncpy(nids[2].szTip, tooltip.c_str(), sizeof(nids[2].szTip) - 1);
        nids[2].szTip[sizeof(nids[2].szTip) - 1] = '\0';

        Shell_NotifyIcon(NIM_ADD, &nids[0]);
        Shell_NotifyIcon(NIM_ADD, &nids[1]);
        Shell_NotifyIcon(NIM_ADD, &nids[2]);
    }

    timerId = SetTimer(hWnd, 1, 1000, NULL);
    return 0;
}

case WM_TIMER: {
    if (trayIconMethod == 0) {
        // Redraw method - обновляем каждые 30 секунд или при смене минуты
        static int lastMinute = -1;
        static int secondsCounter = 0;
        SYSTEMTIME st;
        GetTimeInSelectedTimezone(st);
        secondsCounter++;
        if ((lastMinute != st.wMinute) ||  (secondsCounter >= 30)) {
            UpdateClockIcons();
            lastMinute = st.wMinute;
            secondsCounter = 0;
        }
    } else {
        // Three icons method - обновляем каждую секунду
        SYSTEMTIME st;
        GetTimeInSelectedTimezone(st);
        UpdateClockIcons();
    }
    return 0;
}

    case WM_TRAYICON: {
        if (lParam == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            HMENU tzMenu = CreatePopupMenu();
            HMENU methodMenu = CreatePopupMenu();

            int total = (int)timezones.size();
            int offset = selectedTzIndex - 7;
            if (offset < 0) offset = 0;
            if (offset > total - 15) offset = total - 15;
            if (offset < 0) offset = 0;

            for (int i = 0; i < total; ++i) {
                int idx = (i + offset) % total;
                std::string label = FormatTimezone(timezones[idx]);
                UINT flags = MF_STRING | (idx == selectedTzIndex ? MF_CHECKED : 0);
                AppendMenuA(tzMenu, flags, 1000 + idx, label.c_str());
            }

            // Добавляем подменю для выбора метода
            UINT redrawFlags = MF_STRING | (trayIconMethod == 0 ? MF_CHECKED : 0);
            UINT threeIconsFlags = MF_STRING | (trayIconMethod == 1 ? MF_CHECKED : 0);
            AppendMenuA(methodMenu, redrawFlags, 2000, "Redraw");
            AppendMenuA(methodMenu, threeIconsFlags, 2001, "Three Icons");

            AppendMenu(menu, MF_POPUP, (UINT_PTR)tzMenu, "Select Timezone");
            AppendMenu(menu, MF_POPUP, (UINT_PTR)methodMenu, "Icon Fix Method");
            AppendMenu(menu, MF_STRING, 1, "Exit");

            POINT pt;
            GetCursorPos(&pt);

            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            PostMessage(hwnd, WM_NULL, 0, 0);

            DestroyMenu(tzMenu);
            DestroyMenu(methodMenu);
            DestroyMenu(menu);

            if (cmd == 1) {
                PostQuitMessage(0);
            } else if (cmd >= 1000 && cmd < 1000 + (int)timezones.size()) {
                selectedTzIndex = cmd - 1000;
                SaveSelectedTzKeyNameToRegistry(timezones[selectedTzIndex].TimeZoneKeyName);
                UpdateClockIcons();
            } else if (cmd == 2000 || cmd == 2001) {
                // Смена метода иконок
                int newMethod = (cmd == 2000) ? 0 : 1;
                if (newMethod != trayIconMethod) {
                    trayIconMethod = newMethod;
                    SaveTrayIconMethodToRegistry(trayIconMethod);
                    
                    // Пересоздаем иконки с новым методом
                    RemoveAllIcons();
                    Sleep(100);
                    SendMessage(hwnd, WM_CREATE, 0, 0);
                }
            }
        }
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hWnd, timerId);
        Sleep(100);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "TrayClockApp";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClass(&wc);

    hwnd = CreateWindowEx(0, wc.lpszClassName, "HiddenWindow", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInst, NULL);

    if (!hwnd) {
        MessageBox(NULL, "Window creation failed!", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, SW_HIDE);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyWindow(hwnd);
    RemoveAllIcons();
    Sleep(100);
    return 0;
}