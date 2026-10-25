-- Modern keyboard defaults. Saved bindings override these values.
local keys = {
    throttle = "DIKEYBOARD_W",
    brake = "DIKEYBOARD_S",
    left = "DIKEYBOARD_A",
    right = "DIKEYBOARD_D",
    nitro = "DIKEYBOARD_LSHIFT",
    handbrake = "DIKEYBOARD_SPACE",
    reverse = "DIKEYBOARD_R",
    gear_up = "DIKEYBOARD_E",
    gear_down = "DIKEYBOARD_Q",
    camera = "DIKEYBOARD_C",
    rear = "DIKEYBOARD_B",
    pan_left = "DIKEYBOARD_Z",
    pan_right = "DIKEYBOARD_X",
    horn = "DIKEYBOARD_H",
    crew1_up = "DIKEYBOARD_1",
    crew1_down = "DIKEYBOARD_2",
    crew2_up = "DIKEYBOARD_3",
    crew2_down = "DIKEYBOARD_4",
    crew3_up = "DIKEYBOARD_5",
    crew3_down = "DIKEYBOARD_6",
    next_car = "DIKEYBOARD_NEXT",
}
return {
    api = 1,
    paint_buy_key = function() return 48 end, -- ASCII 0
    paint_buy_label = function() return "[0] " end,
    feature = function(name) if name == "controls" then return true end end,
    control_default = function(action) return keys[action] end,
}
