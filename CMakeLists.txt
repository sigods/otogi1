cmake_minimum_required(VERSION 3.20)

# ============================================================================
# Otogi: Myth of Demons - Static Recompilation
#
# The xboxrecomp toolkit is expected alongside this project (default
# ../xboxrecomp). Generated code lands in src/game/recomp/gen via regen.sh.
# ============================================================================

project(otogi1 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── xboxrecomp toolkit ──────────────────────────────────────────

set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/xboxrecomp" CACHE PATH
    "Path to the xboxrecomp toolkit root directory (vendored by default)")

# Builds the runtime libraries and provides the 'xboxrecomp' umbrella target.
add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

# ── Recompiled game executable ──────────────────────────────────

# The recomp pipeline writes gen_*.c files here. The directory does not exist
# until regen.sh has run; an empty glob is intentional at that stage.
# CONFIGURE_DEPENDS re-runs the glob when files are added or removed.
file(GLOB GAME_RECOMP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/recomp/gen/*.c")

add_executable(otogi1
    src/game/main.c
    src/game/recomp_manual.c
    ${GAME_RECOMP_SOURCES}
)

target_include_directories(otogi1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game/recomp
    ${XBOXRECOMP_DIR}/include
)

target_link_libraries(otogi1 PRIVATE xboxrecomp)

# ── Platform specifics ──────────────────────────────────────────

if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)

    target_link_libraries(otogi1 PRIVATE
        d3d11
        dxgi
        dxguid
        winmm
        dbghelp
        $<IF:$<BOOL:${MINGW}>,xinput1_4,xinput>
    )

    if(MSVC)
        # Generated chunks can be very large; keep the section limit high.
        target_link_options(otogi1 PRIVATE /LARGEADDRESSAWARE)
        target_compile_options(otogi1 PRIVATE /bigobj)
    endif()
else()
    # The toolkit's platform target already links Threads::Threads and libm.
endif()

set_target_properties(otogi1 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
