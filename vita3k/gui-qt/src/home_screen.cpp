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

#include <gui/functions.h>


#include <io/VitaIoDevice.h>

#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>

using namespace std::string_literals;

namespace gui_qt {

std::vector<std::string>::iterator get_live_area_current_open_apps_list_index(GuiState &gui, const std::string &app_path) {
    return std::find(gui.live_area_current_open_apps_list.begin(), gui.live_area_current_open_apps_list.end(), app_path);
}

}