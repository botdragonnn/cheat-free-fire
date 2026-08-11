#include "Render.hpp"

struct WindowSearchData {
    const char* targetClassName;
    HWND foundWindow;
};

static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam)
{
    WindowSearchData* searchData = reinterpret_cast<WindowSearchData*>(lParam);

    char className[256];
    if (RealGetWindowClassA(hwnd, className, sizeof(className)) > 0)
    {
        const char* a = searchData->targetClassName;
        const char* b = className;

        while (*a && *b && *a == *b)
        {
            a++;
            b++;
        }

        if (*a == '\0' && *b == '\0')
        {
            searchData->foundWindow = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL CALLBACK EnumChildWindowsCallback(HWND hwnd, LPARAM lParam)
{
    return EnumWindowsCallback(hwnd, lParam);
}

static BOOL CALLBACK EnumWindowsForChildSearch(HWND hwnd, LPARAM lParam)
{
    WindowSearchData* searchData = reinterpret_cast<WindowSearchData*>(lParam);
    EnumChildWindows(hwnd, EnumChildWindowsCallback, lParam);
    return (searchData->foundWindow == NULL);
}

HWND Render::LookupWindowByClassName(const char* toFindClassName)
{
    WindowSearchData searchData;
    searchData.targetClassName = toFindClassName;
    searchData.foundWindow = NULL;

    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&searchData));

    if (searchData.foundWindow == NULL)
    {
       EnumWindows(EnumWindowsForChildSearch, reinterpret_cast<LPARAM>(&searchData));
    }

    return searchData.foundWindow;
}