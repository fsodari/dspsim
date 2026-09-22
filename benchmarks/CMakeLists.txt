add_executable(sosfilt sosfilt.cpp)
target_link_libraries(sosfilt PRIVATE dspsim::dspsim-core spdlog::spdlog)
