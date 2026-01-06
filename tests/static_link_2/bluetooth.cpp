#include "bluetooth.h"

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

std::vector<std::string> listDevices(std::chrono::duration<double> timeout) {
    StartDeviceScan();
    DeviceUpdate device = {};
    std::vector<std::string> devices;
    auto startTime = std::chrono::steady_clock::now();
    while (startTime + timeout > std::chrono::steady_clock::now()) {
        ScanStatus status = PollDevice(&device, true);
        if (status == ScanStatus::AVAILABLE) {
            devices.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(device.name));
        }
        if (status == ScanStatus::FINISHED) break;
    }
    StopDeviceScan();
    return devices;
};

void scanDevices(std::function<void(const DeviceUpdate&)> callback, std::chrono::duration<double> timeout) {
    StartDeviceScan();
    DeviceUpdate device = {};
    auto startTime = std::chrono::steady_clock::now();
    while (startTime + timeout > std::chrono::steady_clock::now()) {
        ScanStatus status = PollDevice(&device, true);
        if (status == ScanStatus::AVAILABLE) {
            callback(device);
        }
        if (status == ScanStatus::FINISHED) break;
    }
    StopDeviceScan();
};

bool BluetoothClient::connectToDevice(const std::wstring& deviceId) {
	StartDeviceScan();
	bool found = false;
	while (!found) {
		DeviceUpdate device = {};
		ScanStatus status = PollDevice(&device, true);
		if (status == ScanStatus::AVAILABLE || status == ScanStatus::FINISHED) {
			if (compareMAC(device.id, deviceId.c_str())) {
				found = true;
				wcscpy_s(deviceID_, device.id);
			}
		}
		if (status == ScanStatus::FINISHED) break;
	}
	StopDeviceScan();
	return found;
}


bool BluetoothClient::scanForService(const std::wstring& serviceId) {
	ScanServices(deviceID_);
	bool found = false;
	Service service = {};
	while (true) {
		ScanStatus status = PollService(&service, false);
		if (status == ScanStatus::AVAILABLE) {
			if (_wcsicmp(service.uuid, serviceId.c_str()) == 0) {
				found = true;
			}
		}
		if (status == ScanStatus::FINISHED) break;
	}
	StopDeviceScan();
	return found;
}

bool BluetoothClient::scanForCharacteristic(const std::wstring& serviceId, const std::wstring& characteristicId) {
	ScanCharacteristics(deviceID_, (wchar_t*)serviceId.c_str());
	bool found = false;
	Characteristic characteristic = {};
	while (true) {
		ScanStatus status = PollCharacteristic(&characteristic, false);
		if (status == ScanStatus::AVAILABLE) {
			if (_wcsicmp(characteristic.uuid, characteristicId.c_str()) == 0) {
				found = true;
			}
		}
		if (status == ScanStatus::FINISHED) break;
	}
	StopDeviceScan();
	return found;
}

bool BluetoothClient::subscribeToCharacteristic(const std::wstring& serviceId, const std::wstring& characteristicId, std::function<void(const BLEData&)> dataCallback) {
	ScanServices(deviceID_);
	// print all args
	std::wcout << L"Subscribing to characteristic:" << std::endl;
	std::wcout << L"  Device ID: " << deviceID_ << std::endl;
	std::wcout << L"  Service ID: " << serviceId << std::endl;
	std::wcout << L"  Characteristic ID: " << characteristicId << std::endl;
	bool result = SubscribeCharacteristic(deviceID_, (wchar_t*)serviceId.c_str(), (wchar_t*)characteristicId.c_str(), true);
	if (result) {

		std::thread([dataCallback, serviceId, characteristicId, currentDeviceId = deviceID_]() {
			BLEData data = {};
			const wchar_t* serviceUuid = serviceId.c_str();
			const wchar_t* characteristicUuid = characteristicId.c_str();
			const wchar_t* deviceUuid = currentDeviceId;
			while (true) {
				if (PollData(&data, true)) {
					if (_wcsicmp(data.characteristicUuid, characteristicUuid) == 0 &&
						_wcsicmp(data.serviceUuid, serviceUuid) == 0 &&
						_wcsicmp(data.deviceId, deviceUuid) == 0) {
						dataCallback(data);
					}
				}
			}
		}).detach();
	}
	return result;
}

bool BluetoothClient::unsubscribe(const std::wstring& serviceId, const std::wstring& characteristicId) {
	return UnsubscribeCharacteristic(deviceID_, (wchar_t*)serviceId.c_str(), (wchar_t*)characteristicId.c_str(), true);
}