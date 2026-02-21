#include "core/Diagnostics/diagnostics_pack_export.h"

#include "core_pack.h"

#include <json-c/json.h>
#include <string.h>

static uint32_t json_u32(json_object *obj, const char *key) {
    json_object *v = NULL;
    if (!obj || !key) return 0u;
    if (!json_object_object_get_ex(obj, key, &v) || !v) return 0u;
    int value = json_object_get_int(v);
    return (value < 0) ? 0u : (uint32_t)value;
}

bool diagnostics_pack_export_from_diag_response_json(const char *pack_path,
                                                     const char *diag_response_json) {
    if (!pack_path || !pack_path[0] || !diag_response_json || !diag_response_json[0]) return false;

    json_object *root = json_tokener_parse(diag_response_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }

    json_object *ok_obj = NULL;
    if (!json_object_object_get_ex(root, "ok", &ok_obj) || !ok_obj || !json_object_get_boolean(ok_obj)) {
        json_object_put(root);
        return false;
    }

    json_object *result = NULL;
    json_object *summary = NULL;
    uint32_t total = 0;
    uint32_t error = 0;
    uint32_t warn = 0;
    uint32_t info = 0;
    uint32_t returned = 0;

    if (json_object_object_get_ex(root, "result", &result) && result) {
        returned = json_u32(result, "returned_count");
        if (json_object_object_get_ex(result, "summary", &summary) && summary) {
            total = json_u32(summary, "total");
            error = json_u32(summary, "error");
            warn = json_u32(summary, "warn");
            info = json_u32(summary, "info");
        }
    }

    IdeDiagnosticsPackHeaderV1 header;
    memset(&header, 0, sizeof(header));
    header.schema_version = 1u;
    header.total = total;
    header.error = error;
    header.warn = warn;
    header.info = info;
    header.returned_count = returned;

    CorePackWriter writer;
    CoreResult r = core_pack_writer_open(pack_path, &writer);
    if (r.code != CORE_OK) {
        json_object_put(root);
        return false;
    }

    r = core_pack_writer_add_chunk(&writer, "IDHD", &header, sizeof(header));
    if (r.code == CORE_OK) {
        r = core_pack_writer_add_chunk(&writer,
                                       "IDJS",
                                       diag_response_json,
                                       (uint64_t)strlen(diag_response_json));
    }

    CoreResult close_r = core_pack_writer_close(&writer);
    json_object_put(root);
    if (r.code != CORE_OK || close_r.code != CORE_OK) return false;
    return true;
}
