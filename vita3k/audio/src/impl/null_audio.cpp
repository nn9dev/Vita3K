// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include "audio/impl/null_audio.h"

#include <util/log.h>

NullAudioOutPort::~NullAudioOutPort() {}

NullAudioAdapter::NullAudioAdapter(AudioState &audio_state)
    : AudioAdapter(audio_state) {}

NullAudioAdapter::~NullAudioAdapter() {}

bool NullAudioAdapter::init() {
    return true;
}

AudioOutPortPtr NullAudioAdapter::open_port(int nb_channels, int freq, int nb_sample) {
    return std::make_shared<NullAudioOutPort>();
}

void NullAudioAdapter::audio_output(AudioOutPort &out_port, const void *buffer) {}

void NullAudioAdapter::set_volume(AudioOutPort &out_port, float volume) {}

void NullAudioAdapter::switch_state(const bool pause) {}

void NullAudioAdapter::wake_all_ports() {}