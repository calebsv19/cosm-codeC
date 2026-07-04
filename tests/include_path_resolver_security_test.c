#include "core/Analysis/include_path_resolver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int expect(int condition, const char* message) {
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

static int write_text_file(const char* path, const char* text) {
    FILE* f = fopen(path, "w");
    if (!f) return 1;
    fputs(text ? text : "", f);
    fclose(f);
    return 0;
}

static int contains_path(const BuildFlagSet* set, const char* path) {
    if (!set || !path) return 0;
    for (size_t i = 0; i < set->include_count; ++i) {
        if (set->include_paths[i] && strcmp(set->include_paths[i], path) == 0) {
            return 1;
        }
    }
    return 0;
}

static int contains_macro(const BuildFlagSet* set, const char* macro) {
    if (!set || !macro) return 0;
    for (size_t i = 0; i < set->macro_count; ++i) {
        if (set->macro_defines[i] && strcmp(set->macro_defines[i], macro) == 0) {
            return 1;
        }
    }
    return 0;
}

static int write_manifest(const char* root) {
    char path[512];
    snprintf(path, sizeof(path), "%s/project.fisics.json", root);
    return write_text_file(path,
                           "{\n"
                           "  \"schema\": \"fisiCs.project\",\n"
                           "  \"version\": 0,\n"
                           "  \"name\": \"resolver_test\",\n"
                           "  \"defaults\": {\n"
                           "    \"include_dirs\": [\"manifest_include\"],\n"
                           "    \"defines\": [\"MANIFEST_FLAG=1\"],\n"
                           "    \"overlays\": [\"physics-units\"]\n"
                           "  },\n"
                           "  \"translation_units\": [{\"source\":\"src/main.c\"}]\n"
                           "}\n");
}

int main(void) {
    char root[256];
    snprintf(root, sizeof(root), "/tmp/ide_include_path_\"_security_%ld", (long)getpid());
    if (expect(mkdir(root, 0755) == 0, "expected workspace mkdir")) return 1;

    char include_dir[512];
    snprintf(include_dir, sizeof(include_dir), "%s/include", root);
    if (expect(mkdir(include_dir, 0755) == 0, "expected include mkdir")) return 1;
    char manifest_include_dir[512];
    snprintf(manifest_include_dir, sizeof(manifest_include_dir), "%s/manifest_include", root);
    if (expect(mkdir(manifest_include_dir, 0755) == 0, "expected manifest include mkdir")) return 1;

    char marker[256];
    snprintf(marker, sizeof(marker), "/tmp/ide_include_path_resolver_marker_%ld", (long)getpid());
    unlink(marker);

    char makefile[512];
    snprintf(makefile, sizeof(makefile), "%s/Makefile", root);
    char makefile_body[1024];
    snprintf(makefile_body,
             sizeof(makefile_body),
             "CFLAGS = -Iinclude -DSTATIC_TEXT_FLAG=1\n"
             "DANGER := $(shell touch %s)\n",
             marker);
    if (expect(write_text_file(makefile, makefile_body) == 0,
               "expected Makefile write")) return 1;
    if (expect(write_manifest(root) == 0, "expected manifest write")) return 1;

    unsetenv("IDE_TRUST_WORKSPACE_MAKE_VARS");

    BuildFlagSet flags = {0};
    gather_build_flags(root, "-Iextra_include -DEXTRA_FLAG=1", &flags);

    char expected_include[512];
    snprintf(expected_include, sizeof(expected_include), "%s/include", root);
    char expected_extra[512];
    snprintf(expected_extra, sizeof(expected_extra), "%s/extra_include", root);
    char expected_manifest_include[512];
    snprintf(expected_manifest_include, sizeof(expected_manifest_include), "%s/manifest_include", root);

    int failed = 0;
    failed |= expect(contains_path(&flags, expected_include),
                     "expected textual Makefile include path");
    failed |= expect(contains_path(&flags, expected_extra),
                     "expected extra flags include path");
    failed |= expect(contains_macro(&flags, "STATIC_TEXT_FLAG=1"),
                     "expected textual Makefile macro");
    failed |= expect(contains_macro(&flags, "EXTRA_FLAG=1"),
                     "expected extra flags macro");
    failed |= expect(contains_path(&flags, expected_manifest_include),
                     "expected manifest include path");
    failed |= expect(contains_macro(&flags, "MANIFEST_FLAG=1"),
                     "expected manifest macro");
    failed |= expect((flags.overlay_features & BUILD_FLAG_OVERLAY_PHYSICS_UNITS) != 0,
                     "expected manifest physics-units overlay");
    failed |= expect(access(marker, F_OK) != 0,
                     "passive analysis must not execute Makefile shell expansion");

    BuildFlagSet source_flags = {0};
    build_flags_enable_overlays_for_source(&source_flags,
                                           "double x [[fisics::dim(length)]] = 1.0;",
                                           strlen("double x [[fisics::dim(length)]] = 1.0;"));
    failed |= expect((source_flags.overlay_features & BUILD_FLAG_OVERLAY_PHYSICS_UNITS) != 0,
                     "expected source annotation physics-units overlay");

    free_build_flag_set(&flags);
    char manifest_path[512];
    snprintf(manifest_path, sizeof(manifest_path), "%s/project.fisics.json", root);
    unlink(marker);
    unlink(manifest_path);
    unlink(makefile);
    rmdir(manifest_include_dir);
    rmdir(include_dir);
    rmdir(root);
    return failed ? 1 : 0;
}
