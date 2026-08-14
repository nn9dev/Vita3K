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

#if defined(TARGET_OS_OSX)
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

extern "C" void *get_metal_layer_from_view(void *nsview) {
    return (__bridge void *)((__bridge NSView *)nsview).layer;
}

#else if defined(TARGET_OS_IOS)
#import <UIKit/UIKit.h>
#import <QuartzCore/CAMetalLayer.h>

extern "C" void *get_metal_layer_from_view(void *uiview) {
    return (__bridge void *)((__bridge UIView *)uiview).layer;
}

extern "C" void get_view_drawable_size(void *uiview, int *out_width, int *out_height) {
    UIView *view = (__bridge UIView *)uiview;
    const CGSize points = view.bounds.size;
    const CGFloat scale = view.contentScaleFactor > 0 ? view.contentScaleFactor : [UIScreen mainScreen].scale;
    if (out_width) *out_width = (int)(points.width * scale);
    if (out_height) *out_height = (int)(points.height * scale);
}
#endif
