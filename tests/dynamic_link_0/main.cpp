#include <iostream>
#include <windows.h>

int main() {
    HMODULE hLib = LoadLibraryA("BLE_WinRT.dll");
    if (!hLib) {
        std::cout << "ERROR: BLE_WinRT.dll not found at runtime!" << std::endl;
        return 1;
    }
    auto Quit = (void(*)())GetProcAddress(hLib, "Quit");
    if (!Quit) {
        std::cout << "ERROR: Could not find Quit() in BLE_WinRT.dll!" << std::endl;
        FreeLibrary(hLib);
        return 2;
    }
    std::cout << "Dynamic link test: BLE_WinRT.dll loaded successfully." << std::endl;
    Quit();
    FreeLibrary(hLib);
    return 0;
}
