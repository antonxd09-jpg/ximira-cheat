#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace xim::features::hitsound {

// один слот звука: файл + громкость + включён
struct Slot {
    std::atomic<bool>  enabled{false};
    std::atomic<float> volume{0.8f};            // 0 .. 1
    std::string        file;                    // абсолютный путь
    // медиа-плеер держим в .cpp, здесь только настройки.
};

struct Config {
    std::atomic<bool>  enabled{false};
    std::atomic<float> master_volume{0.9f};
    std::atomic<int>   cooldown_ms{50};

    // слоты: [0] hit, [1] headshot, [2] kill
    Slot slots[3];

    // UI-путь к папке со звуками (открывается кнопкой "Open")
    std::string folder = "/storage/emulated/0/Ximira/Sounds/";
};

Config& config();

// инициализация: создать папку если нет, зарезолвить JNI-методы NativeBridge.
bool init();

// пересканировать папку → список файлов (заполняет список для UI).
// вызывается из UI при открытии дропдауна + один раз в init.
void rescan_folder();

// текущий список файлов (пути). Используется UI-дропдауном.
const std::vector<std::string>& files();

// установить выбранный файл в слот (индекс 0..2) и пересоздать плеер.
void set_slot_file(int slot, const std::string& path);

// вызывается каждый кадр из frame_callback после snapshot.
// смотрит падение HP у игроков, проигрывает звук.
void update();

// отключить всё: стоп плееров, освобождение.
void shutdown();

// ручной вызов (для тестов/UI-превью)
void play_slot(int slot);

// сколько раз в этом матче сработал каждый слот (для отладки)
uint64_t play_count(int slot);

} // namespace xim::features::hitsound