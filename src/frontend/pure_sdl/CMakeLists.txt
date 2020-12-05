project(pure_sdl)

add_executable(melonDS_sdl main.cpp
	Platform.cpp
	PlatformConfig.cpp

	../Util_ROM.cpp
	../Util_Video.cpp
	../Util_Audio.cpp
)

target_include_directories(melonDS_sdl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(melonDS_sdl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_include_directories(melonDS_sdl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(melonDS_sdl core)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
target_link_libraries(melonDS_sdl PkgConfig::SDL2)

find_package(Threads REQUIRED)
target_link_libraries(melonDS_sdl Threads::Threads)

install(TARGETS melonDS_sdl RUNTIME DESTINATION bin)
