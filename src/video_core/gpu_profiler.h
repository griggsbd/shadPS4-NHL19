// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// GPU command processor profiler (diagnostic build only).
//
// Every tick of a thread's time is charged to exactly one category: the innermost open Scope, or
// the thread's current state when no scope is open. The GPU thread's totals are logged as
// milliseconds per frame once every kWindowFrames flips, so the categories add up to the frame
// time.
//
// GPU queues are coroutines, so no Scope may stay open across a yield. A queue that yields
// because it is waiting calls Block() first and Unblock() once it can run again. While no queue
// can run, the thread's time goes to the wait of the most important blocked queue.

#include <array>
#include <atomic>

#include "common/arch.h"
#include "common/rdtsc.h"
#include "common/types.h"

#ifdef ARCH_X86_64
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif

namespace VideoCore::Prof {

enum class Cat : u32 {
    Other,        // PM4 parsing, register writes, events and loop overhead
    Idle,         // waiting for the game to submit work
    VoWait,       // waiting until the presenter releases a display buffer
    RegWait,      // WAIT_REG_MEM on other labels, semaphores, rewinds
    CeWait,       // constant engine and draw engine counter sync
    HostWait,     // waiting for the host GPU to reach a timeline point
    FrameWait,    // waiting for a free presentation frame
    Draw,         // Draw / DrawIndirect, the part not covered below
    Dispatch,     // DispatchDirect / DispatchIndirect, the part not covered below
    Pipeline,     // pipeline lookup: key refresh, hash lookup, compiles
    Targets,      // render target and depth lookups (PrepareRenderState + BeginRendering)
    Buffers,      // BindBuffers and DMA buffer sync
    Textures,     // BindTextures
    VertexIndex,  // vertex and index buffer binding
    Descriptors,  // descriptor writes (pipeline->BindResources)
    BufferUpload, // BufferCache::SynchronizeMemory
    ImageUpload,  // TextureCache::RefreshImage
    Dma,          // CopyBuffer / FillBuffer
    Fence,        // OnFence / OnSubmit housekeeping
    Submit,       // Scheduler::SubmitExecution
    Flip,         // flip preparation (PrepareFrame)
    Commands,     // commands sent to the GPU thread by other threads
    Readback,     // downloads of GPU written memory
    Calibrate,    // used once to measure the profiler's own cost
    Count
};

// Queues that can block, from the most to the least important.
enum class Queue : u32 { Gfx, Asc, Ce, Count };

// Waits on other threads, totalled across all of them.
enum class Global : u32 {
    GpuIdle,       // game waiting for the GPU thread before a submit
    Pacing,        // game held until the previous flip is on screen
    ReadbackStall, // game waiting for a readback
    Present,       // present thread presenting a game frame
    Count
};

constexpr u32 kNumCats = static_cast<u32>(Cat::Count);
constexpr u32 kNumQueues = static_cast<u32>(Queue::Count);
constexpr u32 kNumGlobals = static_cast<u32>(Global::Count);
constexpr u32 kMaxDepth = 32;

struct ThreadState {
    std::array<u64, kNumCats> self{};
    std::array<u64, kNumCats> calls{};
    std::array<Cat, kMaxDepth> stack{};
    std::array<Cat, kNumQueues> blocked{};
    u32 depth = 0;
    Cat state = Cat::Other;
    u64 last = 0;
    u64 transitions = 0;
    u64 vo_checks = 0;
    u64 last_vo_check = 0;
};

inline thread_local ThreadState t_state{};

struct GlobalCounter {
    std::atomic<u64> ticks{0};
    std::atomic<u64> calls{0};
    std::atomic<u64> max{0};
};

inline std::array<GlobalCounter, kNumGlobals> g_counters{};

// Unfenced on x86: half the cost of a fenced read, and the regions timed here are long enough
// that reordering around the read does not matter.
inline u64 Now() {
#ifdef ARCH_X86_64
    return __rdtsc();
#else
    return Common::FencedRDTSC();
#endif
}

// Charges the time since the last transition to whatever the thread was doing.
inline void Charge(ThreadState& s, u64 now) {
    if (s.last != 0) {
        const Cat current =
            s.depth == 0 ? s.state : s.stack[(s.depth < kMaxDepth ? s.depth : kMaxDepth) - 1];
        s.self[static_cast<u32>(current)] += now - s.last;
    }
    s.last = now;
    ++s.transitions;
}

class Scope {
public:
    explicit Scope(Cat cat_) : cat{cat_} {
        auto& s = t_state;
        Charge(s, Now());
        if (s.depth < kMaxDepth) {
            s.stack[s.depth] = cat;
        }
        ++s.depth;
    }
    ~Scope() {
        auto& s = t_state;
        Charge(s, Now());
        --s.depth;
        ++s.calls[static_cast<u32>(cat)];
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    Cat cat;
};

inline void SetState(ThreadState& s, Cat state) {
    if (s.state != state) {
        Charge(s, Now());
        s.state = state;
    }
}

// A queue is about to yield because it is waiting on `wait`.
inline void Block(Queue queue, Cat wait) {
    auto& s = t_state;
    s.blocked[static_cast<u32>(queue)] = wait;
    Cat state = Cat::Other;
    for (const Cat c : s.blocked) {
        if (c != Cat::Other) {
            state = c;
            break;
        }
    }
    SetState(s, state);
}

// A queue is running again, so the thread is doing work.
inline void Unblock(Queue queue) {
    auto& s = t_state;
    s.blocked[static_cast<u32>(queue)] = Cat::Other;
    SetState(s, Cat::Other);
}

// A display buffer label check finished, waited on or not.
inline void MarkVoCheck() {
    auto& s = t_state;
    ++s.vo_checks;
    s.last_vo_check = Now();
}

class GlobalScope {
public:
    explicit GlobalScope(Global global_) : global{global_}, start{Now()} {}
    ~GlobalScope() {
        const u64 ticks = Now() - start;
        auto& c = g_counters[static_cast<u32>(global)];
        c.ticks.fetch_add(ticks, std::memory_order_relaxed);
        c.calls.fetch_add(1, std::memory_order_relaxed);
        u64 prev = c.max.load(std::memory_order_relaxed);
        while (ticks > prev &&
               !c.max.compare_exchange_weak(prev, ticks, std::memory_order_relaxed)) {
        }
    }
    GlobalScope(const GlobalScope&) = delete;
    GlobalScope& operator=(const GlobalScope&) = delete;

private:
    Global global;
    u64 start;
};

// Called on the GPU thread whenever it reaches a flip. Logs once every kWindowFrames flips.
void OnFlip(u32 vblank_hz);

} // namespace VideoCore::Prof
