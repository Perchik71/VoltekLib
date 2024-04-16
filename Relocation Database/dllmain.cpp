#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void WINAPI ForceLoadLibraryPerennially(HMODULE hModule)
{
    HMODULE temp;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        (LPCSTR)hModule, &temp);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
        ForceLoadLibraryPerennially(hModule);

    return TRUE;
}