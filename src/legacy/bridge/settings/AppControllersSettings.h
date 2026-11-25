#pragma once

// Host side of the Controllers service: implements src/app/settings/ControllersService.h on top of the Classic objects.
// Classic source: src/legacy/SettingsScreen.cpp buildRows() Controllers rows; src/legacy/ControllersScreen.* (the I/O it does:
// Input::listDevices / keyState / absInfo, MisterIni, ControllerMap, Bluetooth), src/legacy/ButtonLabels.*, Preferences.
//   Needs: Context: preferences (faceButtonStyle); SettingsHost: input (the App's Input), rawInputWanted;
//   owns a Bluetooth (btpair child process), cancelled in the destructor.

#include "Bluetooth.h"
#include "app/settings/ControllersService.h"
#include "bridge/settings/AdapterEnv.h"

class AppControllersSettings : public patsgui::ControllersService {
public:
    explicit AppControllersSettings(const AdapterEnv &env) : env_(env) {}
    ~AppControllersSettings() override;

    bool available() const override;
    void update(float deltaSeconds) override;
    std::string faceButtonStyle() const override;
    void setFaceButtonStyle(const std::string &style) override;
    void refreshControllers() override;
    const std::vector<patsgui::ControllerInfo> &controllers() const override { return cache_; }
    bool bluetoothAvailable() const override;
    std::vector<patsgui::ControllerInfo> listDevices() override;
    patsgui::PadSnapshot pad(int deviceId) override;
    void setRawInputWanted(bool wanted) override;
    bool readDeadzone(int deviceId, int &value) override;
    bool saveDeadzone(int deviceId, int value) override;
    bool readMapping(int deviceId, std::array<uint32_t, 32> &slots) override;
    bool writeMapping(int deviceId, const std::array<uint32_t, 32> &slots) override;
    bool startBluetoothScan() override;
    bool nextBluetoothLine(std::string &line) override;
    void cancelBluetoothScan() override;

private:
    Input *input() const { return env_.context.settingsHost.input; }
    // The listed device with this event number (a fresh Input::listDevices() entry), or false when it is gone.
    bool find(int deviceId, patsgui::ControllerInfo &out, int *slot = nullptr) const;
    patsgui::PadSnapshot readPad(int slot) const;

    AdapterEnv env_;
    Bluetooth bluetooth_;
    std::vector<patsgui::ControllerInfo> cache_;     // the list as refreshControllers() built it (Classic's rows_)
    unsigned frame_ = 0;                             // counts update(): one pad snapshot per device and frame
    unsigned cachedFrame_ = ~0u;
    int cachedDevice_ = -1;
    patsgui::PadSnapshot cachedPad_;
};
