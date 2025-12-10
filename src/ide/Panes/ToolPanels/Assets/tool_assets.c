#include "tool_assets.h"

#include "app/GlobalInfo/project.h"
#include "app/GlobalInfo/core_state.h"
#include "core/Clipboard/clipboard.h"

#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

static AssetCatalog gCatalog = {0};
static bool selected[1024];
static int flatCountCached = 0;

static void free_asset_entry(AssetEntry* e) {
    if (!e) return;
    free(e->name);
    free(e->relPath);
    free(e->absPath);
}

static void clear_category(AssetCategoryList* list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        free_asset_entry(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void clear_catalog(void) {
    for (int i = 0; i < ASSET_CATEGORY_COUNT; i++) {
        clear_category(&gCatalog.categories[i]);
    }
    gCatalog.totalCount = 0;
    memset(selected, 0, sizeof(selected));
}

static void ensure_capacity(AssetCategoryList* list) {
    if (list->count < list->capacity) return;
    int newCap = (list->capacity == 0) ? 32 : list->capacity * 2;
    AssetEntry* newItems = realloc(list->items, sizeof(AssetEntry) * newCap);
    if (!newItems) return;
    list->items = newItems;
    list->capacity = newCap;
}

static char* dup_string(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* out = malloc(len + 1);
    if (out) memcpy(out, s, len + 1);
    return out;
}

static AssetCategory classify_extension(const char* extLower) {
    if (!extLower) return ASSET_CATEGORY_OTHER;
    if (!strcmp(extLower, ".png") || !strcmp(extLower, ".jpg") || !strcmp(extLower, ".jpeg") ||
        !strcmp(extLower, ".bmp") || !strcmp(extLower, ".gif") || !strcmp(extLower, ".tga")) {
        return ASSET_CATEGORY_IMAGES;
    }
    if (!strcmp(extLower, ".wav") || !strcmp(extLower, ".mp3") || !strcmp(extLower, ".ogg") ||
        !strcmp(extLower, ".flac")) {
        return ASSET_CATEGORY_AUDIO;
    }
    if (!strcmp(extLower, ".json") || !strcmp(extLower, ".ini") || !strcmp(extLower, ".cfg") ||
        !strcmp(extLower, ".xml") || !strcmp(extLower, ".shader") || !strcmp(extLower, ".glsl") ||
        !strcmp(extLower, ".vert") || !strcmp(extLower, ".frag") || !strcmp(extLower, ".toml") ||
        !strcmp(extLower, ".yaml") || !strcmp(extLower, ".yml")) {
        return ASSET_CATEGORY_DATA;
    }
    return ASSET_CATEGORY_OTHER;
}

static bool is_code_extension(const char* extLower) {
    if (!extLower) return false;
    return (!strcmp(extLower, ".c")    || !strcmp(extLower, ".h")   ||
            !strcmp(extLower, ".cc")   || !strcmp(extLower, ".cpp") ||
            !strcmp(extLower, ".hpp")  || !strcmp(extLower, ".cxx") ||
            !strcmp(extLower, ".hh")   || !strcmp(extLower, ".m")   ||
            !strcmp(extLower, ".mm")   || !strcmp(extLower, ".o")   ||
            !strcmp(extLower, ".a")    || !strcmp(extLower, ".so")  ||
            !strcmp(extLower, ".dylib")|| !strcmp(extLower, ".dll") ||
            !strcmp(extLower, ".exe")  || !strcmp(extLower, ".obj"));
}

static char* to_lower_ext(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot) return NULL;
    size_t len = strlen(dot);
    char* lower = malloc(len + 1);
    if (!lower) return NULL;
    for (size_t i = 0; i < len; i++) {
        lower[i] = (char)tolower((unsigned char)dot[i]);
    }
    lower[len] = '\0';
    return lower;
}

static bool should_skip_dir(const char* name) {
    if (!name) return true;
    return (!strcmp(name, ".") || !strcmp(name, "..") ||
            !strcmp(name, ".git") || !strcmp(name, "ide_files") ||
            !strcmp(name, "build"));
}

static void add_asset(const char* workspace, const char* absPath, const char* relPath, const char* name, AssetCategory cat) {
    if (!absPath || !relPath || !name) return;
    AssetCategoryList* list = &gCatalog.categories[cat];
    ensure_capacity(list);
    if (list->count >= list->capacity) return;

    AssetEntry* e = &list->items[list->count++];
    e->name = dup_string(name);
    e->absPath = dup_string(absPath);
    e->relPath = dup_string(relPath);
    e->category = cat;
    list->category = cat;
    list->collapsed = false;
    gCatalog.totalCount++;
}

static void walk_dir(const char* workspace, const char* basePath, const char* relPrefix) {
    DIR* dir = opendir(basePath);
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (should_skip_dir(ent->d_name)) continue;

        char childAbs[PATH_MAX];
        if (snprintf(childAbs, sizeof(childAbs), "%s/%s", basePath, ent->d_name) >= (int)sizeof(childAbs)) {
            continue;
        }

        struct stat st;
        if (stat(childAbs, &st) != 0) continue;

        char childRel[PATH_MAX];
        if (relPrefix && *relPrefix) {
            snprintf(childRel, sizeof(childRel), "%s/%s", relPrefix, ent->d_name);
        } else {
            snprintf(childRel, sizeof(childRel), "%s", ent->d_name);
        }

        if (S_ISDIR(st.st_mode)) {
            walk_dir(workspace, childAbs, childRel);
        } else if (S_ISREG(st.st_mode)) {
            if (strcmp(ent->d_name, ".DS_Store") == 0) continue;
            char* extLower = to_lower_ext(childAbs);
            if (is_code_extension(extLower)) {
                free(extLower);
                continue;
            }
            AssetCategory cat = classify_extension(extLower);
            free(extLower);
            add_asset(workspace, childAbs, childRel, ent->d_name, cat);
        }
    }

    closedir(dir);
}

static int compare_entries(const void* a, const void* b) {
    const AssetEntry* ea = (const AssetEntry*)a;
    const AssetEntry* eb = (const AssetEntry*)b;
    if (!ea || !eb || !ea->relPath || !eb->relPath) return 0;
    return strcasecmp(ea->relPath, eb->relPath);
}

void initAssetManagerPanel(void) {
    clear_catalog();

    const char* workspace = getWorkspacePath();
    if (!workspace || !*workspace) {
        return;
    }

    walk_dir(workspace, workspace, "");

    for (int i = 0; i < ASSET_CATEGORY_COUNT; i++) {
        AssetCategoryList* list = &gCatalog.categories[i];
        list->category = (AssetCategory)i;
        list->collapsed = false;
        if (list->count > 1) {
            qsort(list->items, list->count, sizeof(AssetEntry), compare_entries);
        }
    }
}

const AssetCatalog* assets_get_catalog(void) {
    return &gCatalog;
}

bool assets_category_collapsed(AssetCategory cat) {
    if (cat < 0 || cat >= ASSET_CATEGORY_COUNT) return false;
    return gCatalog.categories[cat].collapsed;
}

void assets_toggle_collapse(AssetCategory cat) {
    if (cat < 0 || cat >= ASSET_CATEGORY_COUNT) return;
    gCatalog.categories[cat].collapsed = !gCatalog.categories[cat].collapsed;
}

static bool is_text_ext(const char* extLower) {
    if (!extLower) return false;
    return (!strcmp(extLower, ".json") || !strcmp(extLower, ".ini") || !strcmp(extLower, ".cfg") ||
            !strcmp(extLower, ".xml")  || !strcmp(extLower, ".shader") || !strcmp(extLower, ".glsl") ||
            !strcmp(extLower, ".vert") || !strcmp(extLower, ".frag")   || !strcmp(extLower, ".toml") ||
            !strcmp(extLower, ".yaml") || !strcmp(extLower, ".yml")    || !strcmp(extLower, ".txt")  ||
            !strcmp(extLower, ".csv")  || !strcmp(extLower, ".md")     || !strcmp(extLower, ".markdown") ||
            !strcmp(extLower, ".log"));
}

bool assets_is_text_like(const AssetEntry* e) {
    if (!e || !e->absPath) return false;
    char* ext = to_lower_ext(e->absPath);
    bool res = is_text_ext(ext);
    free(ext);
    if (res) return true;
    // Handle extension-less README-style files
    if (e->name) {
        const char* nm = e->name;
        if (strncasecmp(nm, "readme", 6) == 0) {
            return true;
        }
    }
    return false;
}

int assets_flatten(AssetFlatRef* out, int max) {
    int total = 0;
    for (int c = 0; c < ASSET_CATEGORY_COUNT && total < max; ++c) {
        AssetCategoryList* list = &gCatalog.categories[c];
        out[total].entry = NULL;
        out[total].category = (AssetCategory)c;
        out[total].indexInCategory = -1;
        out[total].isHeader = true;
        out[total].isMoreLine = false;
        total++;
        if (list->collapsed) continue;

        int toShow = list->count;
        if (toShow > ASSET_RENDER_LIMIT_PER_BUCKET) toShow = ASSET_RENDER_LIMIT_PER_BUCKET;
        for (int i = 0; i < toShow && total < max; ++i) {
            out[total].entry = &list->items[i];
            out[total].category = (AssetCategory)c;
            out[total].indexInCategory = i;
            out[total].isHeader = false;
            out[total].isMoreLine = false;
            total++;
        }
        if (list->count > ASSET_RENDER_LIMIT_PER_BUCKET && total < max) {
            out[total].entry = NULL;
            out[total].category = (AssetCategory)c;
            out[total].indexInCategory = -1;
            out[total].isHeader = false;
            out[total].isMoreLine = true;
            total++;
        }
    }
    flatCountCached = total;
    return total;
}

bool assets_is_selected(int flatIndex) {
    if (flatIndex < 0 || flatIndex >= (int)(sizeof(selected) / sizeof(selected[0]))) return false;
    return selected[flatIndex];
}

void assets_clear_selection(void) {
    memset(selected, 0, sizeof(selected));
}

void assets_select_toggle(int flatIndex, bool additive) {
    if (flatIndex < 0 || flatIndex >= (int)(sizeof(selected) / sizeof(selected[0]))) return;
    if (!additive) assets_clear_selection();
    selected[flatIndex] = !selected[flatIndex];
}

void assets_select_range(int a, int b) {
    if (a < 0 || b < 0) return;
    if (a > b) { int t = a; a = b; b = t; }
    assets_clear_selection();
    for (int i = a; i <= b && i < (int)(sizeof(selected) / sizeof(selected[0])); ++i) {
        selected[i] = true;
    }
}

void handleAssetManagerEvent(UIPane* pane, SDL_Event* event) {
    (void)pane; (void)event;
    // Future: selection and preview handling lives here.
}
