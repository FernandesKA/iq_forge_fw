cmake_minimum_required(VERSION 3.16)
project(iq_forge_fw CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "" FORCE)
endif()

add_subdirectory(external/fpga_loader)

add_library(hal_spi
    hal/spi/src/spi_device.cpp
    hal/spi/src/spi_config.cpp
)
target_include_directories(hal_spi PUBLIC hal/spi/inc)

add_library(drivers_ad9361
    drivers/ad9361/src/ad9361.cpp
)
target_include_directories(drivers_ad9361 PUBLIC drivers/ad9361/inc)
target_link_libraries(drivers_ad9361 PUBLIC hal_spi)

add_library(project_iq_forge
    project/src/iq_forge.cpp
)
target_include_directories(project_iq_forge PUBLIC project/inc)
target_link_libraries(project_iq_forge PUBLIC drivers_ad9361 hal_spi fpga::loader)

add_executable(iq_forge_app project/src/main.cpp)
target_link_libraries(iq_forge_app PRIVATE project_iq_forge)

install(TARGETS iq_forge_app RUNTIME DESTINATION bin)
