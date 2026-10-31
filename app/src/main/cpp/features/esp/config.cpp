#include "features/esp/config.h"

namespace xim::features::esp {

static EspConfig g_cfg;
EspConfig& config() { return g_cfg; }

} // namespace xim::features::esp