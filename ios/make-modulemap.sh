#!/usr/bin/env bash
set -euo pipefail

exclude_components=(
    "android"
    "gui-qt" 
    "ios"
)

exclude_headers=(
    "cubeb_audio.h"
)

# "old_name:new_name"
substitute_components=(
    "module:modutils" # i hate having to do this
    "interface.h:interface"
    "archive.h:archive"
    "platform_api.h:platform_api"
    "resource.h:resource"
)

is_excluded() {
    local target="$1"
    for excl in "${exclude_components[@]+"${exclude_components[@]}"}"; do
        [[ "$excl" == "$target" ]] && return 0
    done
    return 1
}

is_excluded_header() {
    local target="$1"
    local filename="${target##*/}"
    for excl in "${exclude_headers[@]+"${exclude_headers[@]}"}"; do
        [[ "$excl" == "$filename" ]] && return 0
    done
    return 1
}

substitute_name() {
    local target="$1"
    for sub in "${substitute_components[@]+"${substitute_components[@]}"}"; do
        local orig="${sub%%:*}"
        local repl="${sub#*:}"
        [[ "$orig" == "$target" ]] && { echo "$repl"; return; }
    done
    echo "$target"
}


# main
header_array=( $(find . -name *.h -path '*/vita3k/*') )
harray_2=( $(find . -name *.hpp -path '*/external/*'))

if [[ ${#header_array[@]} -eq 0 ]]; then
    echo "No matching files found."
    exit 1
fi

# collect component names via bash parameter expansion magic
components=()
for path in "${header_array[@]}"; do
    clean_path="${path#./}"
    component="${clean_path#vita3k/}"
    component="${component%%/*}"
    components+=("$component")
done
unique_components=( $(printf '%s\n' "${components[@]}" | sort -u) )

# remove old file if it exists
if [[ -f "module.modulemap" ]]; then
    rm "module.modulemap"
fi

# craft the module map via more parameter expansion magic
outfile="module.modulemap"

{
    echo "module vita3k {"
    #echo "  requires cplusplus"
    echo ""

    for component in "${unique_components[@]}"; do
        # skip excluded components
        if is_excluded "$component"; then
            echo "skipped component: ${component}" >&2
            continue
        fi

        module_name="$(substitute_name "$component")"

        echo "  module ${module_name} {"
        for path in "${header_array[@]}"; do
            clean_path="${path#./}"
            c="${clean_path#vita3k/}"
            c="${c%%/*}"
            if [[ "$c" == "$component" ]]; then
                if is_excluded_header "$clean_path"; then
                    echo "skipped header: ${clean_path}" >&2
                    continue
                fi
                echo "    header \"${clean_path}\""
            fi
        done
        echo "    export *"
        echo "  }"
        echo ""
    done

    echo "  export *"
    echo "}"
} > "$outfile"

echo "Wrote ${outfile}"