#ifndef IDE_IPC_PRESENTATION_H
#define IDE_IPC_PRESENTATION_H

#include <stdbool.h>
#include <stddef.h>

#define IDE_IPC_PRESENTATION_MAX_BYTES 4096

typedef bool (*IdeIpcPresentationHandler)(const char* text,
                                          char* error_out,
                                          size_t error_out_cap,
                                          void* userdata);

void ide_ipc_presentation_set_handler(IdeIpcPresentationHandler handler, void* userdata);
bool ide_ipc_presentation_request(const char* text, char* error_out, size_t error_out_cap);
void ide_ipc_presentation_pump(void);
void ide_ipc_presentation_cancel(void);

#endif
