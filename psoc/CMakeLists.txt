# Build for testing
add_library(psoc_common
    ${CMAKE_CURRENT_SOURCE_DIR}/common/mmi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/common/vmmi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/common/vmmi_meta.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/common/avril.c
)
target_include_directories(psoc_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common/include)

enable_testing()
add_subdirectory(tests)
