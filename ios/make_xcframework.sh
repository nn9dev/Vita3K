#!/bin/bash
set -e
# makes an xcframework out of the ios version + the simulator version

#BUILD_TYPE="RelWithDebInfo"
BUILD_TYPE="Debug"
#BUILD_TYPE="Release"

for arg in "$@"; do
    case "$arg" in
        nobuild)        NOBUILD=1  ;;
        noconfig)       NOCONFIG=1 ;;
        noios)          NOIOS=1    ;;
        nosim)          NOSIM=1    ;;
        debug)          BUILD_TYPE="Debug"    ;;
        release)        BUILD_TYPE="Release"  ;;
        relwithdebinfo) BUILD_TYPE="RelWithDebInfo" ;;
        *)        echo "Unknown argument: $arg" ;;
    esac
done

echo "0$NOBUILD" "0$NOCONFIG" "0$NOIOS" "0$NOSIM"
#exit

if [ "$NOCONFIG" != "1" ]; then
    if [ "$NOIOS" != "1" ]; then
        cmake --preset ios-xcode -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    fi
    if [ "$NOSIM" != "1" ]; then
        cmake --preset ios-simulator-xcode -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    fi
fi
if [ "$NOBUILD" != "1" ]; then
    if [ "$NOIOS" != "1" ]; then
        cmake --build build/ios-xcode --target vita3k --config "$BUILD_TYPE"
    fi
    if [ "$NOSIM" != "1" ]; then
        cmake --build build/ios-simulator-xcode --target vita3k --config "$BUILD_TYPE"
    fi
fi

if [ -d "build/Vita3K.xcframework" ]; then
    rm -rf build/Vita3K.xcframework
fi

xcodebuild -create-xcframework \
  -framework "build/ios-xcode/vita3k/$BUILD_TYPE-iphoneos/Vita3K.framework" \
  -framework "build/ios-simulator-xcode/vita3k/$BUILD_TYPE-iphonesimulator/Vita3K.framework" \
  -output "build/Vita3K.xcframework"
