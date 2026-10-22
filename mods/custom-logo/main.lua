return {
  api = 1,
  feature = function(name) if name == "logo" then return true end end,
  logo_slot = function(model, count, existing) return existing >= 0 and existing or count end,
  image_menu = function(portrait, logo, onboarding)
    if onboarding then return {} end
    return {
      {label=logo and "CHANGE CUSTOM LOGO" or "IMPORT CUSTOM LOGO", help="Transparent PNG recommended. Apply CUSTOM LOGO under decals.", action="import_logo"},
      {label="REMOVE CUSTOM LOGO", help="Restore the original logo.", action="clear_logo"},
    }
  end,
  image_alias = function(texture, portrait, logo)
    if (texture == "ui_chooselogo_01" or texture == "mod_custom_logo") and logo ~= "" then return logo end
  end,
}
