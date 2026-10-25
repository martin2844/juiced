-- Edit these functions and restart the game. No compilation or native library needed.
local names = {"GLOW", "PULSATING", "BACK AND FORTH", "RGB CYCLE"}
return {
  api = 1,
  feature = function(name) if name == "neon" then return true end end,
  effect_count = function() return #names end,
  effect_name = function(effect) return names[effect + 1] or names[1] end,
  neon_colour = function(rgb, effect, speed, seconds)
    local phase = seconds * 0.25 * speed
    local gain = effect == 1 and (0.15 + 0.85 * (0.5 + 0.5 * math.sin(phase * 2 * math.pi))) or 1
    local out = 0
    for i = 0, 2 do
      local shift = 16 - i * 8
      local value = (rgb >> shift) & 255
      if effect == 3 then value = 255 * (0.5 + 0.5 * math.cos((phase + i / 3) * 2 * math.pi)) end
      out = out | (math.floor(value * gain) << shift)
    end
    return out
  end,
  neon_texture = function(rgb, effect, speed, seconds)
    if effect == 2 then return "mod_neon_scan_" .. math.floor((seconds * 0.25 * speed % 1) * 32) end
    return "mod_neon_glow"
  end,
}
