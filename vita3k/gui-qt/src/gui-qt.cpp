// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <gui-qt/state.h>

#include <vulkan/vulkan.hpp>

#include <QKeyEvent>
#include <QApplication>
#include <QVulkanInstance>
#include <QWidget>

namespace gui_qt {

void pre_init(GuiState &gui, EmuEnvState &emuenv) {
#ifndef NDEBUG
    gui.inst.setLayers(QByteArrayList() << "VK_LAYER_LUNARG_standard_validation");
#endif

    gui.inst.create();
    gui.window.setVulkanInstance(&gui.inst);

    gui.window.setSurfaceType(QSurface::VulkanSurface);
    vk::SurfaceKHR surface = static_cast<vk::SurfaceKHR>(QVulkanInstance::surfaceForWindow(&gui.window));
    gui.window.resize(960, 540);
    gui.window.show();

    //gui.window;
    // initializes the gpu
    //emu->init(this,static_cast<vk::Instance>(gui.inst.vkInstance()), surface);

    //rendering = true;

    if (!gui.inst.create()) {
        qFatal("Failed to create Vulkan instance: %d", gui.inst.errorCode());
    }

    //ImGui::CreateContext();
    //gui.qt_state.reset()
    //gui.imgui_state.reset(ImGui_ImplSdl_Init(emuenv.renderer.get(), emuenv.window.get(), emuenv.base_path));

    //assert(gui.qt_state);

    //lang::init_lang(gui.lang, emuenv);

    //bool result = ImGui_ImplSdl_CreateDeviceObjects(gui.qt_state.get());
    //assert(result);
}

} // namespace gui