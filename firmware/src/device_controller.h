#pragma once

#include <Arduino.h>
#include <functional>

namespace DeviceController {

    struct QueryResult {
        bool reachable = false;
        bool isOn = false;
    };

    struct AsyncResult {
        char method[16];
        char target[40];
        QueryResult result;
        bool wasControl;      // true = control, false = query
        bool requestedState;  // the `on` param from controlAsync()
    };

    using ResultCallback = std::function<void(const AsyncResult& result)>;

    void init();
    void loop();

    void onResult(ResultCallback cb);

    bool controlAsync(const String& method, const String& target, bool on);
    bool queryAsync(const String& method, const String& target);

    bool busy();
}
