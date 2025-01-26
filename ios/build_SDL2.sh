#!/bin/bash

# Build SDL2 for iOS
# Adapted from: https://marcelbraghetto.github.io/a-simple-triangle/2019/03/09/part-04/

# This method will compile a static library from an Xcode project if it doesn't already exist in the Libs folder.
create_static_library() {
    # The following arguments need to be passed into this method:
    STATIC_LIBRARY=$1
    PROJECT_PATH=$2
    XCODE_PROJECT=$3
    XCODE_TARGET=$4
    BUILD_FOLDER=$5

    # Make sure the Libs folder exists.
    if [ ! -d "../ios/Libs" ]; then
        mkdir "../ios/Libs"
    fi

    # If the static library file doesn't exist, we'll make it.
    if [ ! -e $STATIC_LIBRARY ]; then

        # Navigate to the path containing the Xcode project.
        pushd $PROJECT_PATH
            # Build the iPhone library.
            echo "Building the iOS iPhone static library ..."

            xcrun xcodebuild -configuration "Release" \
                -project $XCODE_PROJECT \
                -target "$XCODE_TARGET" \
                -sdk "iphoneos" \
                build \
                ONLY_ACTIVE_ARCH=NO \
                RUN_CLANG_STATIC_ANALYZER=NO \
                BUILD_DIR="build/$BUILD_FOLDER" \
                SYMROOT="build/$BUILD_FOLDER" \
                OBJROOT="build/$BUILD_FOLDER/obj" \
                BUILD_ROOT="build/$BUILD_FOLDER" \
                TARGET_BUILD_DIR="build/$BUILD_FOLDER/iphoneos"

            # Build the simulator library.
            echo "Building the iOS Simulator static library ..."

            xcrun xcodebuild -configuration "Release" \
                -project $XCODE_PROJECT \
                -target "$XCODE_TARGET" \
                -sdk "iphonesimulator" \
                build \
                ONLY_ACTIVE_ARCH=NO \
                RUN_CLANG_STATIC_ANALYZER=NO \
                BUILD_DIR="build/$BUILD_FOLDER" \
                SYMROOT="build/$BUILD_FOLDER" \
                OBJROOT="build/$BUILD_FOLDER/obj" \
                BUILD_ROOT="build/$BUILD_FOLDER" \
                TARGET_BUILD_DIR="build/$BUILD_FOLDER/iphonesimulator"

            # Join both libraries into one 'fat' library.
            echo "Creating fat library ..."

            xcrun -sdk iphoneos lipo -create \
                -output "build/$BUILD_FOLDER/$STATIC_LIBRARY" \
                "build/$BUILD_FOLDER/iphoneos/$STATIC_LIBRARY" \
                "build/$BUILD_FOLDER/iphonesimulator/$STATIC_LIBRARY"

        echo ""

        popd  # pop $PROJECT_PATH

        if [ -e "$PROJECT_PATH/build/$BUILD_FOLDER/$STATIC_LIBRARY" ]; then
            echo "The fat static library '$STATIC_LIBRARY' is ready."
            echo "Copying '$STATIC_LIBRARY' into Libs."
            cp "$PROJECT_PATH/build/$BUILD_FOLDER/$STATIC_LIBRARY" "../ios/Libs/$STATIC_LIBRARY"
        else
            echo "The fat library '$STATIC_LIBRARY' was not created. Assuming same arch (arm64)."
            echo "Copying '$STATIC_LIBRARY' into Libs."
            cp "$PROJECT_PATH/build/$BUILD_FOLDER/iphoneos/$STATIC_LIBRARY" "../ios/Libs/$STATIC_LIBRARY"
        fi

    fi
}


create_static_library \
    libSDL2.a \
    ../ios/SDL2/Xcode/SDL \
    SDL.xcodeproj \
    "Static Library-iOS" \
    SDL2