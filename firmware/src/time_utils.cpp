#include "time_utils.h"
#include "wifi_manager.h"
#include <time.h>

namespace TimeUtils {

namespace {
    int parseTimeToMinutes(const char* hhmm) {
        int hours = 0, minutes = 0;
        if (sscanf(hhmm, "%d:%d", &hours, &minutes) >= 1) {
            return hours * 60 + minutes;
        }
        return 0;
    }
}

bool isTimeInRange(const char* startHHMM, const char* endHHMM) {
    if (!WiFiManager::isTimeSynced()) return false;
    
    time_t now = time(nullptr);
    struct tm* tm_info = gmtime(&now);
    int currentMinutes = tm_info->tm_hour * 60 + tm_info->tm_min;
    
    int startMinutes = parseTimeToMinutes(startHHMM);
    int endMinutes = parseTimeToMinutes(endHHMM);
    
    if (startMinutes <= endMinutes) {
        return currentMinutes >= startMinutes && currentMinutes < endMinutes;
    } else {
        return currentMinutes >= startMinutes || currentMinutes < endMinutes;
    }
}

}
