#pragma once

#include <cstdint>

namespace xim::world::reg {

bool  init();

void* ti_client_world();
void* ti_registry();
void* ti_store();
void* ti_player();
void* ti_netobject();
void* ti_model();
void* ti_stats();
void* ti_visibility();
void* ti_weapon_part();
void* ti_weapon_instance();
void* ti_camera_comp();
void* klass_player();

// собрать массив Player (ake) — путь njh -> gat -> gzx -> yjk<krm> -> parts
int collect_players(void** out, int max);

// получить компонент-модель jgi от net-object
uintptr_t model_from_net(uintptr_t net);

// получить компонент-статы ylw от player
uintptr_t stats_from_player(uintptr_t player);

} // namespace xim::world::reg