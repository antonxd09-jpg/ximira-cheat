#pragma once

namespace xim::features::chams::bridge {

// обход текущего world::snapshot, чтение jgi → CharacterBindings →
// {character,arms,gloves,ragdoll} Renderer*, регистрация targets в chams.
// вызывается из frame_callback каждый кадр ПЕРЕД chams::update().
void feed();

// сброс кэша (при выходе из матча, смене карты)
void reset();

// диагностика — сколько целей зарегистрировано в прошлом кадре
int  last_fed_count();

} // namespace xim::features::chams::bridge