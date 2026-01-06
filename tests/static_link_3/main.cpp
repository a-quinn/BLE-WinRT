#include <iostream>
#include <vector>
#include <windows.h>

#include "BLE_WinRT_Context.h"
#include <cstdio> // For _wcsicmp on Windows

bool compareMAC(const wchar_t* deviceId, const wchar_t* targetMacStr) {
	// device.id look like "BluetoothLE#BluetoothLE88:d8:2e:42:c4:e3-d9:32:8a:f9:2a:ad"
	// targetMacStr look like "88d82e42c4e3"
	// from device.id remove prefix by scanning until '-' then remove all ':'

	const wchar_t* macStart = wcschr(deviceId, L'-');
	if (macStart) {
		macStart++; // move past '-'
		std::wstring macStr;
		for (const wchar_t* p = macStart; *p != L'\0'; ++p) {
			if (*p != L':') {
				macStr += *p;
			}
		}
		return macStr == targetMacStr;
	}
	return false;
}

auto bl_device = BLEWinRTContext();
BOOL WINAPI ConsoleHandler(DWORD signal) {
	if (signal == CTRL_C_EVENT) {
		std::cout << "CTRL+C received, cleaning up..." << std::endl;

		// if a bluetooth device is referenced, or a characteristic subscribed, this must be called
		// to cleanup WinRT resources properly, otherwise subsequent use of WinRT Bluetooth APIs (within
		// ~15 seconds) will return "Access Denied" errors
		bl_device.Quit();
		ExitProcess(0);
	}
	return TRUE;
}

int main()
{
	// Set the console control handler to handle CTRL+C
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);

	// Find device with MAC address 'd7:d1:95:6e:12:c6'
	uint64_t targetMac = 0xD7D1956E12C6; // 3384

	wchar_t targetMacStr[100];
	swprintf(targetMacStr, 100, L"%012llx", targetMac);
	std::wcout << L"Looking for MAC: " << targetMacStr << std::endl;

	bl_device.StartDeviceScan();
	DeviceUpdate device = {};
	bool found = false;
	std::wcout << L"Scanning for devices..." << std::endl;
	while (!found) {
		ScanStatus status = bl_device.PollDevice(&device, true);
		if (status == ScanStatus::AVAILABLE || status == ScanStatus::FINISHED) {
			//std::wcout << L"Device found: " << device.id << L" Name: " << device.name << std::endl;
			if (compareMAC(device.id, targetMacStr)) {
				std::wcout << L"Found device: " << device.id << L" Name: " << device.name << std::endl;
				found = true;
			}
		}
		if (status == ScanStatus::FINISHED) break;
	}
	bl_device.StopDeviceScan();
	if (!found) {
		std::wcout << L"Device not found." << std::endl;
		return EXIT_FAILURE;
	}

	// Scan services
	bl_device.ScanServices(device.id);
	Service service = {};
	std::vector<std::wstring> serviceUuids;
	std::wcout << L"Services:" << std::endl;
	while (true) {
		ScanStatus status = bl_device.PollService(&service, false);
		if (status == ScanStatus::AVAILABLE) {
			std::wcout << L"  " << service.uuid << std::endl;
			serviceUuids.push_back(service.uuid);
		}
		if (status == ScanStatus::FINISHED) break;
	}

	// Scan characteristics for first service
	if (serviceUuids.empty()) {
		std::wcout << L"No services found." << std::endl;
		return EXIT_FAILURE;
	}

	std::wstring serviceUuid = L"{7F510001-1B15-11E5-B60B-1697F925EC7B}";
	//std::wcout << L"Scanning characteristics for service " << serviceUuid << L"..." << std::endl;
	bool serviceFound = false;
	for (const auto& suuid : serviceUuids) {
		if (_wcsicmp(suuid.c_str(), serviceUuid.c_str()) == 0) {
			serviceFound = true;
			break;
		}
	}

	if (!serviceFound) {
		std::wcout << L"Service " << serviceUuid << L" not found." << std::endl;
		return EXIT_FAILURE;
	}

	bl_device.ScanCharacteristics(device.id, (wchar_t*)serviceUuid.c_str());
	Characteristic characteristic = {};
	std::vector<std::wstring> charUuids;
	std::wcout << L"Characteristics for service " << serviceUuid << L":" << std::endl;
	while (true) {
		ScanStatus status = bl_device.PollCharacteristic(&characteristic, false);
		if (status == ScanStatus::AVAILABLE) {
			std::wcout << L"  " << characteristic.uuid << L" Desc: " << characteristic.userDescription << std::endl;
			charUuids.push_back(characteristic.uuid);
		}
		if (status == ScanStatus::FINISHED) break;
	}
	if (charUuids.empty()) {
		std::wcout << L"No characteristics found." << std::endl;
		return EXIT_FAILURE;
	}

	std::wstring charUuidToSubscribe = L"{7F510019-1B15-11E5-B60B-1697F925EC7B}";

	bool charFound = false;
	for (const auto& cuuid : charUuids) {
		if (_wcsicmp(cuuid.c_str(), charUuidToSubscribe.c_str()) == 0) {
			charFound = true;
			break;
		}
	}
	if (!charFound) {
		std::wcout << L"Characteristic " << charUuidToSubscribe << L" not found." << std::endl;
		return EXIT_FAILURE;
	}

	std::wcout << L"Subscribing to characteristic " << charUuidToSubscribe << L"..." << std::endl;
	bool subOk = bl_device.SubscribeCharacteristic(device.id, (wchar_t*)serviceUuid.c_str(), (wchar_t*)charUuidToSubscribe.c_str(), true);
	if (!subOk) {
		std::wcout << L"Failed to subscribe." << std::endl;
		return EXIT_FAILURE;
	}

	// Poll for data
	BLEData data = {};
	std::wcout << L"Waiting for data..." << std::endl;
	int pollCount = 0;

	std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
	std::chrono::seconds duration(10);
	while (startTime + duration > std::chrono::steady_clock::now()) {
		if (bl_device.PollData(&data, false)) {
			std::wcout << L"Data received from " << data.deviceId << L" Service: " << data.serviceUuid << L" Char: " << data.characteristicUuid << L" Size: " << data.size << std::endl;
			std::wcout << L"  Bytes: ";
			for (int i = 0; i < data.size; ++i) std::wcout << std::hex << (int)data.buf[i] << L" ";
			std::wcout << std::endl;
		pollCount++;
		}
	}

	std::cout << "Polling ended. Total polls: " << pollCount << std::endl;
	std::cout << "Polling frequency: " << (pollCount / duration.count()) << " polls per second" << std::endl;

	bl_device.Quit();
	return EXIT_SUCCESS;
}
