cmake_minimum_required(VERSION 3.16)

project(iCloud-Linux)

set(CMAKE_CXX_STANDARD 17)

set(BINARY_NAME "icloud-linux")

add_executable(
    ${BINARY_NAME}
    src/main.cpp
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(gtk4   REQUIRED gtk4          IMPORTED_TARGET)
    pkg_check_modules(webkit REQUIRED webkitgtk-6.0 IMPORTED_TARGET)
    pkg_check_modules(adwaita REQUIRED libadwaita-1 IMPORTED_TARGET)
    target_link_libraries(${BINARY_NAME} PUBLIC PkgConfig::gtk4 PkgConfig::webkit PkgConfig::adwaita)
else()
    target_link_libraries(${BINARY_NAME} "-framework WebKit")
endif()

install(TARGETS ${BINARY_NAME} DESTINATION bin)


