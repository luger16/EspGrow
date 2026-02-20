#include "time_utils.h"
#include "wifi_manager.h"
#include <time.h>

namespace TimeUtils {

namespace {
    int timezoneOffsetMinutes = 0;
    
    int parseTimeToMinutes(const char* hhmm) {
        int hours = 0, minutes = 0;
        if (sscanf(hhmm, "%d:%d", &hours, &minutes) >= 1) {
            return hours * 60 + minutes;
        }
        return 0;
    }
}

void setTimezoneOffset(int minutes) {
    timezoneOffsetMinutes = minutes;
}

bool isTimeInRange(const char* startHHMM, const char* endHHMM) {
    if (!WiFiManager::isTimeSynced()) return false;
    
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    int currentMinutes = tm_info->tm_hour * 60 + tm_info->tm_min;
    
    currentMinutes += timezoneOffsetMinutes;
    while (currentMinutes < 0) currentMinutes += 24 * 60;
    while (currentMinutes >= 24 * 60) currentMinutes -= 24 * 60;
    
    int startMinutes = parseTimeToMinutes(startHHMM);
    int endMinutes = parseTimeToMinutes(endHHMM);
    
    if (startMinutes <= endMinutes) {
        return currentMinutes >= startMinutes && currentMinutes <= endMinutes;
    } else {
        return currentMinutes >= startMinutes || currentMinutes <= endMinutes;
    }
}

String getCurrentTimeHHMM() {
    if (!WiFiManager::isTimeSynced()) return "--:--";
    
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    int hours = tm_info->tm_hour;
    int minutes = tm_info->tm_min;
    
    hours += timezoneOffsetMinutes / 60;
    minutes += timezoneOffsetMinutes % 60;
    
    while (minutes < 0) { minutes += 60; hours--; }
    while (minutes >= 60) { minutes -= 60; hours++; }
    while (hours < 0) hours += 24;
    while (hours >= 24) hours -= 24;
    
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", hours, minutes);
    return String(buf);
}

}
