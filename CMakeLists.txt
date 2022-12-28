cmake_minimum_required(VERSION 3.22)

project(ecoBeeData VERSION 0.3.1.0)
set(PACKAGE_CONTACT "Richard Buckley <richard.buckley@ieee.org>")
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules" "${CMAKE_MODULE_PATH}")
find_package(CURLPP REQUIRED)
include_directories(${CURLPP_INCLUDE_DIR})

set(DAEMON_USER "daemon")
set(DAEMON_GROUP "daemon")
set(MONITOR_NAME "SYS_MONITOR")
set(ECO_BEE_CONFIG_DIR "${CMAKE_INSTALL_DATA}")

add_compile_options(-Wall -Wextra -pedantic -Werror -Wconversion)

add_executable(ecoBeeData
        src/ecoBeeData.cpp src/ConfigFile.cpp src/XDGFilePaths.cpp)

target_link_libraries(ecoBeeData
        stdc++fs
        ${CURLPP_LIBRARIES}
        )

# ecoBeeData
# Configure config
configure_file("resources/config.in" "resources/config.txt" NEWLINE_STYLE UNIX)
configure_file("resources/config.in" "resources/config.pkg" NEWLINE_STYLE UNIX)

install(TARGETS ecoBeeData
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT ECO_BEE_DATA EXCLUDE_FROM_ALL
        )

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/config.txt"
        "${CMAKE_CURRENT_BINARY_DIR}/resources/config.pkg"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ GROUP_WRITE
        COMPONENT ECO_BEE_DATA EXCLUDE_FROM_ALL
        )

set(CPACK_DEB_COMPONENT_INSTALL 1)
set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS 1)
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_RELEASE ${PROJECT_VERSION_TWEAK})
set(CPACK_GENERATOR "DEB")
#
set(CPACK_PACKAGE_ECO_BEE_DATA_EXECUTABLES ecoBeeData)
set(CPACK_DEBIAN_ECO_BEE_DATA_PACKAGE_NAME "${PROJECT_NAME}")
#set(CPACK_DEBIAN_ECO_BEE_DATA_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_BINARY_DIR}/resources/conffiles")
#
set(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
set(CPACK_RPM_PACKAGE_URL "https://github.com/pa28/echo_bee")
set(CPACK_RPM_PACKAGE_VENDOR "www.hrbuckley.net")
set(CPACK_PACKAGE_CONTACT "Richard Buckley")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        /usr/local
        /usr/local/bin
        /usr/local/share
        /usr/local/include
        /usr/lib/systemd
        /usr/lib/systemd/system
        )


include(CPack)

cpack_add_component(ECO_BEE_DATA
        DISPLAY_NAME "echo_bee"
        DESCRIPTION "CWOP Weather packet aggregator."
        )