# Build the dspsim/psoc libs using models 

add_library(tmodels ${CMAKE_CURRENT_SOURCE_DIR}/models/_portable.c)
target_include_directories(tmodels PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/models)

set(PSOC_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/dspsim/include)
set(PSOC_SRC_DIR ${PSOC_INCLUDE_DIR}/dspsim/psoc)
add_library(tcdict
    ${PSOC_SRC_DIR}/dtypes.c
    ${PSOC_SRC_DIR}/cdict.c
    ${PSOC_SRC_DIR}/hash.c)
target_include_directories(tcdict PUBLIC ${PSOC_INCLUDE_DIR})
target_link_libraries(tcdict PUBLIC tmodels)

add_executable(test_psoc
    test_cdict.cpp
)
target_link_libraries(test_psoc PRIVATE tcdict)

target_link_libraries(test_psoc PRIVATE Catch2::Catch2WithMain)
catch_discover_tests(test_psoc)
