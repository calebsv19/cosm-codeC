#ifndef IDE_DIAGNOSTICS_CORE_DATA_EXPORT_H
#define IDE_DIAGNOSTICS_CORE_DATA_EXPORT_H

#include <stdbool.h>

bool diagnostics_core_data_export_from_diag_response_json(const char *dataset_json_path,
                                                          const char *diag_response_json);

#endif
