#ifndef IDE_DIAGNOSTICS_PACK_EXPORT_H
#define IDE_DIAGNOSTICS_PACK_EXPORT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct IdeDiagnosticsPackHeaderV1 {
    uint32_t schema_version;
    uint32_t total;
    uint32_t error;
    uint32_t warn;
    uint32_t info;
    uint32_t returned_count;
} IdeDiagnosticsPackHeaderV1;

bool diagnostics_pack_export_from_diag_response_json(const char *pack_path,
                                                     const char *diag_response_json);

#endif
