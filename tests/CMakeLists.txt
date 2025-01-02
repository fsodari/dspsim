message("Test Directory")
Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.0 # or a later release
)

FetchContent_MakeAvailable(Catch2)

include(CTest)
include(Catch)

add_subdirectory(psoc)
