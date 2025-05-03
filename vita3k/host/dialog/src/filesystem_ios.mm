//
// Created by good afternoon on 12/11/24.
//

// FilePicker.mm - Objective-C++
#include <Cocoa/Cocoa.h>
#include <iostream>

class FilePicker {
public:
    void openFilePicker() {
        @autoreleasepool {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            [panel setCanChooseFiles:YES];
            [panel setCanChooseDirectories:NO];
            [panel setAllowsMultipleSelection:NO];

            if ([panel runModal] == NSModalResponseOK) {
                NSURL *selectedFile = [panel URL];
                NSString *filePath = [selectedFile path];
                std::cout << "Selected file: " << [filePath UTF8String] << std::endl;
            } else {
                std::cout << "No file selected." << std::endl;
            }
        }
    }
};
