#define _DARWIN_C_SOURCE
#define _XOPEN_SOURCE 700
#include "app/GlobalInfo/project.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Isolate the directory loader from runtime preferences/watch registration. */
void setWorkspacePath(const char *path) { (void)path; }
const char *getWorkspacePath(void) { return NULL; }
void setWorkspaceWatchPath(const char *path) { (void)path; }
void saveWorkspacePreference(const char *path) { (void)path; }
const char *loadWorkspacePreference(void) { return NULL; }

static DirEntry *child(DirEntry *root, const char *name) {
    for (int i = 0; i < root->childCount; ++i)
        if (strcmp(root->children[i]->name, name) == 0) return root->children[i];
    return NULL;
}

int main(void) {
    char path[] = "/tmp/ide-project-scan-XXXXXX";
    assert(mkdtemp(path));
    char source[1024], file[1024], build[1024], generated[1024], loop[1024], linked[1024];
    snprintf(source, sizeof(source), "%s/src", path);
    snprintf(file, sizeof(file), "%s/src/lesson.c", path);
    snprintf(build, sizeof(build), "%s/build", path);
    snprintf(generated, sizeof(generated), "%s/build/generated.c", path);
    snprintf(loop, sizeof(loop), "%s/src/back-to-root", path);
    snprintf(linked, sizeof(linked), "%s/src/linked.c", path);
    assert(mkdir(source, 0700) == 0 && mkdir(build, 0700) == 0);
    FILE *f = fopen(file, "w"); assert(f); fputs("int main(void) { return 0; }", f); fclose(f);
    f = fopen(generated, "w"); assert(f); fclose(f);
    assert(symlink(path, loop) == 0);
    assert(symlink(file, linked) == 0);
    DirEntry *root = loadProjectDirectory(path); assert(root);
    DirEntry *src = child(root, "src"), *output = child(root, "build");
    assert(src && src->parent == root && child(src, "lesson.c"));
    assert(!child(src, "back-to-root"));
    assert(child(src, "linked.c"));
    assert(output && output->childCount == 0);
    destroyProjectDirectory(root);
    unlink(loop); unlink(linked); unlink(file); unlink(generated); rmdir(source); rmdir(build); rmdir(path);
    puts("project_directory_scan_test: success");
}
