# Optional backend only. SDK files are build inputs, never an end-user prerequisite.
set(UPRISING_VULKAN_SDK "$ENV{VULKAN_SDK}" CACHE PATH "macOS SDK root containing lib and share/vulkan")
find_file(kosmic_manifest libkosmickrisp_icd.json PATHS "${UPRISING_VULKAN_SDK}/share/vulkan/icd.d" NO_DEFAULT_PATH REQUIRED)
find_file(vulkan_loader libvulkan.1.dylib PATHS "${UPRISING_VULKAN_SDK}/lib" NO_DEFAULT_PATH REQUIRED)
file(READ "${kosmic_manifest}" manifest)
string(JSON driver_name GET "${manifest}" ICD library_path)
get_filename_component(driver_name "${driver_name}" NAME)
find_file(kosmic_driver "${driver_name}" PATHS "${UPRISING_VULKAN_SDK}/lib" NO_DEFAULT_PATH REQUIRED)
get_filename_component(vulkan_loader "${vulkan_loader}" REALPATH)
get_filename_component(kosmic_driver "${kosmic_driver}" REALPATH)
set_property(TARGET uprising PROPERTY INSTALL_RPATH "@executable_path/../Frameworks")
install(FILES "${vulkan_loader}" DESTINATION Uprising.app/Contents/Frameworks RENAME libvulkan.1.dylib COMPONENT Runtime)
install(FILES "${kosmic_driver}" DESTINATION Uprising.app/Contents/Frameworks RENAME "${driver_name}" COMPONENT Runtime)
file(READ "${CMAKE_ROOT}/Modules/MacOSXBundleInfo.plist.in" bundle_info)
string(REPLACE "<dict>" "<dict>\n\t<key>LSMinimumSystemVersion</key>\n\t<string>26.0</string>" bundle_info "${bundle_info}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/VulkanBundleInfo.plist.in" "${bundle_info}")
set_property(TARGET uprising PROPERTY MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/generated/VulkanBundleInfo.plist.in")
string(JSON manifest SET "${manifest}" ICD library_path "\"../../../Frameworks/${driver_name}\"")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/libkosmickrisp_icd.json" "${manifest}\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/libkosmickrisp_icd.json"
    DESTINATION Uprising.app/Contents/Resources/vulkan/icd.d COMPONENT Runtime)
# BundleUtilities follows otool dependencies, copies non-system libraries and
# rewrites their install names. Explicit roots include the dynamically loaded ICD.
install(CODE "
    include(BundleUtilities)
    set(app \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/Uprising.app\")
    fixup_bundle(\"\${app}\"
        \"\${app}/Contents/Frameworks/libvulkan.1.dylib;\${app}/Contents/Frameworks/${driver_name}\"
        \"${UPRISING_VULKAN_SDK}/lib\")
    verify_app(\"\${app}\")
" COMPONENT Runtime)
# SDK1.4.363.0 does not install these under macOS/. Retain the matching upstream
# license sets in source control, independently of installer layout/caches.
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/kosmickrisp-licenses"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan-loader-licenses"
    DESTINATION Uprising.app/Contents/Resources/Vulkan-Licenses COMPONENT Runtime)
