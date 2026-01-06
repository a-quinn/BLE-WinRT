# BLE-WinRT

This project is a cleaned-up CMake-based version of [BleWinrtDll](https://github.com/adabru/BleWinrtDll), providing a C++ library for Bluetooth Low Energy (BLE) using WinRT APIs. The library can be built as a static or shared library and included in other projects. Thank you [Adam Brunnmeier](https://github.com/adabru) for creating it.

## Building

You need CMake and a compatible Visual Studio generator (e.g., Visual Studio 2019 or newer).

### Static build (static tests, no DLL required)

```powershell
cmake -S . -B build_static -G "Visual Studio 16 2019" -DBLE_WINRT_BUILD_SHARED=OFF
cmake --build build_static --config Release
```

### Dynamic build (dynamic tests, DLL required)

```powershell
cmake -S . -B build_dynamic -G "Visual Studio 16 2019" -DBLE_WINRT_BUILD_SHARED=ON
cmake --build build_dynamic --config Release
```


## Test Projects

This repo includes several test projects demonstrating different ways to link and use the BLE_WinRT library:

- `test_dynamic_link_0.exe` — runtime load library
- `test_dynamic_link_1.exe` — runtime load, scan for devices, with header file
- `test_dynamic_link_2.exe` — runtime load, scan for devices, without header file
- `test_static_link_0.exe` — static link library
- `test_static_link_1.exe` — static link, scan for devices and stream data
- `test_static_link_2.exe` — static link, example using a wrapper class
- `test_static_link_3.exe` — static link, example using non-global BLE_WinRT_Context.
- `test_static_link_4.exe` — static link, example using a wrapper class and non-global BLE_WinRT_Context.


To run a test, open a terminal in `build_static/bin/Release` or `build_dynamic/bin/Release` and execute the desired `.exe` file. For dynamic tests, ensure `BLE_WinRT.dll` is present.

## Usage in other projects

You can include `BLE_WinRT` or `BLE_WinRT_Context` as a subdirectory in your own CMake project and link against it, or use the built libraries.

## License

Original code: WTFPL. See [BleWinrtDll](https://github.com/adabru/BleWinrtDll).

## Useful links to references for odd behaviour
https://learn.microsoft.com/en-us/answers/questions/701689/getcharacteristicsasync-function-gets-stuck-in-win
https://developercommunity.visualstudio.com/t/ble-using-winrt-access-denied-when-executing-getch/1703310
https://stackoverflow.com/questions/35420940/windows-uwp-connect-to-ble-device-after-discovery/36106137#36106137
https://github.com/hbldh/bleak/issues/1217