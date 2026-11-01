#include "core/hook.h"
#include "util/log.h"

#include "shadowhook.h"

namespace xim::hook {

bool install(uintptr_t target, void* replace, void** original) {
    if (!target || !replace) return false;

    void* orig = nullptr;
    int rc = shadowhook_hook_func_addr(reinterpret_cast<void*>(target), replace, &orig);

    if (rc != 0) {
        XERR("DobbyHook failed at %p rc=%d", reinterpret_cast<void*>(target), rc);
        return false;
    }

    if (original) *original = orig;
    XLOG("hook ok %p -> %p (orig %p)",
         reinterpret_cast<void*>(target), replace, orig);
    return true;
}

bool uninstall(uintptr_t target) {
    if (!target) return false;
    return shadowhook_unhook(reinterpret_cast<void*>(target)) == 0;
}

} // namespace xim::hook