#pragma once

#include <cstdint>
#include <functional>

namespace xim::hook {

// установка inline-хука через Dobby
// true — успех, trampoline записан в original
bool install(uintptr_t target, void* replace, void** original);

// шаблонная обёртка для удобства
template <typename T>
bool install(uintptr_t target, T replace, T* original) {
    void* orig = nullptr;
    bool ok = install(target, reinterpret_cast<void*>(replace),
                      &orig);
    if (original) *original = reinterpret_cast<T>(orig);
    return ok;
}

// снятие хука
bool uninstall(uintptr_t target);

} // namespace xim::hook