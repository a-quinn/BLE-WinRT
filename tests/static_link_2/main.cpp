#include <iostream>
#include <vector>
#include <windows.h>
#include "bluetooth.h"

/*
This test demonstrates static linking against the BLE_WinRT.dll, using the BluetoothClient class.
The BluetoothClient class wraps the BLE_WinRT functionality for easier use, something many users would do themselves.
*/

BluetoothClient client;

BOOL WINAPI ConsoleHandler(DWORD signal) {
	if (signal == CTRL_C_EVENT) {
		std::cout << "CTRL+C received, cleaning up..." << std::endl;
		
		// if a bluetooth device is referenced, or a characteristic subscribed, this must be called
		// to cleanup WinRT resources properly, otherwise subsequent use of WinRT Bluetooth APIs (within
		// ~15 seconds) will return "Access Denied" errors
		client.~BluetoothClient();
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

	if (!client.connectToDevice(targetMacStr)) {
		std::wcout << L"Failed to connect to device." << std::endl;
		return EXIT_FAILURE;
	}
	std::wcout << L"Connected to device." << std::endl;

	std::wcout << L"Device ID: " << client.deviceID_ << std::endl;

	std::wstring serviceUuid = L"{7F510001-1B15-11E5-B60B-1697F925EC7B}";
	std::wstring charUuid = L"{7F510019-1B15-11E5-B60B-1697F925EC7B}";

	bool ret = 0;
	ret = client.scanForCharacteristic(serviceUuid, charUuid);
	if (!ret) {
		std::wcout << L"Failed to find characteristic." << std::endl;
		return EXIT_FAILURE;
	}

	ret = client.subscribeToCharacteristic(
		serviceUuid,
		charUuid,
		[](const BLEData& data) {
			std::wcout << L"Data received from " << data.deviceId << L" Service: " << data.serviceUuid << L" Char: " << data.characteristicUuid << L" Size: " << data.size << std::endl;
			std::wcout << L"  Bytes: ";
			for (int i = 0; i < data.size; ++i) std::wcout << std::hex << (int)data.buf[i] << L" ";
			std::wcout << std::endl;
		}
	);
	if (!ret) {
		std::wcout << L"Failed to subscribe to characteristic." << std::endl;
		return EXIT_FAILURE;
	}

	// Poll for data
	BLEData data = {};
	std::wcout << L"Waiting for data..." << std::endl;
	int pollCount = 0;

	std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
	std::chrono::seconds duration(10);
	while (startTime + duration > std::chrono::steady_clock::now()) {
		if (PollData(&data, false)) {
			std::wcout << L"Data received from " << data.deviceId << L" Service: " << data.serviceUuid << L" Char: " << data.characteristicUuid << L" Size: " << data.size << std::endl;
			std::wcout << L"  Bytes: ";
			for (int i = 0; i < data.size; ++i) std::wcout << std::hex << (int)data.buf[i] << L" ";
			std::wcout << std::endl;
		pollCount++;
		}
	}

	std::cout << "Polling ended. Total polls: " << pollCount << std::endl;
	std::cout << "Polling frequency: " << (pollCount / duration.count()) << " polls per second" << std::endl;

	Quit();
	return EXIT_SUCCESS;
}
