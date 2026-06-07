#!/bin/bash

CONFIG=${1:-"Debug-iphonesimulator"}
BUILD_DIR="/Users/afternoon/Documents/ex-code/Vita3K/build/ios-xcode"
OUTPUT="$BUILD_DIR/libVita3KFull.a"

main_libs=("libVita3K.a" "libapp.a" "libaudio.a" "libcamera.a" "libcodec.a" "libcompat.a" "libconfig.a" "libcpu.a" "libctrl.a" "libdisplay.a" "libemuenv.a" "libgdbstub.a" "libglutil.a" "libgxm.a" "libhttp.a" "libime.a" "libinput.a" "libio.a" "libkernel.a" "liblang.a" "libmem.a" "libmodule.a" "libmodules.a" "libmotion.a" "libnet.a" "libngs.a" "libnids.a" "libnp.a" "liboverlay.a" "libpackages.a" "libpatch.a" "libregmgr.a" "librenderer.a" "librtc.a" "libshader.a" "libtouch.a" "libupdater.a" "libutil.a" "libvkutil.a")
external_libs=("libcppcommon.a" "libminiz.a" "libpsvpfsparser.a" "libcapstone.a" "libdlmalloc.a" "libdynarmic.a" "libfmtd.a" "libglad.a" "libglslang.a" "liblibatrac9.a" "libFAT16.a" "liblibzRIF.a" "liblibb64.a" "libmcl.a" "libpugixml.a" "libspdlogd.a" "libSPIRV.a" "libspirv-cross-core.a" "libspirv-cross-glsl.a" "libsubstitute.a" "libxxhash.a" "libyaml-cppd.a" "libtracy.a")
#vcpkg_libs=()
vcpkg_libs=("libcrypto.a" "libboost_container.a" "libboost_program_options.a" "libz.a" "libcurl-d.a" "libboost_atomic.a" "libboost_date_time.a" "libssl.a" "libboost_filesystem.a")
#ffmpeg_libs=()
ffmpeg_libs=("libavutil.a" "libavfilter.a" "libavcodec.a" "libavformat.a" "libavdevice.a" "libswresample.a" "libswscale.a")

found_libs=()

for lib in "${main_libs[@]}"; do
    path=$(find "$BUILD_DIR" -name "$lib" -path "*$CONFIG*" ! -path "*/Objects-normal/*" | head -1)
    if [ -n "$path" ]; then
        echo "Found: $path"
        found_libs+=("$path")
    else
        echo "WARNING: $lib not found for config $CONFIG"
    fi
done

for lib in "${external_libs[@]}"; do
    path=$(find "$BUILD_DIR" -name "$lib" -path "*$CONFIG*" ! -path "*/Objects-normal/*" | head -1)
    if [ -n "$path" ]; then
        echo "Found: $path"
        found_libs+=("$path")
    else
        # fall back to any match if no config-specific one exists
        path=$(find "$BUILD_DIR" -name "$lib" ! -path "*/Objects-normal/*" ! -path "*/debug/*" | head -1)
        if [ -n "$path" ]; then
            echo "Found (no config match, using fallback): $path"
            found_libs+=("$path")
        else
            echo "WARNING: $lib not found"
        fi
    fi
done

for lib in "${vcpkg_libs[@]}"; do
    path=$(find "$BUILD_DIR" -name "$lib" -path "*$CONFIG*" ! -path "*/Objects-normal/*" | head -1)
    if [ -n "$path" ]; then
        echo "Found: $path"
        found_libs+=("$path")
    else
        # fall back to any match if no config-specific one exists
        path=$(find "$BUILD_DIR" -name "$lib" ! -path "*/Objects-normal/*" ! -path "*/debug/*" | head -1)
        if [ -n "$path" ]; then
            echo "Found (no config match, using fallback): $path"
            found_libs+=("$path")
        else
            echo "WARNING: $lib not found"
        fi
    fi
done

for lib in "${ffmpeg_libs[@]}"; do
    path=$(find "$BUILD_DIR" -name "$lib" -path "*$CONFIG*" ! -path "*/Objects-normal/*" | head -1)
    if [ -n "$path" ]; then
        echo "Found: $path"
        found_libs+=("$path")
    else
        # fall back to any match if no config-specific one exists
        path=$(find "$BUILD_DIR" -name "$lib" ! -path "*/Objects-normal/*" ! -path "*/debug/*" | head -1)
        if [ -n "$path" ]; then
            echo "Found (no config match, using fallback): $path"
            found_libs+=("$path")
        else
            echo "WARNING: $lib not found"
        fi
    fi
done

echo ""
echo "Merging ${#found_libs[@]} libraries into $OUTPUT..."
libtool -static -o "$OUTPUT" "${found_libs[@]}"

echo "Stripping debug symbols..."
strip -S "$OUTPUT"

echo "Done: $OUTPUT ($(du -sh "$OUTPUT" | cut -f1))"