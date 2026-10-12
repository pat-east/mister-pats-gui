#pragma once

#include <string>

#include "DiscDriveService.h"

// Inspects the scoped USB CDROM Loader installation. It never reads or writes the global
// MiSTer main= setting.
class DiscCoreManager {
public:
    struct Status {
        bool alternateMainInstalled = false;
        bool psxMglInstalled = false;
        bool saturnMglInstalled = false;
        bool psxMglCompatible = false;
        bool saturnMglCompatible = false;
        bool psxCoreInstalled = false;
        bool saturnCoreInstalled = false;
        bool scopedMainConfigured = false;
        std::string psxMgl;
        std::string saturnMgl;

        bool packageReady() const {
            return alternateMainInstalled && psxMglCompatible && saturnMglCompatible &&
                   scopedMainConfigured;
        }
        bool ready() const {
            return packageReady() && psxCoreInstalled && saturnCoreInstalled;
        }
        bool readyFor(DiscDriveService::System system) const {
            return alternateMainInstalled && scopedMainConfigured &&
                   (system == DiscDriveService::System::PlayStation
                        ? psxMglCompatible && psxCoreInstalled
                        : system == DiscDriveService::System::Saturn
                            ? saturnMglCompatible && saturnCoreInstalled : false);
        }
        bool hasAnyCore() const { return psxMglInstalled || saturnMglInstalled; }
    };

    Status inspect(const std::string &fatRoot = "/media/fat",
                   const std::string &iniPath = "/media/fat/MiSTer.ini") const;
    static const char *projectUrl();
};
