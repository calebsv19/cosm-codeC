#ifndef IDE_IPC_DOCUMENT_H
#define IDE_IPC_DOCUMENT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <json-c/json.h>
typedef bool (*IdeDocumentHandler)(json_object*, json_object**, char*, size_t);
void ide_ipc_document_set_handler(IdeDocumentHandler handler);
bool ide_ipc_document_request(json_object* args, json_object** result, char* error, size_t cap);
void ide_ipc_document_pump(void);
bool ide_ipc_document_from_ui(json_object* args, json_object** result, char* error, size_t cap);
#endif
