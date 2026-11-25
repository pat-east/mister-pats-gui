#pragma once

namespace CrashScreen {

// Draws a stable stop screen after the GUI process has failed. This runs as a fresh process
// from the launch script, outside the failed GUI's worker and teardown paths.
int show(int exitStatus);

} // namespace CrashScreen
