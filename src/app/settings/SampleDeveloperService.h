#pragma once

// Sample Developer service: counts how often the Wireframes were opened. `developer.build=0` is the hidden-category case.

#include "app/settings/DeveloperService.h"
#include "app/settings/SampleControl.h"

namespace patsgui {

class SampleDeveloperService : public DeveloperService, public SampleControl {
public:
    bool devBuild = true;
    int openedWireframes = 0;   // how many times the demo was opened
    bool available() const override { return true; }
    bool developmentBuild() const override { return devBuild; }
    void openWireframes() override { ++openedWireframes; }
    bool set(const std::string &key, const std::string &v) override {
        if (key == "developer.build") { devBuild = sampleBool(v); return true; }
        return false;
    }
    std::vector<std::string> help() const override { return {"developer.build=0|1"}; }
};

}  // namespace patsgui
