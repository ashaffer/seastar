/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright 2017 ScyllaDB
 */
#include <seastar/util/backtrace.hh>

#include <link.h>
#include <sys/types.h>
#include <unistd.h>
#include <format>

#include <errno.h>
#include <string.h>

#include <seastar/core/print.hh>
#include <atomic>


namespace seastar {

static int dl_iterate_phdr_callback(struct dl_phdr_info *info, size_t size, void *data)
{
    std::size_t total_size{0};
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const auto hdr = info->dlpi_phdr[i];

        // Only account loadable, executable (text) segments
        if (hdr.p_type == PT_LOAD && (hdr.p_flags & PF_X) == PF_X) {
            total_size += hdr.p_memsz;
        }
    }

    reinterpret_cast<std::vector<shared_object>*>(data)->push_back({info->dlpi_name, info->dlpi_addr, info->dlpi_addr + total_size});

    return 0;
}

static std::vector<shared_object> enumerate_shared_objects() {
    std::vector<shared_object> shared_objects;
    dl_iterate_phdr(dl_iterate_phdr_callback, &shared_objects);

    return shared_objects;
}

static const std::vector<shared_object> shared_objects{enumerate_shared_objects()};
static const shared_object uknown_shared_object{"", 0, std::numeric_limits<uintptr_t>::max()};
// Flipped BEFORE ~shared_objects runs (defined after it in this TU => destroyed first): a
// shard that keeps logging while another shard is inside exit() must not walk the freed
// vector. Keep these two definitions adjacent to shared_objects.
static std::atomic<bool> g_so_alive{true};
static struct so_liveness_guard { ~so_liveness_guard() { g_so_alive.store(false, std::memory_order_release); } } g_so_guard;
static std::atomic<uint64_t> g_bt_rejected_frames{0};   // counter only; surfaced by the app's 5-min report

uint64_t backtrace_rejected_frames() noexcept { return g_bt_rejected_frames.load(std::memory_order_relaxed); }

// A frame's shared_object pointer must point INTO the static vector's storage (or be
// the sentinel). Anything else is a dangling/garbage pointer: print the raw address.
static bool frame_so_is_sane(const frame& f) noexcept {
    if (f.so == &uknown_shared_object) { return true; }
    if (!g_so_alive.load(std::memory_order_acquire) || shared_objects.empty()) { return false; }
    const shared_object* b = shared_objects.data();
    const shared_object* e = b + shared_objects.size();
    if (f.so < b || f.so >= e) { return false; }
    return (reinterpret_cast<uintptr_t>(f.so) - reinterpret_cast<uintptr_t>(b)) % sizeof(shared_object) == 0;
}

bool operator==(const frame& a, const frame& b) {
    return a.so == b.so && a.addr == b.addr;
}

frame decorate(uintptr_t addr) {
    // If the shared-objects are not enumerated yet, or the enumeration
    // failed return the addr as-is with a dummy shared-object.
    if (!g_so_alive.load(std::memory_order_acquire) || shared_objects.empty()) {
        return {&uknown_shared_object, addr};
    }

    auto it = std::find_if(shared_objects.begin(), shared_objects.end(), [&] (const shared_object& so) {
        return addr >= so.begin && addr < so.end;
    });

    // Unidentified addresses are assumed to originate from the executable.
    auto& so = it == shared_objects.end() ? shared_objects.front() : *it;
    return {&so, addr - so.begin};
}

saved_backtrace current_backtrace() noexcept {
    // Captured in place (no temporary vector): together with static_vector's element-wise
    // move this is what makes the returned frames outlive this call. The two
    // backtrace_symbols_fd dumps that used to live here were fed `frame` structs as if
    // they were void*[] -- garbage, and two blocking writes per report on the reactor.
    saved_backtrace sb;
    back_trace([&] (frame f) {
        if (sb._frames.size() < sb._frames.capacity()) {
            sb._frames.emplace_back(f);
        }
    });
    return sb;
}

size_t saved_backtrace::hash() const {
    size_t h = 0;
    for (const auto& f : _frames) {
        h = ((h << 5) - h) ^ ((frame_so_is_sane(f) ? f.so->begin : 0) + f.addr);
    }
    return h;
}

std::ostream& operator<<(std::ostream& out, const saved_backtrace& b) {
    for (const auto& f : b._frames) {
        out << "  ";
        if (!frame_so_is_sane(f)) {
            g_bt_rejected_frames.fetch_add(1, std::memory_order_relaxed);
            out << std::format("?+0x{:x}\n", f.addr);          // raw; never touch f.so
            continue;
        }
        if (!f.so->name.empty()) { out << f.so->name << "+"; }
        out << std::format("0x{:x}\n", f.addr);                 // executable frames (empty name) now printed too
    }
    return out;
}


} // namespace seastar

