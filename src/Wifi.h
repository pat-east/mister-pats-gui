#pragma once

#include <string>
#include <vector>

class Wifi {
public:
    ~Wifi();
    struct Network {
        std::string ssid;
        std::string security;
        int signalDbm = 0;
        bool open = false;
        bool supported = false;
    };

    enum class TestResult { Waiting, Connected, Failed };

    bool available() const { return !interface_.empty(); }
    const std::string &interface() const { return interface_; }
    bool refreshInterface();
    bool startScan(std::string &error);
    bool scanComplete() const { return scanTimer_ <= 0.0f; }
    void update(float deltaSeconds) { scanTimer_ -= deltaSeconds; }
    std::vector<Network> scanResults(std::string &error) const;

    bool beginTest(const std::string &ssid, const std::string &password, bool open,
                   bool hidden, std::string &error);
    TestResult pollTest(float deltaSeconds, std::string &status);
    bool needsCountryCode() const;
    bool setCountryCode(const std::string &country, std::string &error);
    bool saveTestedNetwork(std::string &error);
    void rollback();
    bool testing() const { return testNetworkId_ >= 0; }

private:
    struct SavedNetwork {
        int id = -1;
        bool enabled = false;
    };

    bool command(const std::string &commands, std::string &output, std::string &error) const;
    bool derivePsk(const std::string &ssid, const std::string &password,
                   std::string &psk, std::string &error) const;
    bool capturePreviousNetworks(std::string &error);
    bool restorePreviousNetworks();
    bool saveConfiguration(const std::string &ssid, const std::string &psk, bool open,
                           std::string &error) const;

    std::string interface_;
    float scanTimer_ = 0.0f;
    float testTimer_ = 0.0f;
    float statusPollTimer_ = 0.0f;
    float dhcpTimer_ = 0.0f;
    int testNetworkId_ = -1;
    int previousNetworkId_ = -1;
    std::vector<SavedNetwork> previousNetworks_;
    std::string testedSsid_;
    std::string testedPsk_;
    std::string countryCode_;
    bool testedOpen_ = false;
};
