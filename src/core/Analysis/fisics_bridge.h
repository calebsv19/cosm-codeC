#ifndef FISICS_BRIDGE_H
#define FISICS_BRIDGE_H

#include <stddef.h>
#include <stdbool.h>

#include "fisics_frontend.h"

struct OpenFile;

// Run Fisics analysis for the given in-memory buffer contents.
// filePath should be the absolute path to the buffer on disk.
void ide_analyze_buffer_for_file(const char* filePath, const char* contents, size_t length);

// Convenience: run analysis for an OpenFile (grabs a snapshot internally).
void ide_analyze_open_file(struct OpenFile* file);

// Optional accessors for future syntax highlighting / outline consumers.
// Returned pointers are owned by the bridge; valid until the next analysis call.
const FisicsTokenSpan* fisics_bridge_get_tokens(const char** filePathOut, size_t* countOut);
const FisicsSymbol*    fisics_bridge_get_symbols(const char** filePathOut, size_t* countOut);

#endif // FISICS_BRIDGE_H
