// (HookShot-PerformanceTesting, 2026-11-09) RX_VK_HITCH_STACKS=<ms>: a submit's or present's recording that runs longer than
// that has its thread's call stack sampled (every 10 ms after, up to 4 times) and logged as module+offset, for
// tools/hitch_symbols.py to resolve against hookshot_vk.map. Hyrule Castle Town's ~60 ms hitches land in a different
// replay phase each time (descriptors, commands, uploads, dispatch): something holds the thread, and the phase timers
// cannot say what. While the thread is suspended only its context is read and unwound into a fixed array (no allocation,
// no lock the thread may hold); the names and the log come after it is resumed.
#include <windows.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "vk_internal.h"

namespace mkwvk {
namespace {

std::atomic<HANDLE> watched{nullptr};      // (the thread in a watched recording, or null)
std::atomic<int64_t> began_ns{0};
std::atomic<const char*> watched_what{nullptr};
std::atomic<uint64_t> generation{0};

int threshold_ms() {
    static const int ms = getenv("RX_VK_HITCH_STACKS") ? std::max(5, atoi(getenv("RX_VK_HITCH_STACKS"))) : 0;
    return ms;
}

int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

constexpr int kFrames = 28;

int unwind(CONTEXT ctx, DWORD64* pcs) {   // (x64: the unwind tables every function has)
    int n = 0;
    while (n < kFrames && ctx.Rip) {
        pcs[n++] = ctx.Rip;
        DWORD64 base = 0;
        PRUNTIME_FUNCTION f = RtlLookupFunctionEntry(ctx.Rip, &base, nullptr);
        if (!f) {   // (a leaf function: its return address is on top of the stack)
            ctx.Rip = *(DWORD64*)ctx.Rsp;
            ctx.Rsp += 8;
        } else {
            void* handler = nullptr;
            DWORD64 frame = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx.Rip, f, &ctx, &handler, &frame, nullptr);
        }
    }
    return n;
}

void log_stack(const char* what, int64_t ms, int sample, const DWORD64* pcs, int n) {
    char line[2048];
    int k = snprintf(line, sizeof line, "vk: hitch: %s at %lld ms (sample %d):", what ? what : "?", (long long)ms, sample);
    for (int i = 0; i < n && k < (int)sizeof line - 80; i++) {
        HMODULE m = nullptr;
        char name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)pcs[i], &m) && m) {
            char full[MAX_PATH];
            if (GetModuleFileNameA(m, full, sizeof full)) {
                const char* b = full;
                for (const char* p = full; *p; p++)
                    if (*p == '\\' || *p == '/') b = p + 1;
                snprintf(name, sizeof name, "%s", b);
            }
        }
        k += snprintf(line + k, sizeof line - k, " %s+%#llx", name, (unsigned long long)(pcs[i] - (DWORD64)m));
    }
    logf("%s", line);
}

void watchdog() {
    uint64_t seen_gen = 0;
    int samples = 0;
    int64_t last_ms = 0;
    for (;;) {
        Sleep(2);
        HANDLE h = watched.load();
        if (!h) continue;
        const uint64_t g = generation.load();
        if (g != seen_gen) seen_gen = g, samples = 0, last_ms = 0;
        const int64_t ms = (now_ns() - began_ns.load()) / 1000000;
        if (ms < threshold_ms() || samples >= 4 || (samples && ms - last_ms < 10)) continue;
        DWORD64 pcs[kFrames];
        int n = 0;
        if (SuspendThread(h) != (DWORD)-1) {
            if (generation.load() == g && watched.load() == h) {   // (still in the same recording)
                CONTEXT ctx{};
                ctx.ContextFlags = CONTEXT_FULL;
                if (GetThreadContext(h, &ctx)) n = unwind(ctx, pcs);
            }
            ResumeThread(h);
        }
        if (!n) continue;
        samples++, last_ms = ms;
        static int logged;
        if (logged++ < 400) log_stack(watched_what.load(), ms, samples, pcs, n);
    }
}

}  // namespace

void hitch_begin(const char* what) {
    if (!threshold_ms()) return;
    static const bool started = [] {
        std::thread(watchdog).detach();
        logf("vk: hitch stacks: recordings over %d ms are sampled (RX_VK_HITCH_STACKS)", threshold_ms());
        return true;
    }();
    (void)started;
    thread_local HANDLE self = [] {
        HANDLE h = nullptr;
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &h,
                        THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0);
        return h;
    }();
    watched_what = what;
    began_ns = now_ns();
    generation++;
    watched = self;
}

void hitch_end() {
    if (!threshold_ms()) return;
    watched = nullptr;
}

}  // namespace mkwvk
