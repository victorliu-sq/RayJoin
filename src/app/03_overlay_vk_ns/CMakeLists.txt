
# =========================== LSI_VK Executable =============================
add_executable(polyover_vk_exec_ns main.cc)

target_link_libraries(polyover_vk_exec_ns PRIVATE
        rayjoin_lib_vk
)