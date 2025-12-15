#pragma once
#include <chrono>
#include <iostream>
#include <vector>
#include <cstdint>

// PerfStats (간단한 통계)
struct PerfStats {
    double last_ms = 0.0;
    double avg_ms = 0.0;
    double min_ms = 1e9;
    double max_ms = 0.0;
    uint64_t count = 0;

    void add(double ms) {
        last_ms = ms;
        count++;
        avg_ms += (ms - avg_ms) / count;
        if (ms < min_ms) min_ms = ms;
        if (ms > max_ms) max_ms = ms;
    }

    void print(const char* tag) const {
        std::cout
            << "[" << tag << "] avg=" << avg_ms
            << " ms, min=" << min_ms
            << " ms, max=" << max_ms
            << " ms, last=" << last_ms
            << " ms, count=" << count
            << "\n";
    }
};

inline double now_ms() {
    using namespace std::chrono;
    return duration<double, std::milli>(
        high_resolution_clock::now().time_since_epoch()).count();
}
