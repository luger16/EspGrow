#pragma once

#include <Arduino.h>
#include <functional>
#include <ESPAsyncWebServer.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0.0"
#endif

#ifndef GITHUB_REPO
#error "GITHUB_REPO must be defined via build_flags (e.g. -DGITHUB_REPO=\\\"owner/repo\\\")"
#endif

namespace OtaManager {

enum class Status {
    Idle,
    Uploading,
    Downloading,
    Installing,
    Success,
    Error,
    Rebooting
};

struct StatusEvent {
    Status status;
    int progress;     // 0-100, or -1 if not applicable
    String error;     // non-empty only for Status::Error
};

struct ReleaseInfo {
    String latestVersion;
    String downloadUrl;
    String releaseUrl;
    String publishedAt;
    int size;
    bool valid;
};

using StatusCallback = std::function<void(const StatusEvent&)>;

// Initialize OTA manager and register HTTP routes on the server.
// The callback receives coarse status events (start/progress/success/error/reboot).
// WebSocket bridging is the caller's responsibility.
void begin(AsyncWebServer* server, StatusCallback onStatus);

// Check rollback state after boot. Call in setup() AFTER all services are
// confirmed healthy (FS mounted, config loaded, sensors initialized).
// Returns true if rollback validation was performed (i.e., we just booted
// from a freshly-written OTA partition).
bool validateRollback();

// Manually trigger a background GitHub release check.
// Results are cached and served by the /api/ota/check endpoint.
void refreshGitHubCache();

// Get current OTA status (for polling, if needed).
Status getStatus();

// Get cached release info (populated by background GitHub check).
ReleaseInfo getCachedRelease();

// Get the chip model suffix used for binary naming (e.g., "c3", "s3").
String getChipModel();

}
