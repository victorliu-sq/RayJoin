# =========================== LSI_VK Executable =============================
if (NOT RAYJOIN_BUILD_VULKAN)
  return()
endif ()

add_executable(polyover_vk_exec_ns main.cc)

target_link_libraries(polyover_vk_exec_ns PRIVATE
        rayjoin_lib_vk
)