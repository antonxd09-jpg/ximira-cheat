#pragma once
#include <cstdint>
namespace xim::hook {
    // hook func_addr, redirect to new_func, save original to orig_func
    bool install(void* func_addr, void* new_func, void** orig_func);
    bool remove(void* func_addr);
}
