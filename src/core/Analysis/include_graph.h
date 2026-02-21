#ifndef INCLUDE_GRAPH_H
#define INCLUDE_GRAPH_H

#include <stddef.h>

#include "fisics_frontend.h"

void include_graph_clear(void);
void include_graph_remove_source(const char* source_path);

void include_graph_replace_from_result(const char* source_path,
                                       const FisicsInclude* includes,
                                       size_t include_count,
                                       const char* workspace_root);

size_t include_graph_collect_dependents(const char* changed_path, char*** out_paths);
void include_graph_free_path_list(char** paths, size_t count);

void include_graph_save(const char* workspace_root);
void include_graph_load(const char* workspace_root);

#endif // INCLUDE_GRAPH_H
