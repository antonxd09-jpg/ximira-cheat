#pragma once

#include <string>
#include <vector>

namespace xim::features::config {

// имя текущего профиля (в UI)
const std::string& current_profile();
void set_current_profile(const std::string& name);

// список профилей, найденных в /storage/emulated/0/Ximira/Configs/*.json
std::vector<std::string> list_profiles();
void refresh_profiles();

// сохранить все настройки (chams, glow, world_mod, camera_mod, postprocess,
// hitsound, esp) в /storage/emulated/0/Ximira/Configs/<name>.json
bool save(const std::string& name);

// загрузить профиль
bool load(const std::string& name);

// удалить профиль
bool remove(const std::string& name);

// экспорт в clipboard (json-строка) → в Android clipboard
bool export_to_clipboard();

// импорт из clipboard
bool import_from_clipboard();

// сохранить последний использованный профиль (autoload при старте)
bool save_last_used();
bool load_last_used();

// инициализация: mkdir -p папки, refresh_profiles()
bool init();

} // namespace xim::features::config