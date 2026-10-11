set(TABOS_TEST_PLATFORM_FAKES fakes/platform_display.c fakes/platform_storage.c)

# Test assertions must keep evaluating checks and setup calls in optimized builds.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-UNDEBUG>)
endif()

add_executable(tabos_core_smoke unit/core_smoke.c ${TABOS_TEST_PLATFORM_FAKES})
# Tests keep assertions enabled, but the linked runtime retains its build mode.
target_compile_definitions(tabos_core_smoke PRIVATE TABOS_TEST_RUNTIME_DIAGNOSTICS=$<CONFIG:Debug>)
target_include_directories(tabos_core_smoke PRIVATE
    "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/pointer/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_core_smoke PRIVATE tabos_core)
tabos_enable_warnings(tabos_core_smoke)
tabos_enable_sanitizers(tabos_core_smoke)

add_test(NAME unit.core_smoke COMMAND tabos_core_smoke)

add_executable(tabos_boot_report unit/boot_report.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_boot_report PRIVATE
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_boot_report PRIVATE tabos_core)
tabos_enable_warnings(tabos_boot_report)
tabos_enable_sanitizers(tabos_boot_report)
add_test(NAME unit.boot_report COMMAND tabos_boot_report)

add_executable(tabos_device_registry unit/device_registry.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_device_registry PRIVATE
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
target_link_libraries(tabos_device_registry PRIVATE tabos_core)
tabos_enable_warnings(tabos_device_registry)
tabos_enable_sanitizers(tabos_device_registry)
add_test(NAME unit.device_registry COMMAND tabos_device_registry)

add_executable(tabos_sdk_device
    unit/sdk_device.c
    "${TABOS_ROOT}/sdk/lib/device.c"
    "${TABOS_ROOT}/sdk/lib/wait.c"
)
target_include_directories(tabos_sdk_device PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_device)
tabos_enable_sanitizers(tabos_sdk_device)
add_test(NAME unit.sdk_device COMMAND tabos_sdk_device)

add_executable(tabos_sdk_posix_filesystem
    unit/sdk_posix_filesystem.c
    "${TABOS_ROOT}/sdk/lib/posix_filesystem.c"
)
target_compile_definitions(tabos_sdk_posix_filesystem PRIVATE TABOS_APPLICATION=1)
target_include_directories(tabos_sdk_posix_filesystem PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_posix_filesystem)
tabos_enable_sanitizers(tabos_sdk_posix_filesystem)
add_test(NAME unit.sdk_posix_filesystem COMMAND tabos_sdk_posix_filesystem)

add_executable(tabos_sdk_battery
    unit/sdk_battery.c
    "${TABOS_ROOT}/sdk/lib/battery.c"
)
target_include_directories(tabos_sdk_battery PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_battery)
tabos_enable_sanitizers(tabos_sdk_battery)
add_test(NAME unit.sdk_battery COMMAND tabos_sdk_battery)

add_executable(tabos_audio_service unit/audio_service.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_audio_service PRIVATE
    "${TABOS_ROOT}/audio/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_audio_service PRIVATE tabos_core)
tabos_enable_warnings(tabos_audio_service)
tabos_enable_sanitizers(tabos_audio_service)
add_test(NAME unit.audio_service COMMAND tabos_audio_service)

add_executable(tabos_sdk_audio
    unit/sdk_audio.c
    "${TABOS_ROOT}/sdk/lib/audio.c"
    "${TABOS_ROOT}/sdk/lib/wait.c"
)
target_include_directories(tabos_sdk_audio PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_audio)
tabos_enable_sanitizers(tabos_sdk_audio)
add_test(NAME unit.sdk_audio COMMAND tabos_sdk_audio)

add_executable(tabos_pointer_service unit/pointer_service.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_pointer_service PRIVATE
    "${TABOS_ROOT}/pointer/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_pointer_service PRIVATE tabos_core)
tabos_enable_warnings(tabos_pointer_service)
tabos_enable_sanitizers(tabos_pointer_service)
add_test(NAME unit.pointer_service COMMAND tabos_pointer_service)

add_executable(tabos_camera_service unit/camera_service.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_camera_service PRIVATE
    "${TABOS_ROOT}/camera/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_camera_service PRIVATE tabos_core)
tabos_enable_warnings(tabos_camera_service)
tabos_enable_sanitizers(tabos_camera_service)
add_test(NAME unit.camera_service COMMAND tabos_camera_service)

add_executable(tabos_camera_ccm unit/camera_ccm.c "${TABOS_ROOT}/platform/esp32p4/camera_ccm.c")
target_include_directories(tabos_camera_ccm PRIVATE "${TABOS_ROOT}/platform/esp32p4")
tabos_enable_warnings(tabos_camera_ccm)
tabos_enable_sanitizers(tabos_camera_ccm)
add_test(NAME unit.camera_ccm COMMAND tabos_camera_ccm)

add_executable(tabos_camera_backend unit/camera_backend.c
    "${TABOS_ROOT}/platform/esp32p4/camera_ccm.c")
target_include_directories(tabos_camera_backend PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/fakes/camera_backend"
    "${TABOS_ROOT}/platform/esp32p4"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${TABOS_GENERATED_INCLUDE_DIR}")
tabos_enable_warnings(tabos_camera_backend)
tabos_enable_sanitizers(tabos_camera_backend)
add_test(NAME unit.camera_backend COMMAND tabos_camera_backend)
set_tests_properties(unit.camera_backend PROPERTIES TIMEOUT 10)

add_executable(tabos_display_clear unit/display_clear.c)
target_include_directories(tabos_display_clear PRIVATE "${TABOS_ROOT}/platform/esp32p4" "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_display_clear)
tabos_enable_sanitizers(tabos_display_clear)
add_test(NAME unit.display_clear COMMAND tabos_display_clear)

add_executable(tabos_display_completion unit/display_completion.c
    "${TABOS_ROOT}/platform/esp32p4/display_completion.c")
target_include_directories(tabos_display_completion PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/fakes/display_completion"
    "${TABOS_ROOT}/platform/esp32p4"
    "${TABOS_GENERATED_INCLUDE_DIR}")
tabos_enable_warnings(tabos_display_completion)
tabos_enable_sanitizers(tabos_display_completion)
add_test(NAME unit.display_completion COMMAND tabos_display_completion)
set_tests_properties(unit.display_completion PROPERTIES TIMEOUT 10)

add_executable(tabos_keyboard_interrupt
    unit/keyboard_interrupt.c
    "${TABOS_ROOT}/platform/esp32p4/keyboard_interrupt.c"
)
target_include_directories(tabos_keyboard_interrupt PRIVATE "${TABOS_ROOT}/platform/esp32p4")
tabos_enable_warnings(tabos_keyboard_interrupt)
tabos_enable_sanitizers(tabos_keyboard_interrupt)
add_test(NAME unit.keyboard_interrupt COMMAND tabos_keyboard_interrupt)

add_executable(tabos_touch_interrupt
    unit/touch_interrupt.c
    "${TABOS_ROOT}/platform/esp32p4/touch_interrupt.c"
)
target_include_directories(tabos_touch_interrupt PRIVATE
    "${TABOS_ROOT}/platform/esp32p4"
    "${TABOS_ROOT}/sdk/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
tabos_enable_warnings(tabos_touch_interrupt)
tabos_enable_sanitizers(tabos_touch_interrupt)
add_test(NAME unit.touch_interrupt COMMAND tabos_touch_interrupt)

add_executable(tabos_sdk_camera
    unit/sdk_camera.c
    "${TABOS_ROOT}/sdk/lib/camera.c"
    "${TABOS_ROOT}/sdk/lib/wait.c"
)
target_include_directories(tabos_sdk_camera PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_camera)
tabos_enable_sanitizers(tabos_sdk_camera)
add_test(NAME unit.sdk_camera COMMAND tabos_sdk_camera)

add_executable(tabos_sdk_pointer
    unit/sdk_pointer.c
    "${TABOS_ROOT}/sdk/lib/pointer.c"
    "${TABOS_ROOT}/sdk/lib/wait.c"
)
target_include_directories(tabos_sdk_pointer PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_pointer)
tabos_enable_sanitizers(tabos_sdk_pointer)
add_test(NAME unit.sdk_pointer COMMAND tabos_sdk_pointer)

add_executable(tabos_host_pointer unit/host_pointer.c)
target_include_directories(tabos_host_pointer PRIVATE
    "${TABOS_ROOT}/platform/host/sdl"
    "${TABOS_ROOT}/pointer/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_host_pointer PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
tabos_enable_warnings(tabos_host_pointer)
tabos_enable_sanitizers(tabos_host_pointer)
add_test(NAME unit.host_pointer COMMAND tabos_host_pointer)

add_executable(tabos_host_camera_completion unit/host_camera_completion.c)
target_include_directories(tabos_host_camera_completion PRIVATE
    "${TABOS_ROOT}/camera/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
target_link_libraries(tabos_host_camera_completion PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
tabos_enable_warnings(tabos_host_camera_completion)
tabos_enable_sanitizers(tabos_host_camera_completion)
add_test(NAME unit.host_camera_completion COMMAND tabos_host_camera_completion)

add_executable(tabos_host_headless_audio unit/host_headless_audio.c)
target_include_directories(tabos_host_headless_audio PRIVATE
    "${TABOS_ROOT}/audio/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
target_link_libraries(tabos_host_headless_audio PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
tabos_enable_warnings(tabos_host_headless_audio)
tabos_enable_sanitizers(tabos_host_headless_audio)
add_test(NAME unit.host_headless_audio COMMAND tabos_host_headless_audio)

add_executable(tabos_raster unit/raster.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_raster PRIVATE
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
target_link_libraries(tabos_raster PRIVATE tabos_core)
tabos_enable_warnings(tabos_raster)
tabos_enable_sanitizers(tabos_raster)
add_test(NAME unit.raster COMMAND tabos_raster)

add_executable(tabos_sdk_graphics
    unit/sdk_graphics.c
    "${TABOS_ROOT}/sdk/lib/graphics.c"
)
target_include_directories(tabos_sdk_graphics PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_graphics)
tabos_enable_sanitizers(tabos_sdk_graphics)
add_test(NAME unit.sdk_graphics COMMAND tabos_sdk_graphics)

add_executable(tabos_sdk_input
    unit/sdk_input.c
    "${TABOS_ROOT}/sdk/lib/input.c"
)
target_include_directories(tabos_sdk_input PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_input)
tabos_enable_sanitizers(tabos_sdk_input)
add_test(NAME unit.sdk_input COMMAND tabos_sdk_input)

add_executable(tabos_sdk_reboot
    unit/sdk_reboot.c
    "${TABOS_ROOT}/sdk/lib/reboot.c"
)
target_include_directories(tabos_sdk_reboot PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_reboot)
tabos_enable_sanitizers(tabos_sdk_reboot)
add_test(NAME unit.sdk_reboot COMMAND tabos_sdk_reboot)

add_executable(tabos_sdk_network
    unit/sdk_network.c
    "${TABOS_ROOT}/sdk/lib/network.c"
    "${TABOS_ROOT}/sdk/lib/wait.c"
)
target_include_directories(tabos_sdk_network PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_network)
tabos_enable_sanitizers(tabos_sdk_network)
add_test(NAME unit.sdk_network COMMAND tabos_sdk_network)

add_executable(tabos_sdk_tls
    unit/sdk_tls.c
    "${TABOS_ROOT}/sdk/lib/tls.c"
)
target_include_directories(tabos_sdk_tls PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_tls)
tabos_enable_sanitizers(tabos_sdk_tls)
add_test(NAME unit.sdk_tls COMMAND tabos_sdk_tls)

add_executable(tabos_pool_table unit/pool_table.c "${TABOS_ROOT}/apps/pool/src/table.c")
target_include_directories(tabos_pool_table PRIVATE "${TABOS_ROOT}/apps/pool/include")
tabos_enable_warnings(tabos_pool_table)
tabos_enable_sanitizers(tabos_pool_table)
add_test(NAME unit.pool_table COMMAND tabos_pool_table)

add_executable(tabos_pool_game unit/pool_game.c
    "${TABOS_ROOT}/apps/pool/src/game.c" "${TABOS_ROOT}/apps/pool/src/rules.c" "${TABOS_ROOT}/apps/pool/src/physics.c"
    "${TABOS_ROOT}/apps/pool/src/clock.c" "${TABOS_ROOT}/apps/pool/src/input.c"
    "${TABOS_ROOT}/apps/pool/src/table.c")
target_include_directories(tabos_pool_game PRIVATE
    "${TABOS_ROOT}/apps/pool/include" "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_pool_game)
tabos_enable_sanitizers(tabos_pool_game)
add_test(NAME unit.pool_game COMMAND tabos_pool_game)

add_executable(tabos_pool_physics unit/pool_physics.c
    "${TABOS_ROOT}/apps/pool/src/game.c" "${TABOS_ROOT}/apps/pool/src/rules.c" "${TABOS_ROOT}/apps/pool/src/physics.c"
    "${TABOS_ROOT}/apps/pool/src/clock.c" "${TABOS_ROOT}/apps/pool/src/input.c"
    "${TABOS_ROOT}/apps/pool/src/table.c")
target_include_directories(tabos_pool_physics PRIVATE
    "${TABOS_ROOT}/apps/pool/include" "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_pool_physics)
tabos_enable_sanitizers(tabos_pool_physics)
add_test(NAME unit.pool_physics COMMAND tabos_pool_physics)

add_executable(tabos_pool_contacts unit/pool_contacts.c
    "${TABOS_ROOT}/apps/pool/src/game.c" "${TABOS_ROOT}/apps/pool/src/rules.c" "${TABOS_ROOT}/apps/pool/src/physics.c"
    "${TABOS_ROOT}/apps/pool/src/table.c")
target_include_directories(tabos_pool_contacts PRIVATE "${TABOS_ROOT}/apps/pool/include")
tabos_enable_warnings(tabos_pool_contacts)
tabos_enable_sanitizers(tabos_pool_contacts)
add_test(NAME unit.pool_contacts COMMAND tabos_pool_contacts)

add_executable(tabos_pool_pockets unit/pool_pockets.c
    "${TABOS_ROOT}/apps/pool/src/game.c" "${TABOS_ROOT}/apps/pool/src/rules.c" "${TABOS_ROOT}/apps/pool/src/physics.c"
    "${TABOS_ROOT}/apps/pool/src/input.c" "${TABOS_ROOT}/apps/pool/src/table.c")
target_include_directories(tabos_pool_pockets PRIVATE
    "${TABOS_ROOT}/apps/pool/include" "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_pool_pockets)
tabos_enable_sanitizers(tabos_pool_pockets)
add_test(NAME unit.pool_pockets COMMAND tabos_pool_pockets)

add_executable(tabos_pool_rules unit/pool_rules.c
    "${TABOS_ROOT}/apps/pool/src/game.c" "${TABOS_ROOT}/apps/pool/src/rules.c"
    "${TABOS_ROOT}/apps/pool/src/physics.c" "${TABOS_ROOT}/apps/pool/src/input.c"
    "${TABOS_ROOT}/apps/pool/src/table.c")
target_include_directories(tabos_pool_rules PRIVATE
    "${TABOS_ROOT}/apps/pool/include" "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_pool_rules)
tabos_enable_sanitizers(tabos_pool_rules)
add_test(NAME unit.pool_rules COMMAND tabos_pool_rules)

add_executable(tabos_pool_ai unit/pool_ai.c
    "${TABOS_ROOT}/apps/pool/src/game.c" "${TABOS_ROOT}/apps/pool/src/rules.c"
    "${TABOS_ROOT}/apps/pool/src/physics.c" "${TABOS_ROOT}/apps/pool/src/input.c"
    "${TABOS_ROOT}/apps/pool/src/table.c" "${TABOS_ROOT}/apps/pool/src/ai.c")
target_include_directories(tabos_pool_ai PRIVATE
    "${TABOS_ROOT}/apps/pool/include" "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_pool_ai)
tabos_enable_sanitizers(tabos_pool_ai)
add_test(NAME unit.pool_ai COMMAND tabos_pool_ai)

add_executable(tabos_pool_sound unit/pool_sound.c "${TABOS_ROOT}/apps/pool/src/sound.c")
target_include_directories(tabos_pool_sound PRIVATE
    "${TABOS_ROOT}/apps/pool/include" "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_pool_sound)
tabos_enable_sanitizers(tabos_pool_sound)
add_test(NAME unit.pool_sound COMMAND tabos_pool_sound)

add_executable(tabos_pool_render unit/pool_render.c
    "${TABOS_ROOT}/apps/pool/src/table.c" "${TABOS_ROOT}/apps/pool/src/render.c"
    "${TABOS_ROOT}/apps/pool/src/game.c" "${TABOS_ROOT}/apps/pool/src/rules.c" "${TABOS_ROOT}/apps/pool/src/physics.c"
    "${TABOS_ROOT}/apps/pool/src/clock.c"
    "${TABOS_ROOT}/sdk/lib/graphics.c")
target_include_directories(tabos_pool_render PRIVATE
    "${TABOS_ROOT}/apps/pool/include" "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_pool_render)
tabos_enable_sanitizers(tabos_pool_render)
add_test(NAME unit.pool_render COMMAND tabos_pool_render)

add_executable(tabos_soccer_game unit/soccer_game.c "${TABOS_ROOT}/apps/soccer/src/game.c"
    "${TABOS_ROOT}/apps/soccer/src/camera.c")
target_include_directories(tabos_soccer_game PRIVATE "${TABOS_ROOT}/apps/soccer/include")
tabos_enable_warnings(tabos_soccer_game)
tabos_enable_sanitizers(tabos_soccer_game)
add_test(NAME unit.soccer_game COMMAND tabos_soccer_game)

add_executable(tabos_soccer_visuals unit/soccer_visuals.c "${TABOS_ROOT}/apps/soccer/src/visuals.c")
target_include_directories(tabos_soccer_visuals PRIVATE "${TABOS_ROOT}/apps/soccer/include")
tabos_enable_warnings(tabos_soccer_visuals)
tabos_enable_sanitizers(tabos_soccer_visuals)
add_test(NAME unit.soccer_visuals COMMAND tabos_soccer_visuals)

add_executable(tabos_soccer_sound
    unit/soccer_sound.c
    "${TABOS_ROOT}/apps/soccer/src/sound.c"
)
target_include_directories(tabos_soccer_sound PRIVATE
    "${TABOS_ROOT}/apps/soccer/include" "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_soccer_sound)
tabos_enable_sanitizers(tabos_soccer_sound)
add_test(NAME unit.soccer_sound COMMAND tabos_soccer_sound)

add_executable(tabos_starfall_game
    unit/starfall_game.c
    "${TABOS_ROOT}/apps/starfall/src/game.c"
)
target_include_directories(tabos_starfall_game PRIVATE
    "${TABOS_ROOT}/apps/starfall/include"
)
tabos_enable_warnings(tabos_starfall_game)
tabos_enable_sanitizers(tabos_starfall_game)
add_test(NAME unit.starfall_game COMMAND tabos_starfall_game)

add_executable(tabos_irc_protocol
    unit/irc_protocol.c
    "${TABOS_ROOT}/apps/netutils/src/irc/protocol.c"
)
target_include_directories(tabos_irc_protocol PRIVATE
    "${TABOS_ROOT}/apps/netutils/src/irc"
)
tabos_enable_warnings(tabos_irc_protocol)
tabos_enable_sanitizers(tabos_irc_protocol)
add_test(NAME unit.irc_protocol COMMAND tabos_irc_protocol)

add_executable(tabos_doom_video
    unit/doom_video.c
    "${TABOS_ROOT}/apps/doom/src/doom_tabos_video.c"
)
target_include_directories(tabos_doom_video PRIVATE
    "${TABOS_ROOT}/apps/doom/include"
    "${TABOS_ROOT}/sdk/include"
)
tabos_enable_warnings(tabos_doom_video)
tabos_enable_sanitizers(tabos_doom_video)
add_test(NAME unit.doom_video COMMAND tabos_doom_video)

add_executable(tabos_doom_input
    unit/doom_input.c
    "${TABOS_ROOT}/apps/doom/src/doom_tabos_input.c"
)
target_include_directories(tabos_doom_input PRIVATE
    "${TABOS_ROOT}/apps/doom/include"
    "${TABOS_ROOT}/sdk/include"
)
tabos_enable_warnings(tabos_doom_input)
tabos_enable_sanitizers(tabos_doom_input)
add_test(NAME unit.doom_input COMMAND tabos_doom_input)

add_executable(tabos_doom_sfx
    unit/doom_sfx.c
    "${TABOS_ROOT}/apps/doom/src/doom_tabos_sfx.c"
)
target_include_directories(tabos_doom_sfx PRIVATE "${TABOS_ROOT}/apps/doom/include")
tabos_enable_warnings(tabos_doom_sfx)
tabos_enable_sanitizers(tabos_doom_sfx)
add_test(NAME unit.doom_sfx COMMAND tabos_doom_sfx)

add_executable(tabos_doom_storage
    unit/doom_storage.c
    "${TABOS_ROOT}/apps/doom/src/doom_tabos_storage.c"
)
target_include_directories(tabos_doom_storage PRIVATE
    "${TABOS_ROOT}/apps/doom/include"
    "${TABOS_ROOT}/sdk/include"
)
tabos_enable_warnings(tabos_doom_storage)
tabos_enable_sanitizers(tabos_doom_storage)
add_test(NAME unit.doom_storage COMMAND tabos_doom_storage)

add_executable(tabos_display_diagnostic unit/display_diagnostic.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_display_diagnostic PRIVATE
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/net/include"
    "${TABOS_ROOT}/platform/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_display_diagnostic PRIVATE tabos_core)
tabos_enable_warnings(tabos_display_diagnostic)
tabos_enable_sanitizers(tabos_display_diagnostic)

add_test(NAME unit.display_diagnostic COMMAND tabos_display_diagnostic)

add_executable(tabos_input_queue unit/input_queue.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_input_queue PRIVATE
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_input_queue PRIVATE tabos_core)
tabos_enable_warnings(tabos_input_queue)
tabos_enable_sanitizers(tabos_input_queue)

add_test(NAME unit.input_queue COMMAND tabos_input_queue)

add_executable(tabos_console_service unit/console_service.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_console_service PRIVATE
    "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_console_service PRIVATE tabos_core Threads::Threads)
tabos_enable_warnings(tabos_console_service)
tabos_enable_sanitizers(tabos_console_service)

add_test(NAME unit.console_service COMMAND tabos_console_service)

add_executable(tabos_terminal_ansi unit/terminal_ansi.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_terminal_ansi PRIVATE
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_terminal_ansi PRIVATE tabos_core)
tabos_enable_warnings(tabos_terminal_ansi)
tabos_enable_sanitizers(tabos_terminal_ansi)

add_test(NAME unit.terminal_ansi COMMAND tabos_terminal_ansi)

add_executable(tabos_timer unit/timer.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_timer PRIVATE
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${TABOS_ROOT}/time/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_timer PRIVATE tabos_core)
tabos_enable_warnings(tabos_timer)
tabos_enable_sanitizers(tabos_timer)

add_test(NAME unit.timer COMMAND tabos_timer)

add_executable(tabos_runtime_event unit/runtime_event.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_runtime_event PRIVATE
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_runtime_event PRIVATE tabos_core)
tabos_enable_warnings(tabos_runtime_event)
tabos_enable_sanitizers(tabos_runtime_event)
add_test(NAME unit.runtime_event COMMAND tabos_runtime_event)

add_executable(tabos_power_manager unit/power_manager.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_power_manager PRIVATE
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_power_manager PRIVATE tabos_core)
tabos_enable_warnings(tabos_power_manager)
tabos_enable_sanitizers(tabos_power_manager)
add_test(NAME unit.power_manager COMMAND tabos_power_manager)

add_executable(tabos_power_config unit/power_config.c "${TABOS_ROOT}/kernel/power_config.c")
target_include_directories(tabos_power_config PRIVATE
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
tabos_enable_warnings(tabos_power_config)
tabos_enable_sanitizers(tabos_power_config)
add_test(NAME unit.power_config COMMAND tabos_power_config)

add_executable(tabos_power_config_runtime
    component/power_config.c
    fakes/platform_display.c
    "${TABOS_ROOT}/platform/host/posix/storage_backend.c"
    "${TABOS_ROOT}/platform/posix/storage.c"
)
target_include_directories(tabos_power_config_runtime PRIVATE
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_power_config_runtime PRIVATE tabos_core)
tabos_enable_warnings(tabos_power_config_runtime)
tabos_enable_sanitizers(tabos_power_config_runtime)
add_test(NAME component.power_config COMMAND tabos_power_config_runtime)

add_executable(tabos_host_runtime_event unit/host_runtime_event.c)
target_include_directories(tabos_host_runtime_event PRIVATE
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
if(TABOS_TARGET STREQUAL "linux")
    # platform runtime initialization calls back into portable services, which
    # in turn consume platform synchronization. Rescan both static archives so
    # GNU ld resolves dependencies introduced by either side.
    target_link_libraries(tabos_host_runtime_event PRIVATE
        "-Wl,--start-group" tabos_core tabos_platform_host "-Wl,--end-group"
        SDL3::SDL3
    )
else()
    target_link_libraries(tabos_host_runtime_event PRIVATE
        tabos_core tabos_platform_host SDL3::SDL3
    )
endif()
tabos_enable_warnings(tabos_host_runtime_event)
tabos_enable_sanitizers(tabos_host_runtime_event)
add_test(NAME unit.host_runtime_event COMMAND tabos_host_runtime_event)

add_executable(tabos_host_power_model unit/host_power_model.c)
target_include_directories(tabos_host_power_model PRIVATE
    "${TABOS_ROOT}/platform/host/sdl"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
if(TABOS_TARGET STREQUAL "linux")
    # Power activity reaches portable input/pointer services, which call back
    # into platform synchronization. Rescan both archives as for runtime events.
    target_link_libraries(tabos_host_power_model PRIVATE
        "-Wl,--start-group" tabos_core tabos_platform_host "-Wl,--end-group" SDL3::SDL3
    )
else()
    target_link_libraries(tabos_host_power_model PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
endif()
tabos_enable_warnings(tabos_host_power_model)
tabos_enable_sanitizers(tabos_host_power_model)
add_test(NAME unit.host_power_model COMMAND tabos_host_power_model)

add_executable(tabos_wall_clock unit/wall_clock.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_wall_clock PRIVATE
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${TABOS_ROOT}/time/include"
)
target_link_libraries(tabos_wall_clock PRIVATE tabos_core)
tabos_enable_warnings(tabos_wall_clock)
tabos_enable_sanitizers(tabos_wall_clock)
add_test(NAME unit.wall_clock COMMAND tabos_wall_clock)

add_executable(tabos_sdk_clock unit/sdk_clock.c "${TABOS_ROOT}/sdk/lib/clock.c")
target_include_directories(tabos_sdk_clock PRIVATE "${TABOS_ROOT}/sdk/include")
target_compile_definitions(tabos_sdk_clock PRIVATE _POSIX_C_SOURCE=200809L)
tabos_enable_warnings(tabos_sdk_clock)
tabos_enable_sanitizers(tabos_sdk_clock)
add_test(NAME unit.sdk_clock COMMAND tabos_sdk_clock)

add_executable(tabos_network_config unit/network_config.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_network_config PRIVATE
    "${TABOS_ROOT}/net/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_network_config PRIVATE tabos_core)
tabos_enable_warnings(tabos_network_config)
tabos_enable_sanitizers(tabos_network_config)
add_test(NAME unit.network_config COMMAND tabos_network_config)

add_executable(tabos_network_service unit/network_service.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_network_service PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/net/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_network_service PRIVATE tabos_core Threads::Threads)
tabos_enable_warnings(tabos_network_service)
tabos_enable_sanitizers(tabos_network_service)
add_test(NAME unit.network_service COMMAND tabos_network_service)

add_executable(tabos_shell_parser
    unit/shell_parser.c
    "${TABOS_ROOT}/apps/shell/src/parser.c"
)
target_include_directories(tabos_shell_parser PRIVATE
    "${TABOS_ROOT}/apps/shell/include"
)
tabos_enable_warnings(tabos_shell_parser)
tabos_enable_sanitizers(tabos_shell_parser)

add_test(NAME unit.shell_parser COMMAND tabos_shell_parser)

add_executable(tabos_shell_input
    unit/shell_input.c
    "${TABOS_ROOT}/apps/shell/src/input.c"
)
target_include_directories(tabos_shell_input PRIVATE
    "${TABOS_ROOT}/apps/shell/include"
)
tabos_enable_warnings(tabos_shell_input)
tabos_enable_sanitizers(tabos_shell_input)

add_test(NAME unit.shell_input COMMAND tabos_shell_input)

add_executable(tabos_shell_history
    unit/shell_history.c
    "${TABOS_ROOT}/apps/shell/src/history.c"
    "${TABOS_ROOT}/apps/shell/src/input.c"
    "${TABOS_ROOT}/apps/shell/src/line.c"
    "${TABOS_ROOT}/apps/shell/src/parser.c"
)
target_include_directories(tabos_shell_history PRIVATE "${TABOS_ROOT}/apps/shell/include")
tabos_enable_warnings(tabos_shell_history)
tabos_enable_sanitizers(tabos_shell_history)
add_test(NAME unit.shell_history COMMAND tabos_shell_history)

add_library(tabos_shell_history_file_faults OBJECT "${TABOS_ROOT}/apps/shell/src/history_file.c")
target_include_directories(tabos_shell_history_file_faults PRIVATE "${TABOS_ROOT}/apps/shell/include")
target_compile_definitions(tabos_shell_history_file_faults PRIVATE
    fprintf=test_history_fprintf fclose=test_history_fclose rename=test_history_rename
)
# Optimized glibc fortify headers replace standard function names after command-line
# macro substitution. Disable fortify only for objects whose libc calls are test seams.
if(TABOS_TARGET STREQUAL "linux")
    target_compile_options(tabos_shell_history_file_faults PRIVATE -U_FORTIFY_SOURCE)
endif()
tabos_enable_warnings(tabos_shell_history_file_faults)
tabos_enable_sanitizers(tabos_shell_history_file_faults)
add_executable(tabos_shell_history_file
    component/shell_history_file.c
    "${TABOS_ROOT}/apps/shell/src/history.c"
    $<TARGET_OBJECTS:tabos_shell_history_file_faults>
)
target_include_directories(tabos_shell_history_file PRIVATE "${TABOS_ROOT}/apps/shell/include")
tabos_enable_warnings(tabos_shell_history_file)
tabos_enable_sanitizers(tabos_shell_history_file)
add_test(NAME component.shell_history_file COMMAND tabos_shell_history_file)
set_tests_properties(component.shell_history_file PROPERTIES TIMEOUT 10)

add_library(tabos_starfall_storage_faults OBJECT "${TABOS_ROOT}/apps/starfall/src/storage.c")
target_include_directories(tabos_starfall_storage_faults PRIVATE "${TABOS_ROOT}/apps/starfall/include")
target_compile_definitions(tabos_starfall_storage_faults PRIVATE rename=test_starfall_rename)
if(TABOS_TARGET STREQUAL "linux")
    target_compile_options(tabos_starfall_storage_faults PRIVATE -U_FORTIFY_SOURCE)
endif()
tabos_enable_warnings(tabos_starfall_storage_faults)
tabos_enable_sanitizers(tabos_starfall_storage_faults)
add_executable(tabos_starfall_storage
    component/starfall_storage.c
    $<TARGET_OBJECTS:tabos_starfall_storage_faults>
)
target_include_directories(tabos_starfall_storage PRIVATE "${TABOS_ROOT}/apps/starfall/include")
tabos_enable_warnings(tabos_starfall_storage)
tabos_enable_sanitizers(tabos_starfall_storage)
add_test(NAME component.starfall_storage COMMAND tabos_starfall_storage)
set_tests_properties(component.starfall_storage PROPERTIES TIMEOUT 10)

add_library(tabos_shell_session_main OBJECT "${TABOS_ROOT}/apps/shell/src/main.c")
target_include_directories(tabos_shell_session_main PRIVATE
    "${TABOS_ROOT}/apps/shell/include" "${TABOS_ROOT}/sdk/include"
)
target_compile_definitions(tabos_shell_session_main PRIVATE
    main=shell_test_main read=test_shell_read getcwd=test_shell_getcwd
    ioctl=test_shell_ioctl tabos_spawn=test_shell_spawn waitpid=test_shell_waitpid
)
if(TABOS_TARGET STREQUAL "linux")
    target_compile_options(tabos_shell_session_main PRIVATE -U_FORTIFY_SOURCE)
endif()
tabos_enable_warnings(tabos_shell_session_main)
tabos_enable_sanitizers(tabos_shell_session_main)
add_executable(tabos_shell_session
    component/shell_session.c
    "${TABOS_ROOT}/apps/shell/src/history.c"
    "${TABOS_ROOT}/apps/shell/src/history_file.c"
    "${TABOS_ROOT}/apps/shell/src/line.c"
    "${TABOS_ROOT}/apps/shell/src/input.c"
    "${TABOS_ROOT}/apps/shell/src/parser.c"
    $<TARGET_OBJECTS:tabos_shell_session_main>
    ${TABOS_TEST_PLATFORM_FAKES}
)
target_include_directories(tabos_shell_session PRIVATE
    "${TABOS_ROOT}/apps/shell/include" "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/platform/include" "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_shell_session PRIVATE tabos_core)
tabos_enable_warnings(tabos_shell_session)
tabos_enable_sanitizers(tabos_shell_session)
add_test(NAME component.shell_session COMMAND tabos_shell_session)
set_tests_properties(component.shell_session PROPERTIES TIMEOUT 10)

# Run explicitly with a freshly SDK-built shell path. Ordinary host tests must
# remain independent of separately built/installed application artifacts.
add_executable(tabos_shell_rv32 component/shell_rv32.c)
target_include_directories(tabos_shell_rv32 PRIVATE
    "${TABOS_ROOT}/kernel/include" "${TABOS_ROOT}/input/include" "${TABOS_ROOT}/platform/include"
)
if(TABOS_TARGET STREQUAL "linux")
    target_link_libraries(tabos_shell_rv32 PRIVATE
        "-Wl,--start-group" tabos_core tabos_platform_host "-Wl,--end-group" SDL3::SDL3
    )
else()
    target_link_libraries(tabos_shell_rv32 PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
endif()
tabos_enable_warnings(tabos_shell_rv32)
tabos_enable_sanitizers(tabos_shell_rv32)

# Optional real-application harness; ordinary CTest does not require SDK artifacts.
add_executable(tabos_basic_rv32 "${TABOS_ROOT}/apps/basic/tests/rv32.c")
target_include_directories(tabos_basic_rv32 PRIVATE
    "${TABOS_ROOT}/audio/include"
    "${TABOS_ROOT}/kernel/include" "${TABOS_ROOT}/input/include" "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/console/include" "${TABOS_ROOT}/graphics/include"
)
if(TABOS_TARGET STREQUAL "linux")
    target_link_libraries(tabos_basic_rv32 PRIVATE
        "-Wl,--start-group" tabos_core tabos_platform_host "-Wl,--end-group" SDL3::SDL3
    )
else()
    target_link_libraries(tabos_basic_rv32 PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
endif()
tabos_enable_warnings(tabos_basic_rv32)
tabos_enable_sanitizers(tabos_basic_rv32)

add_executable(tabos_basic_kernal_helper "${TABOS_ROOT}/apps/basic/tests/kernal_helper.c")
target_include_directories(tabos_basic_kernal_helper PRIVATE "${TABOS_ROOT}/apps/basic/include")
tabos_enable_warnings(tabos_basic_kernal_helper)
tabos_enable_sanitizers(tabos_basic_kernal_helper)
add_test(NAME component.basic_kernal_helper COMMAND tabos_basic_kernal_helper)

add_executable(tabos_basic_storage "${TABOS_ROOT}/apps/basic/tests/storage.c")
target_include_directories(tabos_basic_storage PRIVATE "${TABOS_ROOT}/apps/basic/include")
tabos_enable_sanitizers(tabos_basic_storage)
add_test(NAME component.basic_storage COMMAND tabos_basic_storage)

add_executable(tabos_basic_native
    "${TABOS_ROOT}/apps/basic/src/main.c"
    "${TABOS_ROOT}/apps/basic/src/basic_engine.c"
    "${TABOS_ROOT}/apps/basic/src/basic_kernal.c"
    "${TABOS_ROOT}/apps/basic/src/basic_storage.c"
    "${TABOS_ROOT}/apps/basic/src/tabos_runtime.c"
    "${TABOS_ROOT}/apps/basic/src/basic_extensions.c"
    "${TABOS_ROOT}/apps/basic/src/basic_graphics.c"
    "${TABOS_ROOT}/apps/basic/src/basic_c64.c"
    "${TABOS_ROOT}/apps/basic/src/basic_sid.c"
    "${TABOS_ROOT}/apps/basic/src/basic_sound.c"
    "${TABOS_ROOT}/sdk/lib/graphics.c"
    "${TABOS_ROOT}/sdk/lib/audio.c"
    "${TABOS_ROOT}/apps/basic/tests/media_platform.c"
    "${TABOS_ROOT}/apps/basic/upstream/cbmbasic.c"
    "${TABOS_ROOT}/apps/basic/tests/native_platform.c"
)
target_include_directories(tabos_basic_native PRIVATE
    "${TABOS_ROOT}/apps/basic/include" "${TABOS_ROOT}/sdk/include"
)
tabos_enable_sanitizers(tabos_basic_native)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME component.basic_native
    COMMAND "${Python3_EXECUTABLE}" "${TABOS_ROOT}/apps/basic/tests/native.py" $<TARGET_FILE:tabos_basic_native>
)
set_tests_properties(component.basic_native PROPERTIES TIMEOUT 60)
add_test(NAME component.basic_compatibility
    COMMAND "${Python3_EXECUTABLE}" "${TABOS_ROOT}/apps/basic/tests/compatibility.py" $<TARGET_FILE:tabos_basic_native>
)
set_tests_properties(component.basic_compatibility PROPERTIES TIMEOUT 60)
add_test(NAME component.basic_policy
    COMMAND "${Python3_EXECUTABLE}" "${TABOS_ROOT}/apps/basic/tests/policy.py" $<TARGET_FILE:tabos_basic_native>
)
set_tests_properties(component.basic_policy PROPERTIES TIMEOUT 60)
add_test(NAME component.basic_graphics
    COMMAND "${Python3_EXECUTABLE}" "${TABOS_ROOT}/apps/basic/tests/graphics.py" $<TARGET_FILE:tabos_basic_native>)
set_tests_properties(component.basic_graphics PROPERTIES TIMEOUT 60)
add_test(NAME component.basic_c64_graphics
    COMMAND "${Python3_EXECUTABLE}" "${TABOS_ROOT}/apps/basic/tests/c64_graphics.py" $<TARGET_FILE:tabos_basic_native>)
set_tests_properties(component.basic_c64_graphics PROPERTIES TIMEOUT 90)
add_test(NAME component.basic_c64_profile
    COMMAND "${Python3_EXECUTABLE}" "${TABOS_ROOT}/apps/basic/tests/j0_profile.py" $<TARGET_FILE:tabos_basic_native>)
set_tests_properties(component.basic_c64_profile PROPERTIES TIMEOUT 90)
add_test(NAME component.basic_c64_sid
    COMMAND "${Python3_EXECUTABLE}" "${TABOS_ROOT}/apps/basic/tests/c64_sid.py" $<TARGET_FILE:tabos_basic_native>)
set_tests_properties(component.basic_c64_sid PROPERTIES TIMEOUT 60)
add_executable(tabos_basic_sid_math
    "${TABOS_ROOT}/apps/basic/src/basic_sid.c"
    "${TABOS_ROOT}/apps/basic/tests/sid_math.c")
target_include_directories(tabos_basic_sid_math PRIVATE
    "${TABOS_ROOT}/apps/basic/include" "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_basic_sid_math)
tabos_enable_sanitizers(tabos_basic_sid_math)
add_test(NAME component.basic_sid_math COMMAND tabos_basic_sid_math)
add_test(NAME component.basic_examples
    COMMAND "${Python3_EXECUTABLE}" "${TABOS_ROOT}/apps/basic/tests/build_examples.py" $<TARGET_FILE:tabos_basic_native>)
set_tests_properties(component.basic_examples PROPERTIES TIMEOUT 60)
add_test(NAME component.basic_release_audit
    COMMAND "${Python3_EXECUTABLE}" "${TABOS_ROOT}/apps/basic/tests/release_audit.py")
add_test(NAME component.basic_release_session
    COMMAND "${Python3_EXECUTABLE}" "${TABOS_ROOT}/apps/basic/tests/release_session.py" $<TARGET_FILE:tabos_basic_native>)
set_tests_properties(component.basic_release_session PROPERTIES TIMEOUT 90)
target_compile_definitions(tabos_basic_rv32 PRIVATE TABOS_BASIC_EXAMPLES_DIR="${TABOS_ROOT}/apps/basic/examples")

add_executable(tabos_application_lifecycle unit/application_lifecycle.c ${TABOS_TEST_PLATFORM_FAKES})
target_include_directories(tabos_application_lifecycle PRIVATE
    "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_application_lifecycle PRIVATE tabos_core)
tabos_enable_warnings(tabos_application_lifecycle)
tabos_enable_sanitizers(tabos_application_lifecycle)

add_test(NAME unit.application_lifecycle COMMAND tabos_application_lifecycle)

add_executable(tabos_elf_loader
    unit/elf_loader.c
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf/hello_elf.c"
    ${TABOS_TEST_PLATFORM_FAKES}
)
target_include_directories(tabos_elf_loader PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf"
    "${TABOS_ROOT}/loader/include"
    "${TABOS_ROOT}/platform/include"
)
target_link_libraries(tabos_elf_loader PRIVATE tabos_core)
tabos_enable_warnings(tabos_elf_loader)
tabos_enable_sanitizers(tabos_elf_loader)

add_test(NAME unit.elf_loader COMMAND tabos_elf_loader)

add_executable(tabos_file_elf_loader
    component/file_elf_loader.c
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf/hello_elf.c"
    fakes/platform_display.c
    fakes/storage_backend.c
    "${TABOS_ROOT}/platform/posix/storage.c"
)
target_include_directories(tabos_file_elf_loader PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf"
    "${TABOS_ROOT}/loader/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_file_elf_loader PRIVATE tabos_core)
tabos_enable_warnings(tabos_file_elf_loader)
tabos_enable_sanitizers(tabos_file_elf_loader)

add_test(NAME component.file_elf_loader COMMAND tabos_file_elf_loader)

add_executable(tabos_rv32_execution
    component/rv32_execution.c
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf/hello_elf.c"
)
target_include_directories(tabos_rv32_execution PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/fixtures/elf"
    "${TABOS_ROOT}/loader/include"
    "${TABOS_ROOT}/platform/include"
)
if(TABOS_TARGET STREQUAL "linux")
    # tabos_core consumes the platform contract while the host backend calls
    # back into portable services. GNU ld needs both static archives rescanned
    # when this focused executable does not otherwise pull every host object in.
    target_link_libraries(tabos_rv32_execution PRIVATE
        "-Wl,--start-group" tabos_core tabos_platform_host "-Wl,--end-group"
        SDL3::SDL3
    )
else()
    target_link_libraries(tabos_rv32_execution PRIVATE
        tabos_core tabos_platform_host SDL3::SDL3
    )
endif()
tabos_enable_warnings(tabos_rv32_execution)
tabos_enable_sanitizers(tabos_rv32_execution)

add_test(NAME component.rv32_execution COMMAND tabos_rv32_execution)

add_executable(tabos_elf_graphics_cleanup component/elf_graphics_cleanup.c)
target_include_directories(tabos_elf_graphics_cleanup PRIVATE
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/platform/include"
)
if(TABOS_TARGET STREQUAL "linux")
    target_link_libraries(tabos_elf_graphics_cleanup PRIVATE
        "-Wl,--start-group" tabos_core tabos_platform_host "-Wl,--end-group" SDL3::SDL3
    )
else()
    target_link_libraries(tabos_elf_graphics_cleanup PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
endif()
tabos_enable_warnings(tabos_elf_graphics_cleanup)
tabos_enable_sanitizers(tabos_elf_graphics_cleanup)
add_test(NAME component.elf_graphics_cleanup COMMAND tabos_elf_graphics_cleanup)
set_tests_properties(component.elf_graphics_cleanup PROPERTIES TIMEOUT 30)

add_executable(tabos_elf_wait component/elf_wait.c)
target_include_directories(tabos_elf_wait PRIVATE "${TABOS_ROOT}/input/include"
    "${TABOS_ROOT}/net/include"
    "${TABOS_ROOT}/kernel/include"
    "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/platform/include"
)
if(TABOS_TARGET STREQUAL "linux")
    target_link_libraries(tabos_elf_wait PRIVATE
        "-Wl,--start-group" tabos_core tabos_platform_host "-Wl,--end-group" SDL3::SDL3
    )
else()
    target_link_libraries(tabos_elf_wait PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
endif()
tabos_enable_warnings(tabos_elf_wait)
tabos_enable_sanitizers(tabos_elf_wait)
add_test(NAME component.elf_wait COMMAND tabos_elf_wait)
set_tests_properties(component.elf_wait PROPERTIES TIMEOUT 30)

add_executable(tabos_network_socket
    component/network_socket.c
    "${TABOS_ROOT}/platform/posix/socket.c"
    "${TABOS_ROOT}/platform/posix/host_io.c"
)
find_package(Threads REQUIRED)
add_executable(tabos_host_io unit/host_io.c "${TABOS_ROOT}/platform/posix/host_io.c")
target_link_libraries(tabos_host_io PRIVATE Threads::Threads)
target_compile_definitions(tabos_host_io PRIVATE _POSIX_C_SOURCE=200809L)
tabos_enable_warnings(tabos_host_io)
tabos_enable_sanitizers(tabos_host_io)
add_test(NAME unit.host_io COMMAND tabos_host_io)
add_executable(tabos_native_task unit/native_task.c "${TABOS_ROOT}/platform/esp32p4/application_task.c")
target_include_directories(tabos_native_task PRIVATE
    "${TABOS_ROOT}/tests/fakes/native_task"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${TABOS_GENERATED_INCLUDE_DIR}")
target_link_libraries(tabos_native_task PRIVATE Threads::Threads)
tabos_enable_warnings(tabos_native_task)
tabos_enable_sanitizers(tabos_native_task)
add_test(NAME unit.native_task COMMAND tabos_native_task)
set_tests_properties(unit.native_task PROPERTIES TIMEOUT 15)
add_executable(tabos_wifi_driver
    unit/wifi_driver.c
    "${TABOS_ROOT}/platform/esp32p4/wifi_driver.c"
    "${TABOS_ROOT}/platform/esp32p4/wifi_startup.c")
target_include_directories(tabos_wifi_driver PRIVATE
    "${TABOS_ROOT}/tests/fakes/wifi_driver"
    "${TABOS_ROOT}/platform/esp32p4")
tabos_enable_warnings(tabos_wifi_driver)
tabos_enable_sanitizers(tabos_wifi_driver)
target_link_libraries(tabos_wifi_driver PRIVATE Threads::Threads)
add_test(NAME unit.wifi_driver COMMAND tabos_wifi_driver)
add_executable(tabos_native_tls_cancel unit/native_tls_cancel.c)
target_include_directories(tabos_native_tls_cancel PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_native_tls_cancel)
tabos_enable_sanitizers(tabos_native_tls_cancel)
add_test(NAME unit.native_tls_cancel COMMAND tabos_native_tls_cancel)
add_executable(tabos_native_socket_cancel component/native_socket_cancel.c
    "${TABOS_ROOT}/platform/posix/host_io.c")
target_include_directories(tabos_native_socket_cancel PRIVATE "${TABOS_ROOT}/platform/include" "${TABOS_ROOT}/sdk/include")
target_link_libraries(tabos_native_socket_cancel PRIVATE Threads::Threads)
tabos_enable_warnings(tabos_native_socket_cancel)
tabos_enable_sanitizers(tabos_native_socket_cancel)
add_test(NAME component.native_socket_cancel COMMAND tabos_native_socket_cancel)
set_tests_properties(component.native_socket_cancel PROPERTIES TIMEOUT 10)
set_tests_properties(unit.host_io PROPERTIES TIMEOUT 10)
add_executable(tabos_host_network_io component/host_network_io.c
    "${TABOS_ROOT}/platform/posix/host_io.c"
    "${TABOS_ROOT}/platform/posix/socket.c"
    "${TABOS_ROOT}/platform/posix/network.c"
    "${TABOS_ROOT}/platform/posix/tls.c")
target_include_directories(tabos_host_network_io PRIVATE "${TABOS_ROOT}/platform/include" "${TABOS_ROOT}/sdk/include")
target_compile_definitions(tabos_host_network_io PRIVATE _POSIX_C_SOURCE=200809L TABOS_TEST_TLS_HOOKS=1)
if(TABOS_TARGET STREQUAL "macos")
    target_compile_definitions(tabos_host_network_io PRIVATE _DARWIN_C_SOURCE)
endif()
target_link_libraries(tabos_host_network_io PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
tabos_enable_warnings(tabos_host_network_io)
tabos_enable_sanitizers(tabos_host_network_io)
add_test(NAME component.host_network_io COMMAND tabos_host_network_io)
set_tests_properties(component.host_network_io PROPERTIES TIMEOUT 15)
target_link_libraries(tabos_network_socket PRIVATE Threads::Threads)
if(TABOS_TARGET STREQUAL "linux")
    target_compile_definitions(tabos_network_socket PRIVATE _POSIX_C_SOURCE=200809L)
endif()
target_include_directories(tabos_network_socket PRIVATE
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
tabos_enable_warnings(tabos_network_socket)
tabos_enable_sanitizers(tabos_network_socket)
add_test(NAME component.network_socket COMMAND tabos_network_socket)

add_executable(tabos_filesystem
    component/filesystem.c
    fakes/storage_backend.c
    "${TABOS_ROOT}/platform/posix/storage.c"
)
target_include_directories(tabos_filesystem PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_filesystem PRIVATE tabos_core)
tabos_enable_warnings(tabos_filesystem)
tabos_enable_sanitizers(tabos_filesystem)

add_test(NAME component.filesystem COMMAND tabos_filesystem)

add_library(tabos_storage_rename_source OBJECT "${TABOS_ROOT}/platform/posix/storage.c")
target_include_directories(tabos_storage_rename_source PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
)
target_compile_definitions(tabos_storage_rename_source PRIVATE rename=test_storage_rename)
if(TABOS_TARGET STREQUAL "linux")
    target_compile_options(tabos_storage_rename_source PRIVATE -U_FORTIFY_SOURCE)
endif()
tabos_enable_warnings(tabos_storage_rename_source)
tabos_enable_sanitizers(tabos_storage_rename_source)
add_executable(tabos_storage_rename
    component/storage_rename.c
    fakes/storage_backend.c
    $<TARGET_OBJECTS:tabos_storage_rename_source>
)
target_include_directories(tabos_storage_rename PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_storage_rename PRIVATE tabos_core)
tabos_enable_warnings(tabos_storage_rename)
tabos_enable_sanitizers(tabos_storage_rename)
add_test(NAME component.storage_rename COMMAND tabos_storage_rename)

add_library(tabos_posix_source OBJECT
    component/posix_filesystem.c
)
target_include_directories(tabos_posix_source PRIVATE
    "${TABOS_ROOT}/sdk/posix/include"
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/sdk/include"
)
tabos_enable_warnings(tabos_posix_source)
tabos_enable_sanitizers(tabos_posix_source)

add_executable(tabos_posix_filesystem
    $<TARGET_OBJECTS:tabos_posix_source>
    fakes/storage_backend.c
    "${TABOS_ROOT}/platform/posix/storage.c"
)
target_include_directories(tabos_posix_filesystem PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_posix_filesystem PRIVATE tabos_core)
tabos_enable_warnings(tabos_posix_filesystem)
tabos_enable_sanitizers(tabos_posix_filesystem)

add_test(NAME component.posix_filesystem COMMAND tabos_posix_filesystem)

add_library(tabos_coreutils_cp_program OBJECT "${TABOS_ROOT}/apps/coreutils/src/cp/main.c")
target_include_directories(tabos_coreutils_cp_program PRIVATE
    "${TABOS_ROOT}/sdk/posix/include"
    "${TABOS_ROOT}/sdk/include"
)
target_compile_definitions(tabos_coreutils_cp_program PRIVATE main=tabos_cp_main)
tabos_enable_warnings(tabos_coreutils_cp_program)
tabos_enable_sanitizers(tabos_coreutils_cp_program)

add_executable(tabos_coreutils_cp
    component/coreutils_cp.c
    $<TARGET_OBJECTS:tabos_coreutils_cp_program>
    fakes/storage_backend.c
    "${TABOS_ROOT}/platform/posix/storage.c"
)
target_include_directories(tabos_coreutils_cp PRIVATE
    "${TABOS_ROOT}/fs/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_coreutils_cp PRIVATE tabos_core)
tabos_enable_warnings(tabos_coreutils_cp)
tabos_enable_sanitizers(tabos_coreutils_cp)
add_test(NAME component.coreutils_cp COMMAND tabos_coreutils_cp)

add_executable(tabos_coreutils_wc unit/coreutils_wc.c)
target_include_directories(tabos_coreutils_wc PRIVATE "${TABOS_ROOT}")
tabos_enable_warnings(tabos_coreutils_wc)
tabos_enable_sanitizers(tabos_coreutils_wc)
add_test(NAME unit.coreutils_wc COMMAND tabos_coreutils_wc)

add_executable(tabos_coreutils_ls unit/coreutils_ls.c)
target_include_directories(tabos_coreutils_ls PRIVATE "${TABOS_ROOT}" "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_coreutils_ls)
tabos_enable_sanitizers(tabos_coreutils_ls)
add_test(NAME unit.coreutils_ls COMMAND tabos_coreutils_ls)

add_executable(tabos_netutils_fetch unit/netutils_fetch.c)
target_include_directories(tabos_netutils_fetch PRIVATE
    "${TABOS_ROOT}"
    "${TABOS_ROOT}/sdk/include"
)
tabos_enable_warnings(tabos_netutils_fetch)
tabos_enable_sanitizers(tabos_netutils_fetch)
add_test(NAME unit.netutils_fetch COMMAND tabos_netutils_fetch)

add_test(NAME integration.host_smoke COMMAND tabos_host --smoke)
set_tests_properties(integration.host_smoke PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
)

add_test(
    NAME architecture.portable_header_boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_boundaries.cmake"
)

add_test(
    NAME architecture.shell_api_boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_shell_boundary.cmake"
)

add_test(
    NAME architecture.application_api_boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_application_api_boundary.cmake"
)

add_test(
    NAME architecture.symbol_naming
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_naming.cmake"
)

add_test(
    NAME build.invalid_host_target
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        "-DTABOS_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/invalid-target"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_invalid_target.cmake"
)

add_test(
    NAME build.application_tracking
    COMMAND "${CMAKE_COMMAND}"
        "-DTABOS_SOURCE_DIR=${TABOS_ROOT}"
        "-DTABOS_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/application-build-tracking"
        "-DTABOS_C_COMPILER=${CMAKE_C_COMPILER}"
        -P "${CMAKE_CURRENT_LIST_DIR}/check_application_build_tracking.cmake"
)

add_test(
    NAME build.application_arguments
    COMMAND sh "${CMAKE_CURRENT_LIST_DIR}/check_application_build_arguments.sh"
        "${TABOS_ROOT}"
)

add_executable(tabos_camera_concurrency
    component/camera_concurrency.c
    "${TABOS_ROOT}/camera/camera.c"
    "${TABOS_ROOT}/platform/host/sdl/mutex.c"
)
target_include_directories(tabos_camera_concurrency PRIVATE
    "${TABOS_ROOT}/camera/include"
    "${TABOS_ROOT}/platform/include"
    "${TABOS_ROOT}/sdk/include"
    "${TABOS_GENERATED_INCLUDE_DIR}"
)
target_link_libraries(tabos_camera_concurrency PRIVATE SDL3::SDL3)
tabos_enable_warnings(tabos_camera_concurrency)
tabos_enable_sanitizers(tabos_camera_concurrency)
add_test(NAME component.camera_concurrency COMMAND tabos_camera_concurrency)
set_tests_properties(component.camera_concurrency PROPERTIES TIMEOUT 15)

add_executable(tabos_kilo component/kilo.c ${TABOS_TEST_PLATFORM_FAKES}
    "${TABOS_ROOT}/apps/kilo/src/editor.c"
    "${TABOS_ROOT}/apps/kilo/src/input.c"
    "${TABOS_ROOT}/apps/kilo/src/storage.c"
    "${TABOS_ROOT}/apps/kilo/src/render.c")
target_include_directories(tabos_kilo PRIVATE "${TABOS_ROOT}/apps/kilo/include"
    "${TABOS_ROOT}/platform/include" "${TABOS_ROOT}/graphics/include"
    "${TABOS_ROOT}/kernel/include" "${TABOS_GENERATED_INCLUDE_DIR}")
target_link_libraries(tabos_kilo PRIVATE tabos_core)
tabos_enable_warnings(tabos_kilo)
tabos_enable_sanitizers(tabos_kilo)
add_test(NAME component.kilo COMMAND tabos_kilo)

add_executable(tabos_kilo_rv32 component/kilo_rv32.c)
target_include_directories(tabos_kilo_rv32 PRIVATE
    "${TABOS_ROOT}/kernel/include" "${TABOS_ROOT}/input/include" "${TABOS_ROOT}/platform/include"
)
if(TABOS_TARGET STREQUAL "linux")
    target_link_libraries(tabos_kilo_rv32 PRIVATE
        "-Wl,--start-group" tabos_core tabos_platform_host "-Wl,--end-group" SDL3::SDL3
    )
else()
    target_link_libraries(tabos_kilo_rv32 PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
endif()
tabos_enable_warnings(tabos_kilo_rv32)
tabos_enable_sanitizers(tabos_kilo_rv32)

add_executable(tabos_pool_rv32 component/pool_rv32.c)
target_include_directories(tabos_pool_rv32 PRIVATE
    "${TABOS_ROOT}/apps/pool/include"
    "${TABOS_ROOT}/graphics/include" "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/kernel/include" "${TABOS_ROOT}/input/include" "${TABOS_ROOT}/platform/include"
)
if(TABOS_TARGET STREQUAL "linux")
    target_link_libraries(tabos_pool_rv32 PRIVATE
        "-Wl,--start-group" tabos_core tabos_platform_host "-Wl,--end-group" SDL3::SDL3
    )
else()
    target_link_libraries(tabos_pool_rv32 PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
endif()
tabos_enable_warnings(tabos_pool_rv32)
tabos_enable_sanitizers(tabos_pool_rv32)

add_executable(tabos_soccer_rv32 component/soccer_rv32.c)
target_include_directories(tabos_soccer_rv32 PRIVATE
    "${TABOS_ROOT}/audio/include"
    "${TABOS_ROOT}/graphics/include" "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/kernel/include" "${TABOS_ROOT}/input/include" "${TABOS_ROOT}/platform/include"
)
if(TABOS_TARGET STREQUAL "linux")
    target_link_libraries(tabos_soccer_rv32 PRIVATE
        "-Wl,--start-group" tabos_core tabos_platform_host "-Wl,--end-group" SDL3::SDL3
    )
else()
    target_link_libraries(tabos_soccer_rv32 PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
endif()
tabos_enable_warnings(tabos_soccer_rv32)
tabos_enable_sanitizers(tabos_soccer_rv32)

add_executable(tabos_gpio_interrupt
    unit/gpio_interrupt.c
    "${TABOS_ROOT}/platform/esp32p4/gpio_interrupt.c"
)
target_include_directories(tabos_gpio_interrupt PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/fakes/espidf"
    "${TABOS_ROOT}/platform/esp32p4"
)
tabos_enable_warnings(tabos_gpio_interrupt)
tabos_enable_sanitizers(tabos_gpio_interrupt)
add_test(NAME unit.gpio_interrupt COMMAND tabos_gpio_interrupt)

# Use the same explicit source manifest and profile for native Lua tests.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${TABOS_ROOT}/apps/lua/sources.mk")
file(READ "${TABOS_ROOT}/apps/lua/sources.mk" lua_manifest)
string(REGEX MATCHALL "[a-zA-Z0-9_/]+[.]c" lua_sources "${lua_manifest}")
list(TRANSFORM lua_sources PREPEND "${TABOS_ROOT}/apps/lua/")
add_library(tabos_lua_profile STATIC ${lua_sources} "${TABOS_ROOT}/sdk/lib/graphics.c"
    "${TABOS_ROOT}/sdk/lib/audio.c" "${TABOS_ROOT}/sdk/lib/pointer.c"
    "${TABOS_ROOT}/sdk/lib/device.c")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach(lua_source IN LISTS lua_sources)
        if(lua_source MATCHES "/apps/lua/src/")
            set_property(SOURCE "${lua_source}" APPEND PROPERTY COMPILE_OPTIONS
                -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
        endif()
    endforeach()
endif()
target_include_directories(tabos_lua_profile PUBLIC "${TABOS_ROOT}/apps/lua/include"
    "${TABOS_ROOT}/apps/lua/vendor/lua/src" "${TABOS_ROOT}/sdk/include")
target_compile_options(tabos_lua_profile PUBLIC
    -include "${TABOS_ROOT}/apps/lua/include/lua_tabos/config.h")
tabos_enable_sanitizers(tabos_lua_profile)
add_executable(tabos_lua_native "${TABOS_ROOT}/apps/lua/src/main.c" component/lua_fakes.c)
target_link_libraries(tabos_lua_native PRIVATE tabos_lua_profile m)
tabos_enable_sanitizers(tabos_lua_native)
add_executable(tabos_lua component/lua.c component/lua_fakes.c)
target_link_libraries(tabos_lua PRIVATE tabos_lua_profile m)
tabos_enable_warnings(tabos_lua)
tabos_enable_sanitizers(tabos_lua)
add_test(NAME component.lua COMMAND tabos_lua)
set_tests_properties(component.lua PROPERTIES TIMEOUT 30)
add_executable(tabos_lua_rv32 component/lua_rv32.c)
target_include_directories(tabos_lua_rv32 PRIVATE
    "${TABOS_ROOT}/graphics/include" "${TABOS_ROOT}/console/include"
    "${TABOS_ROOT}/kernel/include" "${TABOS_ROOT}/input/include" "${TABOS_ROOT}/platform/include")
if(TABOS_TARGET STREQUAL "linux")
    target_link_libraries(tabos_lua_rv32 PRIVATE
        "-Wl,--start-group" tabos_core tabos_platform_host "-Wl,--end-group" SDL3::SDL3)
else()
    target_link_libraries(tabos_lua_rv32 PRIVATE tabos_core tabos_platform_host SDL3::SDL3)
endif()
tabos_enable_warnings(tabos_lua_rv32)
tabos_enable_sanitizers(tabos_lua_rv32)

foreach(lua_suite utf8 strings math)
    add_test(NAME component.lua_upstream_${lua_suite}
        COMMAND tabos_lua_native -e "_U=true; _soft=true; _port=true"
            "${TABOS_ROOT}/apps/lua/tests/upstream/${lua_suite}.lua")
    set_tests_properties(component.lua_upstream_${lua_suite} PROPERTIES TIMEOUT 30)
endforeach()

add_test(NAME component.lua_cli COMMAND "${CMAKE_COMMAND}"
    "-DLUA_EXECUTABLE=$<TARGET_FILE:tabos_lua_native>"
    "-DLUA_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/lua-cli"
    -P "${CMAKE_CURRENT_LIST_DIR}/check_lua_cli.cmake")
set_tests_properties(component.lua_cli PROPERTIES TIMEOUT 30)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME unit.host_environment
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/unit/host_environment.py"
        "${TABOS_ROOT}")

add_test(NAME component.lua_snake_audio
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/lua_snake_audio.py"
        "$<TARGET_FILE:tabos_lua_native>" "${TABOS_ROOT}/apps/lua/examples/snake.lua")
set_tests_properties(component.lua_snake_audio PROPERTIES TIMEOUT 40)

add_test(NAME component.lua_starfall
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/lua_starfall.py"
        "$<TARGET_FILE:tabos_lua_native>" "${TABOS_ROOT}/apps/lua/examples/starfall.lua")
set_tests_properties(component.lua_starfall PROPERTIES TIMEOUT 60)

add_test(NAME component.lua_touch
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/lua_touch.py"
        "$<TARGET_FILE:tabos_lua_native>" "${TABOS_ROOT}/apps/lua/examples/touch.lua")
set_tests_properties(component.lua_touch PROPERTIES TIMEOUT 30)

add_test(NAME architecture.check_regressions
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/check_architecture_regressions.py"
        "${CMAKE_COMMAND}" "${CMAKE_CURRENT_LIST_DIR}"
)

add_executable(tabos_sdk_directory_listing
    component/sdk_directory_listing.c
    "${TABOS_ROOT}/sdk/lib/posix_filesystem.c"
    fakes/platform_display.c fakes/storage_backend.c
    "${TABOS_ROOT}/platform/posix/storage.c"
)
target_compile_definitions(tabos_sdk_directory_listing PRIVATE TABOS_APPLICATION=1)
target_include_directories(tabos_sdk_directory_listing PRIVATE
    "${TABOS_ROOT}/fs/include" "${TABOS_ROOT}/loader/include"
    "${TABOS_ROOT}/platform/include" "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_sdk_directory_listing PRIVATE tabos_core)
tabos_enable_warnings(tabos_sdk_directory_listing)
tabos_enable_sanitizers(tabos_sdk_directory_listing)
add_test(NAME component.sdk_directory_listing COMMAND tabos_sdk_directory_listing)

add_executable(tabos_network_config_file
    component/network_config_file.c fakes/platform_display.c
    "${TABOS_ROOT}/platform/posix/storage.c"
)
target_include_directories(tabos_network_config_file PRIVATE
    "${TABOS_ROOT}/fs/include" "${TABOS_ROOT}/net/include"
    "${TABOS_ROOT}/platform/include" "${TABOS_ROOT}/sdk/include"
    "${CMAKE_CURRENT_LIST_DIR}/fakes"
)
target_link_libraries(tabos_network_config_file PRIVATE tabos_core)
tabos_enable_warnings(tabos_network_config_file)
tabos_enable_sanitizers(tabos_network_config_file)
add_test(NAME component.network_config_file COMMAND tabos_network_config_file)

add_executable(tabos_tab5_msc_control unit/tab5_msc_control.c)
tabos_enable_warnings(tabos_tab5_msc_control)
tabos_enable_sanitizers(tabos_tab5_msc_control)
add_test(NAME unit.tab5_msc_control COMMAND tabos_tab5_msc_control)
add_test(NAME tools.msc_upload COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/msc_upload_test.py")

add_test(NAME tools.device_test COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/device_test_test.py")

add_executable(tabos_sdk_compute unit/sdk_compute.c "${TABOS_ROOT}/sdk/lib/compute.c")
target_include_directories(tabos_sdk_compute PRIVATE "${TABOS_ROOT}/sdk/include")
tabos_enable_warnings(tabos_sdk_compute)
tabos_enable_sanitizers(tabos_sdk_compute)
add_test(NAME unit.sdk_compute COMMAND tabos_sdk_compute)
