#pragma once
#include <atomic>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <chrono>
#include <sys/resource.h>

// Process-local counters. Each value is independently sampled, not a transaction.
class RuntimeMetrics {
public:
    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    std::atomic<uint64_t> accepted{0}, active{0}, epollEvents{0}, completed{0};
    std::atomic<uint64_t> dynamicRoutes{0}, staticFiles{0}, notFound{0}, failed{0};
    nlohmann::json snapshot() const {
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
        struct rusage usage{};
        const bool cpuAvailable=getrusage(RUSAGE_SELF,&usage)==0;
        const int64_t cpuMs=(int64_t(usage.ru_utime.tv_sec)+int64_t(usage.ru_stime.tv_sec))*1000+(usage.ru_utime.tv_usec+usage.ru_stime.tv_usec)/1000;
        return {{"uptime_seconds",elapsed/1000},{"sample_monotonic_ms",elapsed},{"process_cpu_ms",cpuAvailable?nlohmann::json(cpuMs):nlohmann::json(nullptr)},{"accepted",accepted.load()},{"active",active.load()},{"epoll_events",epollEvents.load()},
                {"completed",completed.load()},{"dynamic_routes",dynamicRoutes.load()},
                {"static_files",staticFiles.load()},{"not_found",notFound.load()},{"failed",failed.load()}};
    }
};
inline RuntimeMetrics &runtimeMetrics() { static RuntimeMetrics m; return m; }
