return {
  api = 1,
  feature = function(name) if name == "portrait" then return true end end,
  profile_prompt = function(alreadyAsked, hasPhoto) return not alreadyAsked and not hasPhoto end,
  image_menu = function(portrait, logo, onboarding)
    local rows = {{label=portrait and "CHANGE DRIVER PHOTO" or "CHOOSE DRIVER PHOTO", help="Choose a PNG or JPEG. Portraits are cropped to a square.", action="import_portrait"}}
    if not onboarding then rows[#rows+1]={label="REMOVE DRIVER PHOTO", help="Restore the original silhouette.", action="clear_portrait"} end
    return rows
  end,
  image_alias = function(texture, portrait, logo)
    if texture == "ui_crewmember_00" and portrait ~= "" then return portrait end
  end,
}
