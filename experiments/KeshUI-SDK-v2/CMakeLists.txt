cmake_minimum_required(VERSION 3.24)
project(KeshUI_SDK_V2 VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KESHUI_BUILD_DEMO "Build KeshUI gallery" ON)
option(KESHUI_BUILD_TESTS "Build KeshUI tests" ON)

find_package(PkgConfig REQUIRED)
find_package(X11 REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(PANGO REQUIRED pangocairo)

add_library(keshui STATIC
    src/keshui.cpp
    src/backend_x11.cpp
)

target_include_directories(keshui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CAIRO_INCLUDE_DIRS}
        ${PANGO_INCLUDE_DIRS}
        ${X11_INCLUDE_DIR}
)

target_compile_options(keshui PRIVATE -Wall -Wextra -Wpedantic)
target_link_directories(keshui PRIVATE ${CAIRO_LIBRARY_DIRS} ${PANGO_LIBRARY_DIRS})
target_link_libraries(keshui PUBLIC ${X11_LIBRARIES} ${CAIRO_LIBRARIES} ${PANGO_LIBRARIES})
target_compile_definitions(keshui PUBLIC KESHUI_PLATFORM_X11=1)

add_executable(kxc tools/kxc/main.cpp)
target_compile_options(kxc PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(kxc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(KESHUI_BUILD_DEMO)
    set(KESHUI_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
    file(MAKE_DIRECTORY ${KESHUI_GENERATED_DIR})

    add_custom_command(
        OUTPUT ${KESHUI_GENERATED_DIR}/MainWindow.g.hpp
        COMMAND kxc
            ${CMAKE_CURRENT_SOURCE_DIR}/demo/MainWindow.xaml
            ${KESHUI_GENERATED_DIR}/MainWindow.g.hpp
        DEPENDS kxc ${CMAKE_CURRENT_SOURCE_DIR}/demo/MainWindow.xaml
        VERBATIM
        COMMENT "KXamlC: compiling MainWindow.xaml"
    )

    add_executable(keshui_gallery
        demo/main.cpp
        ${KESHUI_GENERATED_DIR}/MainWindow.g.hpp
    )
    target_include_directories(keshui_gallery PRIVATE ${KESHUI_GENERATED_DIR})
    target_link_libraries(keshui_gallery PRIVATE keshui)
    target_compile_options(keshui_gallery PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(KESHUI_BUILD_TESTS)
    enable_testing()
    add_test(NAME kxc_check COMMAND kxc --check ${CMAKE_CURRENT_SOURCE_DIR}/demo/MainWindow.xaml)
endif()

install(TARGETS keshui kxc
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
install(DIRECTORY include/keshui DESTINATION include)
