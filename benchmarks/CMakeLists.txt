add_executable(sosfilt sosfilt.cpp)
target_link_libraries(sosfilt PRIVATE dspsim::dspsim-core spdlog::spdlog)

add_executable(parallel_eval parallel_eval.cpp)
target_link_libraries(parallel_eval PRIVATE dspsim::dspsim-core spdlog::spdlog)
