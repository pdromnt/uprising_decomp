# Keep SDL's existing parser and wired-proxy filtering; permit only the known
# Xbox Series Bluetooth endpoint, as validated in TOU.
function(uprising_patch_sdl_xbox_ble source_dir)
    set(hid_file "${source_dir}/src/joystick/hidapi/SDL_hidapi_xboxone.c")
    file(READ "${hid_file}" content)
    set(anchor [=[    if (device && SDL_strncmp(device->path, "\\\\?\\HID#", 8) == 0) {]=])
    set(replacement [=[    /* Uprising: Series BLE is real Bluetooth HID, not a wired XGIP proxy. */
    if (device && SDL_strncmp(device->path, "\\\\?\\HID#", 8) == 0 &&
        !(vendor_id == 0x045e && product_id == 0x0b13 && device->is_bluetooth)) {]=])
    string(FIND "${content}" "${replacement}" patched)
    if(patched EQUAL -1)
        string(FIND "${content}" "${anchor}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR "Pinned SDL Xbox HID endpoint filter changed")
        endif()
        string(REPLACE "${anchor}" "${replacement}" content "${content}")
        file(WRITE "${hid_file}" "${content}")
    endif()
endfunction()
