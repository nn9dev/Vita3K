add_library(
        gui-qt
        STATIC
        include/gui-qt/functions.h
        include/gui-qt/state.h
        include/gui-qt/qt_impl_sdl_state.h
        src/home_screen.cpp
        src/gui-qt.cpp
        # list files whenever we begin to add them
)
find_package(Qt5 COMPONENTS Core Widgets)

target_include_directories(gui-qt PUBLIC include ${CMAKE_SOURCE_DIR}/vita3k)
target_link_libraries(gui-qt PUBLIC app compat config dialog emuenv https ime imgui glutil lang regmgr np)
target_link_libraries(gui-qt PRIVATE audio ctrl kernel miniz psvpfsparser pugixml::pugixml stb renderer packages sdl2 touch vkutil host::dialog)
target_link_libraries(gui-qt PUBLIC tracy)
target_link_libraries(gui-qt PUBLIC vulkan)
target_link_libraries(gui-qt PUBLIC Qt5::Core Qt5::Widgets Qt5::Gui)
