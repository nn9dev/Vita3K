
if(ANDROID)
    add_library(host_dialog STATIC
            src/filesystem_android.cpp
    )
elseif (IOS)
    add_library(host_dialog STATIC
            src/filesystem_ios.cpp
            #src/FilePicker.mm
    )
else()
    add_library(host_dialog STATIC
            src/filesystem_nfd.cpp
    )
endif()

# Create alias for more friendly naming and avoid conflicts
# in global CMake scope
add_library(host::dialog ALIAS host_dialog)

target_include_directories(host_dialog PUBLIC include)

target_link_libraries(host_dialog PUBLIC util)
if(ANDROID)
    target_link_libraries(host_dialog PRIVATE sdl2 android)
elseif (IOS)
    target_link_libraries(host_dialog PRIVATE "-framework Foundation" "-framework UIKit" "-framework MobileCoreServices")
    set_source_files_properties(filesystem_ios.mm PROPERTIES COMPILE_FLAGS "-ObjC++")
else()
    target_link_libraries(host_dialog PRIVATE NFDe::NFDe)
endif()