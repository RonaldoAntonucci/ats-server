# Lua Named Callback Consumption
> Each native callback registration consumes its named global

Entry: `src/lua/scripts/luascript.cpp:LuaScriptInterface::getEvent()`

Flow: lookup named global → store function in event table → clear named global → return event ID

Impact:
- Reusing one callback name across multiple `Combat:setCallback()` calls leaves every registration after the first without a function unless the global is republished.
- Keep the formula as one local function reference; publish it under the expected name immediately before each registration.
- Test doubles must capture `_G[name]` and clear it during `setCallback`, matching the native boundary.

Production example: `data/scripts/spells/attack/armamento_assault.lua:createCombat()`

Regression harnesses: `tests/integration/game/armamento_assault_it.cpp`, `tests/lua/test_armamento_assault.lua`

Updated: 2026-08-24
