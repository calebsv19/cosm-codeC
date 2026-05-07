#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <app_bin> <frameworks_dir>" >&2
    exit 1
fi

APP_BIN="$1"
FRAMEWORKS_DIR="$2"
SEARCH_ROOTS_LIST="${PACKAGE_DEP_SEARCH_ROOTS:-/opt/homebrew:/usr/local}"
AWK_BIN="/usr/bin/awk"
BASENAME_BIN="/usr/bin/basename"
CHMOD_BIN="/bin/chmod"
CP_BIN="/bin/cp"
GREP_BIN="/usr/bin/grep"
INSTALL_NAME_TOOL_BIN="/usr/bin/install_name_tool"
MKDIR_BIN="/bin/mkdir"
OTOOL_BIN="/usr/bin/otool"

"$MKDIR_BIN" -p "$FRAMEWORKS_DIR"

resolve_search_root_dep() {
    dep_name="$1"
    old_ifs="${IFS}"
    IFS=':'
    for root in $SEARCH_ROOTS_LIST; do
        [ -n "$root" ] || continue
        if [ -f "$root/lib/$dep_name" ]; then
            printf '%s\n' "$root/lib/$dep_name"
            IFS="${old_ifs}"
            return 0
        fi
    done
    IFS="${old_ifs}"
    return 1
}

replacement_path_for() {
    current_file="$1"
    dep_base="$2"
    case "$current_file" in
        */Contents/MacOS/*)
            printf '%s\n' "@executable_path/../Frameworks/$dep_base"
            ;;
        */Contents/Resources/*)
            printf '%s\n' "@loader_path/../Frameworks/$dep_base"
            ;;
        */Contents/Frameworks/*)
            printf '%s\n' "@loader_path/$dep_base"
            ;;
        *)
            printf '%s\n' "@loader_path/$dep_base"
            ;;
    esac
}

WORK_TMP_DIR="${TMPDIR:-/tmp}/ide_bundle_dylibs.$$"
"$MKDIR_BIN" -p "$WORK_TMP_DIR"
QUEUE_FILE="$WORK_TMP_DIR/queue.txt"
SEEN_FILE="$WORK_TMP_DIR/seen.txt"
touch "$QUEUE_FILE" "$SEEN_FILE"

cleanup() {
    rm -rf "$WORK_TMP_DIR" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

echo "$APP_BIN" >>"$QUEUE_FILE"

while IFS= read -r current_file; do
    [ -n "$current_file" ] || continue
    if "$GREP_BIN" -Fxq "$current_file" "$SEEN_FILE"; then
        continue
    fi
    echo "$current_file" >>"$SEEN_FILE"

    deps="$("$OTOOL_BIN" -L "$current_file" | "$AWK_BIN" 'NR>1 {print $1}' | "$GREP_BIN" -E '^/opt/homebrew|^/usr/local|^@rpath/' || true)"
    if [ -z "$deps" ]; then
        continue
    fi

    echo "$deps" | while IFS= read -r dep; do
        [ -n "$dep" ] || continue
        dep_base="$("$BASENAME_BIN" "$dep")"
        dep_dst="$FRAMEWORKS_DIR/$dep_base"
        dep_src="$dep"

        case "$dep" in
            @rpath/*)
                if [ -f "$FRAMEWORKS_DIR/$dep_base" ]; then
                    dep_src="$FRAMEWORKS_DIR/$dep_base"
                elif dep_src="$(resolve_search_root_dep "$dep_base" 2>/dev/null)"; then
                    :
                else
                    echo "warning: unable to resolve $dep for $current_file" >&2
                    continue
                fi
                ;;
        esac

        if [ ! -f "$dep_dst" ]; then
            "$CP_BIN" -fL "$dep_src" "$dep_dst"
            "$CHMOD_BIN" u+w "$dep_dst"
            "$INSTALL_NAME_TOOL_BIN" -id "@loader_path/$dep_base" "$dep_dst" || true
            echo "$dep_dst" >>"$QUEUE_FILE"
        fi

        replacement="$(replacement_path_for "$current_file" "$dep_base")"
        "$INSTALL_NAME_TOOL_BIN" -change "$dep" "$replacement" "$current_file"
    done
done <"$QUEUE_FILE"

for required in libMoltenVK.dylib libvulkan.1.dylib; do
    if dep_src="$(resolve_search_root_dep "$required" 2>/dev/null)"; then
        dep_dst="$FRAMEWORKS_DIR/$required"
        if [ ! -f "$dep_dst" ]; then
            "$CP_BIN" -fL "$dep_src" "$dep_dst"
            "$CHMOD_BIN" u+w "$dep_dst"
        fi
        "$INSTALL_NAME_TOOL_BIN" -id "@loader_path/$required" "$dep_dst" || true
    fi
done

exit 0
