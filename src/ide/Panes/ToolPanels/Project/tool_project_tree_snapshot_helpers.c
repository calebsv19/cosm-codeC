#include "tool_project_tree_snapshot_helpers.h"

#include <stdlib.h>
#include <string.h>

typedef struct ProjectTextBuilder {
    char* data;
    size_t len;
    size_t cap;
} ProjectTextBuilder;

static bool project_builder_reserve(ProjectTextBuilder* b, size_t extra) {
    if (!b) return false;
    {
        size_t need = b->len + extra + 1;
        if (need <= b->cap) return true;
        {
            size_t nextCap = b->cap > 0 ? b->cap : 256;
            while (nextCap < need) nextCap *= 2;
            {
                char* next = (char*)realloc(b->data, nextCap);
                if (!next) return false;
                b->data = next;
                b->cap = nextCap;
                return true;
            }
        }
    }
}

static bool project_builder_append(ProjectTextBuilder* b, const char* text) {
    if (!b || !text) return false;
    {
        size_t n = strlen(text);
        if (!project_builder_reserve(b, n)) return false;
        memcpy(b->data + b->len, text, n);
        b->len += n;
        b->data[b->len] = '\0';
        return true;
    }
}

static bool project_builder_append_char(ProjectTextBuilder* b, char ch) {
    if (!project_builder_reserve(b, 1)) return false;
    b->data[b->len++] = ch;
    b->data[b->len] = '\0';
    return true;
}

const char* project_tree_display_name(const DirEntry* entry) {
    if (!entry || !entry->path) return "";
    {
        const char* slash = strrchr(entry->path, '/');
        return slash ? (slash + 1) : entry->path;
    }
}

static bool project_snapshot_skip_entry(const DirEntry* entry) {
    if (!entry) return true;
    {
        const char* name = project_tree_display_name(entry);
        if (!name || !name[0]) return true;
        if (entry->parent == NULL) return false;
        if (name[0] == '.' && strcmp(name, "..") != 0) return true;
        if (entry->type == ENTRY_FILE) {
            const char* ext = strrchr(name, '.');
            if (ext && (strcmp(ext, ".o") == 0 ||
                        strcmp(ext, ".obj") == 0 ||
                        strcmp(ext, ".out") == 0)) {
                return true;
            }
            if (strcmp(name, "last_build") == 0) return true;
        }
        return false;
    }
}

static bool project_append_visible_entry(ProjectTextBuilder* b, const DirEntry* entry, int depth) {
    if (!b || !entry) return true;
    if (project_snapshot_skip_entry(entry)) return true;

    {
        const char* name = project_tree_display_name(entry);
        for (int i = 0; i < depth; ++i) {
            if (!project_builder_append(b, "  ")) return false;
        }
        if (entry->type == ENTRY_FOLDER) {
            if (!project_builder_append(b, entry->isExpanded ? "[-] " : "[+] ")) return false;
        }
        if (!project_builder_append(b, name ? name : "")) return false;
        if (!project_builder_append_char(b, '\n')) return false;

        if (entry->type == ENTRY_FOLDER && entry->isExpanded) {
            for (int i = 0; i < entry->childCount; ++i) {
                if (!project_append_visible_entry(b, entry->children[i], depth + 1)) return false;
            }
        }
        return true;
    }
}

bool project_tree_build_visible_snapshot(const DirEntry* root, char** out_text, size_t* out_len) {
    if (!root || !out_text) return false;
    *out_text = NULL;
    if (out_len) *out_len = 0;

    {
        ProjectTextBuilder b = {0};
        if (!project_builder_reserve(&b, 512)) return false;
        b.data[0] = '\0';
        if (!project_append_visible_entry(&b, root, 0) || b.len == 0) {
            free(b.data);
            return false;
        }
        *out_text = b.data;
        if (out_len) *out_len = b.len;
        return true;
    }
}
