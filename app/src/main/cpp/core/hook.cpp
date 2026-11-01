#include "core/hook.h"
#include "util/log.h"
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <map>

namespace xim::hook {

static std::map<void*, uint8_t*> g_trampolines;

static bool make_rwx(void* addr, size_t len) {
    uintptr_t page = (uintptr_t)addr & ~(uintptr_t)(getpagesize()-1);
    return mprotect((void*)page, len + ((uintptr_t)addr - page), PROT_READ|PROT_WRITE|PROT_EXEC) == 0;
}

// ARM64: LDR x16, #8; BR x16; [8-byte addr] = 16 bytes
static void write_abs_jump(void* dst, void* target) {
    uint32_t tramp[4];
    tramp[0] = 0x58000050; // LDR x16, #8
    tramp[1] = 0xD61F0200; // BR x16
    uint64_t t = (uint64_t)target;
    memcpy(&tramp[2], &t, 8);
    memcpy(dst, tramp, 16);
    __builtin___clear_cache((char*)dst, (char*)dst + 16);
}

bool install(void* func_addr, void* new_func, void** orig_func) {
    if (!func_addr || !new_func) return false;
    if (!make_rwx(func_addr, 32)) { XERR("hook: mprotect failed"); return false; }
    // save original 16 bytes + jump back
    uint8_t* trampoline = (uint8_t*)mmap(nullptr, 32,
        PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (trampoline == MAP_FAILED) return false;
    memcpy(trampoline, func_addr, 16);
    write_abs_jump(trampoline + 16, (uint8_t*)func_addr + 16);
    if (orig_func) *orig_func = trampoline;
    write_abs_jump(func_addr, new_func);
    g_trampolines[func_addr] = trampoline;
    XLOG("hook: installed %p -> %p", func_addr, new_func);
    return true;
}

bool remove(void* func_addr) {
    auto it = g_trampolines.find(func_addr);
    if (it == g_trampolines.end()) return false;
    if (!make_rwx(func_addr, 16)) return false;
    memcpy(func_addr, it->second, 16);
    __builtin___clear_cache((char*)func_addr, (char*)func_addr + 16);
    munmap(it->second, 32);
    g_trampolines.erase(it);
    return true;
}

} // namespace xim::hook
