cmake_minimum_required(VERSION 3.16)
project(iq_forge_fw CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist" CACHE PATH "" FORCE)
endif()

add_subdirectory(external/fpga_loader)

add_library(hal_spi
    hal/spi/src/spi_device.cpp
    hal/spi/src/spi_config.cpp
)
target_include_directories(hal_spi PUBLIC hal/spi/inc)

add_library(hal_gpio
    hal/gpio/src/mmio_register.cpp
)
target_include_directories(hal_gpio PUBLIC hal/gpio/inc)

add_library(drivers_ad9361
    drivers/ad9361/src/ad9361.cpp
)
target_include_directories(drivers_ad9361 PUBLIC drivers/ad9361/inc)
target_link_libraries(drivers_ad9361 PUBLIC hal_spi)

add_library(ad9361_api_no_os
    drivers/ad9361/api/no_os/no_os_alloc.c
    drivers/ad9361/api/no_os/no_os_mutex.c
    drivers/ad9361/api/no_os/no_os_delay.c
    drivers/ad9361/api/no_os/no_os_util.c
    drivers/ad9361/api/no_os/no_os_spi.c
    drivers/ad9361/api/no_os/no_os_gpio.c
)
target_include_directories(ad9361_api_no_os PUBLIC drivers/ad9361/api/no_os)

add_library(ad9361_api_hal_platform
    drivers/ad9361/api/platform/hal_spi.cpp
    drivers/ad9361/api/platform/hal_gpio.cpp
)
target_include_directories(ad9361_api_hal_platform PUBLIC drivers/ad9361/api/platform)
target_link_libraries(ad9361_api_hal_platform PUBLIC ad9361_api_no_os hal_spi hal_gpio)

add_library(ad9361_no_os
    drivers/ad9361/api/ad9361.c
    drivers/ad9361/api/ad9361_api.c
    drivers/ad9361/api/ad9361_util.c
    drivers/ad9361/api/ad9361_ext_band_ctrl.c
    drivers/ad9361/api/ad9361_conv_stub.c
)
target_include_directories(ad9361_no_os PUBLIC drivers/ad9361/api)
target_link_libraries(ad9361_no_os PUBLIC ad9361_api_hal_platform m)

add_library(ad9361_transceiver
    drivers/ad9361_transceiver/src/default_init_param.c
    drivers/ad9361_transceiver/src/ad9361_shim.c
    drivers/ad9361_transceiver/src/ad9361_transceiver.cpp
)
target_include_directories(ad9361_transceiver PUBLIC drivers/ad9361_transceiver/inc)
target_link_libraries(ad9361_transceiver PUBLIC hal_spi PRIVATE ad9361_no_os)

add_library(drivers_ad9361_ctrl_gpio
    drivers/ad9361_ctrl_gpio/src/ad9361_ctrl_gpio.cpp
)
target_include_directories(drivers_ad9361_ctrl_gpio PUBLIC drivers/ad9361_ctrl_gpio/inc)
target_link_libraries(drivers_ad9361_ctrl_gpio PUBLIC hal_gpio)

add_library(drivers_dds_ctrl_gpio
    drivers/dds_ctrl_gpio/src/dds_ctrl_gpio.cpp
)
target_include_directories(drivers_dds_ctrl_gpio PUBLIC drivers/dds_ctrl_gpio/inc)
target_link_libraries(drivers_dds_ctrl_gpio PUBLIC hal_gpio)

add_library(project_iq_forge
    project/src/iq_forge.cpp
)
target_include_directories(project_iq_forge PUBLIC project/inc)
target_link_libraries(project_iq_forge PUBLIC drivers_ad9361 drivers_ad9361_ctrl_gpio drivers_dds_ctrl_gpio ad9361_transceiver hal_spi fpga::loader)

add_executable(iq_forge_app project/src/main.cpp)
target_link_libraries(iq_forge_app PRIVATE project_iq_forge)

install(TARGETS iq_forge_app RUNTIME DESTINATION bin)
