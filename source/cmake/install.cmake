# Only the Runtime component is packaged; dependency installs are excluded.
if(APPLE)
    set(runtime_assets Uprising.app/Contents/Resources)
    install(TARGETS uprising BUNDLE DESTINATION . COMPONENT Runtime)
else()
    set(runtime_assets .)
    install(TARGETS uprising RUNTIME DESTINATION . COMPONENT Runtime)
endif()
set(asset_root "${CMAKE_CURRENT_SOURCE_DIR}/..")
install(DIRECTORY "${asset_root}/FONTS" "${asset_root}/GRIDS" "${asset_root}/Music" "${asset_root}/VIDEO"
    DESTINATION "${runtime_assets}" COMPONENT Runtime
    PATTERN "*.cfg" EXCLUDE PATTERN "*.log" EXCLUDE)
install(FILES "${asset_root}/uprising.cln" "${asset_root}/README.TXT" "${asset_root}/hints.txt"
    "${asset_root}/README.md" "${asset_root}/THIRD_PARTY_NOTICES.md"
    DESTINATION "${runtime_assets}" COMPONENT Runtime)
install(FILES third_party/libsmacker/COPYING DESTINATION "${runtime_assets}" RENAME libsmacker-COPYING.txt COMPONENT Runtime)
install(FILES third_party/SDL3-LICENSE.txt third_party/FreeType-FTL.txt third_party/stb-LICENSE.txt third_party/enet-LICENSE.txt
    DESTINATION "${runtime_assets}" COMPONENT Runtime)
install(FILES third_party/fsr/LICENSE.txt DESTINATION "${runtime_assets}" RENAME FSR-LICENSE.txt COMPONENT Runtime)
if(APPLE)
    install(FILES "${asset_root}/README.TXT" DESTINATION . COMPONENT Runtime)
endif()
