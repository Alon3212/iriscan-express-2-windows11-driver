cmake_minimum_required(VERSION 3.15)
project(ColorUSBScanner C)

if(NOT WIN32)
    message(FATAL_ERROR "This Data Source is Windows-only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Build in 32-bit mode (Visual Studio: -A Win32; MinGW: i686-w64-mingw32).")
endif()

set(SOURCES
    src/twain_ds.c
    src/libusb_dyn.c
    src/gt68xx_usb.c
    src/gt68xx_proto.c
)

add_library(ColorUSBScanner SHARED ${SOURCES})

target_include_directories(ColorUSBScanner PRIVATE
    src
    third_party
    third_party/libusb
)

target_compile_definitions(ColorUSBScanner PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(ColorUSBScanner PRIVATE kernel32 user32)

if(MSVC)
    target_link_options(ColorUSBScanner PRIVATE
        "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/ColorUSBScanner.def")
    target_compile_options(ColorUSBScanner PRIVATE /W3)
else()
    # MinGW: the symbol is _DS_Entry@20; --kill-at exports DS_Entry.
    target_link_options(ColorUSBScanner PRIVATE "-Wl,--kill-at")
    target_compile_options(ColorUSBScanner PRIVATE -Wall -Wextra)
endif()

set_target_properties(ColorUSBScanner PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ColorUSBScanner"
    SUFFIX ".ds"
)

# Local install (does not use the Windows installer).
install(TARGETS ColorUSBScanner RUNTIME DESTINATION ".")
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/PORTING_GT68XX.md"
    DESTINATION "."
)
