// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
#include <fmt/format.h>

#include "common/logging/log.h"
#include "common/native_clock.h"
#include "core/libraries/kernel/time.h"
#include "video_core/gpu_profiler.h"

namespace VideoCore::Prof {

namespace {

constexpr u32 kWindowFrames = 90;

constexpr const char* kNames[] = {
    "other", "idle", "vo",    "reg",    "ce",   "host", "pframe", "draw",
    "disp",  "pipe", "tgt",   "buf",    "tex",  "vtx",  "desc",   "bufup",
    "imgup", "dma",  "fence", "submit", "flip", "cmds", "rb",     "cal",
};
static_assert(std::size(kNames) == kNumCats);

constexpr bool IsWait(Cat cat) {
    return cat == Cat::VoWait || cat == Cat::RegWait || cat == Cat::CeWait ||
           cat == Cat::HostWait || cat == Cat::FrameWait;
}

constexpr u32 Idx(Cat cat) {
    return static_cast<u32>(cat);
}

constexpr u32 Idx(Global global) {
    return static_cast<u32>(global);
}

struct Snapshot {
    std::array<u64, kNumCats> self{};
    std::array<u64, kNumCats> calls{};
    std::array<u64, kNumGlobals> global_ticks{};
    std::array<u64, kNumGlobals> global_calls{};
    u64 transitions = 0;
    u64 vo_checks = 0;
    u64 time = 0;
};

struct Window {
    u32 frames = 0;
    u64 interval = 0;
    std::array<u64, kNumCats> self{};
    std::array<u64, kNumCats> calls{};
    std::array<u64, kNumGlobals> global_ticks{};
    std::array<u64, kNumGlobals> global_calls{};
    u64 transitions = 0;
    u64 vo_checks = 0;
    u64 tail = 0;
    u32 tail_frames = 0;
    u32 slow = 0; // flips that came more than 1.5 vblanks after the previous one
    u32 over = 0; // frames whose work alone took longer than a vblank
    std::vector<double> interval_ms;
    std::vector<double> work_ms;
    double worst_work = -1.0;
    double worst_interval = 0.0;
    std::array<u64, kNumCats> worst_self{};
};

double Percentile(std::vector<double> values, double p) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const auto i = std::min(values.size() - 1, static_cast<size_t>(p * values.size()));
    return values[i];
}

void LogWindow(const Window& w, double ms_per_tick, double scope_cost_ms, double budget_ms) {
    const double n = w.frames;
    const auto ms = [&](u64 ticks) { return static_cast<double>(ticks) * ms_per_tick; };
    const auto avg = [&](Cat c) { return ms(w.self[Idx(c)]) / n; };
    const auto per = [&](Cat c) { return static_cast<double>(w.calls[Idx(c)]) / n; };
    const auto g_avg = [&](Global g) { return ms(w.global_ticks[Idx(g)]) / n; };
    const auto g_per = [&](Global g) { return static_cast<double>(w.global_calls[Idx(g)]) / n; };
    std::array<double, kNumGlobals> g_max{};
    for (u32 i = 0; i < kNumGlobals; ++i) {
        g_max[i] = ms(g_counters[i].max.exchange(0, std::memory_order_relaxed));
    }

    const double frame = ms(w.interval) / n;
    const double idle = avg(Cat::Idle);
    double waits = 0.0;
    for (u32 i = 0; i < kNumCats; ++i) {
        if (IsWait(static_cast<Cat>(i))) {
            waits += avg(static_cast<Cat>(i));
        }
    }
    const double work = frame - idle - waits;
    const double prof = static_cast<double>(w.transitions) * scope_cost_ms * 0.5 / n;

    LOG_INFO(Render,
             "GpuProf {}f | frame {:.2f} (p50 {:.2f} p90 {:.2f} max {:.2f}, slow {}) | idle {:.2f} "
             "| wait {:.2f}: vo {:.2f} ({:.1f} checks/f, {:.1f} sleeps/f, tail {:.2f}) reg {:.2f} "
             "ce {:.2f} host {:.2f} ({:.1f}/f) pframe {:.2f} | work {:.2f} (p50 {:.2f} p90 {:.2f} "
             "max {:.2f}, over {:.1f}: {})",
             w.frames, frame, Percentile(w.interval_ms, 0.5), Percentile(w.interval_ms, 0.9),
             Percentile(w.interval_ms, 1.0), w.slow, idle, waits, avg(Cat::VoWait),
             static_cast<double>(w.vo_checks) / n, per(Cat::VoWait),
             w.tail_frames ? ms(w.tail) / w.tail_frames : 0.0, avg(Cat::RegWait), avg(Cat::CeWait),
             avg(Cat::HostWait), per(Cat::HostWait), avg(Cat::FrameWait), work,
             Percentile(w.work_ms, 0.5), Percentile(w.work_ms, 0.9), Percentile(w.work_ms, 1.0),
             budget_ms, w.over);

    LOG_INFO(
        Render,
        "GpuProf work | other {:.2f} draw {:.2f} disp {:.2f} pipe {:.2f} tgt {:.2f} buf {:.2f} "
        "tex {:.2f} vtx {:.2f} desc {:.2f} bufup {:.2f} imgup {:.2f} dma {:.2f} fence {:.2f} "
        "submit {:.2f} flip {:.2f} cmds {:.2f} rb {:.2f} | per frame: draws {:.0f} disp "
        "{:.0f} bufup {:.0f} imgup {:.1f} submits {:.1f} rb {:.1f} cmds {:.1f} | prof ~{:.2f}",
        avg(Cat::Other), avg(Cat::Draw), avg(Cat::Dispatch), avg(Cat::Pipeline), avg(Cat::Targets),
        avg(Cat::Buffers), avg(Cat::Textures), avg(Cat::VertexIndex), avg(Cat::Descriptors),
        avg(Cat::BufferUpload), avg(Cat::ImageUpload), avg(Cat::Dma), avg(Cat::Fence),
        avg(Cat::Submit), avg(Cat::Flip), avg(Cat::Commands), avg(Cat::Readback), per(Cat::Draw),
        per(Cat::Dispatch), per(Cat::BufferUpload), per(Cat::ImageUpload), per(Cat::Submit),
        per(Cat::Readback), per(Cat::Commands), prof);

    LOG_INFO(
        Render,
        "GpuProf game | gpu-idle wait {:.2f} ({:.1f}/f, max {:.2f}) pacing {:.2f} (max {:.2f}) "
        "readback {:.2f} ({:.1f}/f, max {:.2f}) | present {:.2f} (max {:.2f})",
        g_avg(Global::GpuIdle), g_per(Global::GpuIdle), g_max[Idx(Global::GpuIdle)],
        g_avg(Global::Pacing), g_max[Idx(Global::Pacing)], g_avg(Global::ReadbackStall),
        g_per(Global::ReadbackStall), g_max[Idx(Global::ReadbackStall)], g_avg(Global::Present),
        g_max[Idx(Global::Present)]);

    // Where the slowest frame of the window went, largest first.
    std::vector<std::pair<double, u32>> parts;
    for (u32 i = 0; i < kNumCats; ++i) {
        const double v = ms(w.worst_self[i]);
        if (static_cast<Cat>(i) != Cat::Idle && v >= 0.05) {
            parts.emplace_back(v, i);
        }
    }
    std::sort(parts.begin(), parts.end(), [](const auto& a, const auto& b) { return a > b; });
    std::string line;
    for (const auto& [v, i] : parts) {
        fmt::format_to(std::back_inserter(line), " {} {:.2f}", kNames[i], v);
    }
    LOG_INFO(Render, "GpuProf worst | work {:.2f} of {:.2f} (idle {:.2f}) |{}", w.worst_work,
             w.worst_interval, ms(w.worst_self[Idx(Cat::Idle)]), line);
}

} // namespace

void OnFlip(u32 vblank_hz) {
    static thread_local bool calibrated = false;
    static thread_local double ms_per_tick = 0.0;
    static thread_local double scope_cost_ms = 0.0;
    static thread_local Snapshot last{};
    static thread_local Window w{};

    auto& s = t_state;
    if (!calibrated) {
        calibrated = true;
        const u64 freq = Libraries::Kernel::Dev::GetClock()->GetTscFrequency();
        ms_per_tick = freq != 0 ? 1000.0 / static_cast<double>(freq) : 0.0;
        // Cost of one timed scope, to estimate the profiler's own overhead.
        constexpr u32 kSamples = 10000;
        const u64 begin = Now();
        for (u32 i = 0; i < kSamples; ++i) {
            Scope calibrate{Cat::Calibrate};
        }
        scope_cost_ms = static_cast<double>(Now() - begin) * ms_per_tick / kSamples;
        LOG_INFO(Render, "GpuProf: TSC {} MHz, {:.1f} ns per scope, logging every {} flips",
                 freq / 1000000, scope_cost_ms * 1e6, kWindowFrames);
    }

    const u64 now = Now();
    Charge(s, now);

    Snapshot cur{};
    cur.self = s.self;
    cur.calls = s.calls;
    for (u32 i = 0; i < kNumGlobals; ++i) {
        cur.global_ticks[i] = g_counters[i].ticks.load(std::memory_order_relaxed);
        cur.global_calls[i] = g_counters[i].calls.load(std::memory_order_relaxed);
    }
    cur.transitions = s.transitions;
    cur.vo_checks = s.vo_checks;
    cur.time = now;

    if (last.time != 0 && ms_per_tick != 0.0) {
        const auto ms = [&](u64 ticks) { return static_cast<double>(ticks) * ms_per_tick; };
        const double budget_ms = 1000.0 / std::max<u32>(vblank_hz, 1);

        std::array<u64, kNumCats> frame{};
        u64 waits = 0;
        for (u32 i = 0; i < kNumCats; ++i) {
            frame[i] = cur.self[i] - last.self[i];
            w.self[i] += frame[i];
            w.calls[i] += cur.calls[i] - last.calls[i];
            if (IsWait(static_cast<Cat>(i))) {
                waits += frame[i];
            }
        }
        for (u32 i = 0; i < kNumGlobals; ++i) {
            w.global_ticks[i] += cur.global_ticks[i] - last.global_ticks[i];
            w.global_calls[i] += cur.global_calls[i] - last.global_calls[i];
        }
        w.transitions += cur.transitions - last.transitions;
        w.vo_checks += cur.vo_checks - last.vo_checks;

        const u64 interval = now - last.time;
        const u64 idle = frame[Idx(Cat::Idle)];
        const u64 work = interval > idle + waits ? interval - idle - waits : 0;
        w.interval += interval;
        if (s.last_vo_check > last.time) {
            w.tail += now - s.last_vo_check;
            ++w.tail_frames;
        }

        const double interval_ms = ms(interval);
        const double work_ms = ms(work);
        w.interval_ms.push_back(interval_ms);
        w.work_ms.push_back(work_ms);
        if (interval_ms > budget_ms * 1.5) {
            ++w.slow;
        }
        if (work_ms > budget_ms) {
            ++w.over;
        }
        if (work_ms > w.worst_work) {
            w.worst_work = work_ms;
            w.worst_interval = interval_ms;
            w.worst_self = frame;
        }

        if (++w.frames >= kWindowFrames) {
            LogWindow(w, ms_per_tick, scope_cost_ms, budget_ms);
            w = Window{};
        }
    }
    last = cur;
}

} // namespace VideoCore::Prof
