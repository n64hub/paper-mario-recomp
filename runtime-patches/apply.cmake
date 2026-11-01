# Patches lib/rt64, lib/N64ModernRuntime and lib/RecompFrontend: N64Recomp-browser's patches/ (its apply.cmake), then
# this game's own changes to them on top, runtime-patches/<project>-<n>-*.patch in order of name (README.md here).
# Doing it again changes nothing.
#
# The game's are taken out first and put back after: N64Recomp-browser's check that its patch is in, for a checkout it
# patched with a three-way merge, is that the checkout's changes are what they were when it went in, and the game's
# change them.
#
#   cmake -P runtime-patches/apply.cmake          from the repo's root (tools/build_n64recomp.sh)
#   include(runtime-patches/apply.cmake)          and ssb_apply_patches() (CMakeLists.txt)

set(SSB_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
include("${SSB_ROOT}/lib/N64Recomp-browser/patches/apply.cmake")

function(ssb_apply_patches)
    get_filename_component(root "${SSB_ROOT}" ABSOLUTE)
    file(GLOB patches "${root}/runtime-patches/*.patch")
    list(SORT patches)
    list(REVERSE patches)
    foreach (patch IN LISTS patches)
        get_filename_component(name "${patch}" NAME)
        string(REGEX REPLACE "-[0-9]+-.*" "" project "${name}")
        execute_process(COMMAND git apply --whitespace=nowarn --reverse "${patch}"
            WORKING_DIRECTORY "${root}/lib/${project}" RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    endforeach()
    list(REVERSE patches)

    n64recomp_browser_apply_patches("${root}/lib")

    foreach (patch IN LISTS patches)
        get_filename_component(name "${patch}" NAME)
        string(REGEX REPLACE "-[0-9]+-.*" "" project "${name}")
        execute_process(COMMAND git apply --whitespace=nowarn "${patch}"
            WORKING_DIRECTORY "${root}/lib/${project}" RESULT_VARIABLE result ERROR_VARIABLE error)
        if (NOT result EQUAL 0)
            message(FATAL_ERROR "${name} doesn't apply to lib/${project}:\n${error}")
        endif()
    endforeach()
endfunction()

if (CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    ssb_apply_patches()
endif()
