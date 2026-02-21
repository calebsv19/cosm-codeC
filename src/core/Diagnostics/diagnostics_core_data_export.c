#include "core/Diagnostics/diagnostics_core_data_export.h"

#include "core_data.h"
#include "core_io.h"

#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>

static const char *k_default_dataset_path = "ide_files/diagnostics_snapshot.dataset.json";

static bool append_dataset_items_json(json_object *items, const CoreDataset *dataset) {
    if (!items || !dataset) return false;

    for (size_t i = 0; i < dataset->item_count; ++i) {
        const CoreDataItem *item = &dataset->items[i];

        if (item->kind == CORE_DATA_SCALAR_F64) {
            json_object *entry = json_object_new_object();
            if (!entry) return false;
            json_object_object_add(entry, "name", json_object_new_string(item->name ? item->name : "unnamed"));
            json_object_object_add(entry, "kind", json_object_new_string("scalar_f64"));
            json_object_object_add(entry, "value", json_object_new_double(item->as.scalar_f64));
            json_object_array_add(items, entry);
            continue;
        }

        if (item->kind != CORE_DATA_TABLE_TYPED || item->as.table_typed.row_count == 0) continue;

        json_object *entry = json_object_new_object();
        json_object *row0 = json_object_new_object();
        if (!entry || !row0) {
            if (entry) json_object_put(entry);
            if (row0) json_object_put(row0);
            return false;
        }

        json_object_object_add(entry, "name", json_object_new_string(item->name ? item->name : "table_typed"));
        json_object_object_add(entry, "kind", json_object_new_string("table_typed"));
        json_object_object_add(entry, "rows", json_object_new_int((int)item->as.table_typed.row_count));
        json_object_object_add(entry, "columns", json_object_new_int((int)item->as.table_typed.column_count));

        for (uint32_t c = 0; c < item->as.table_typed.column_count; ++c) {
            const CoreTableColumnTyped *col = &item->as.table_typed.columns[c];
            const char *name = col->name ? col->name : "col";
            switch (col->type) {
                case CORE_TABLE_COL_F32:
                    json_object_object_add(row0, name, json_object_new_double((double)col->as.f32_values[0]));
                    break;
                case CORE_TABLE_COL_F64:
                    json_object_object_add(row0, name, json_object_new_double(col->as.f64_values[0]));
                    break;
                case CORE_TABLE_COL_I64:
                    json_object_object_add(row0, name, json_object_new_int64(col->as.i64_values[0]));
                    break;
                case CORE_TABLE_COL_U32:
                    json_object_object_add(row0, name, json_object_new_int64((int64_t)col->as.u32_values[0]));
                    break;
                case CORE_TABLE_COL_BOOL:
                    json_object_object_add(row0, name, json_object_new_boolean(col->as.bool_values[0]));
                    break;
                default:
                    break;
            }
        }

        json_object_object_add(entry, "row0", row0);
        json_object_array_add(items, entry);
    }

    return true;
}

static int json_int_or_zero(json_object *obj, const char *key) {
    json_object *v = NULL;
    if (!obj || !key) return 0;
    if (!json_object_object_get_ex(obj, key, &v) || !v) return 0;
    return json_object_get_int(v);
}

bool diagnostics_core_data_export_from_diag_response_json(const char *dataset_json_path,
                                                          const char *diag_response_json) {
    if (!diag_response_json || !diag_response_json[0]) return false;
    const char *out_path = (dataset_json_path && dataset_json_path[0]) ? dataset_json_path : k_default_dataset_path;

    json_object *root = json_tokener_parse(diag_response_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return false;
    }

    json_object *ok = NULL;
    if (!json_object_object_get_ex(root, "ok", &ok) || !ok || !json_object_get_boolean(ok)) {
        json_object_put(root);
        return false;
    }

    json_object *result = NULL;
    json_object *summary = NULL;
    json_object *diagnostics = NULL;
    json_object_object_get_ex(root, "result", &result);
    if (result) {
        json_object_object_get_ex(result, "summary", &summary);
        json_object_object_get_ex(result, "diagnostics", &diagnostics);
    }

    int total = json_int_or_zero(summary, "total");
    int error = json_int_or_zero(summary, "error");
    int warn = json_int_or_zero(summary, "warn");
    int info = json_int_or_zero(summary, "info");
    int returned = json_int_or_zero(result, "returned_count");
    int truncated = json_int_or_zero(result, "truncated");
    int diag_rows = (diagnostics && json_object_is_type(diagnostics, json_type_array))
                        ? (int)json_object_array_length(diagnostics)
                        : 0;

    CoreDataset dataset;
    core_dataset_init(&dataset);
    CoreResult r = core_dataset_add_metadata_string(&dataset, "profile", "ide_diagnostics_dataset_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "dataset_schema", "ide.diagnostics_snapshot");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(&dataset, "dataset_contract_version", 1);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(&dataset, "schema_version", 1);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "summary_table", "ide_diagnostics_summary_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "rows_table", "ide_diagnostics_rows_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_scalar_f64(&dataset, "diagnostics_count", (double)diag_rows);
    if (r.code != CORE_OK) goto fail;

    {
        const char *cols[] = {
            "total", "error", "warn", "info", "returned_count", "truncated", "diagnostics_count"
        };
        CoreTableColumnType types[] = {
            CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64,
            CORE_TABLE_COL_I64, CORE_TABLE_COL_BOOL, CORE_TABLE_COL_I64
        };
        int64_t total_col[] = {(int64_t)total};
        int64_t error_col[] = {(int64_t)error};
        int64_t warn_col[] = {(int64_t)warn};
        int64_t info_col[] = {(int64_t)info};
        int64_t returned_col[] = {(int64_t)returned};
        bool truncated_col[] = {truncated != 0};
        int64_t rows_col[] = {(int64_t)diag_rows};
        const void *column_data[] = {
            total_col, error_col, warn_col, info_col, returned_col, truncated_col, rows_col
        };
        r = core_dataset_add_table_typed(&dataset,
                                         "ide_diagnostics_summary_v1",
                                         cols,
                                         types,
                                         (uint32_t)(sizeof(cols) / sizeof(cols[0])),
                                         1u,
                                         column_data);
        if (r.code != CORE_OK) goto fail;
    }

    if (diag_rows > 0) {
        int64_t *row_index = (int64_t *)calloc((size_t)diag_rows, sizeof(int64_t));
        int64_t *line_col = (int64_t *)calloc((size_t)diag_rows, sizeof(int64_t));
        int64_t *col_col = (int64_t *)calloc((size_t)diag_rows, sizeof(int64_t));
        int64_t *severity_col = (int64_t *)calloc((size_t)diag_rows, sizeof(int64_t));
        int64_t *file_len_col = (int64_t *)calloc((size_t)diag_rows, sizeof(int64_t));
        int64_t *message_len_col = (int64_t *)calloc((size_t)diag_rows, sizeof(int64_t));
        if (!row_index || !line_col || !col_col || !severity_col || !file_len_col || !message_len_col) {
            free(row_index);
            free(line_col);
            free(col_col);
            free(severity_col);
            free(file_len_col);
            free(message_len_col);
            goto fail;
        }

        for (int i = 0; i < diag_rows; ++i) {
            json_object *d = json_object_array_get_idx(diagnostics, (size_t)i);
            json_object *jf = NULL;
            json_object *jl = NULL;
            json_object *jc = NULL;
            json_object *js = NULL;
            json_object *jm = NULL;
            const char *f = "";
            const char *m = "";
            int sev_id = 0;

            if (d && json_object_is_type(d, json_type_object)) {
                json_object_object_get_ex(d, "file", &jf);
                json_object_object_get_ex(d, "line", &jl);
                json_object_object_get_ex(d, "col", &jc);
                json_object_object_get_ex(d, "severity", &js);
                json_object_object_get_ex(d, "message", &jm);
            }
            if (jf) f = json_object_get_string(jf);
            if (jm) m = json_object_get_string(jm);
            if (js) {
                const char *sev = json_object_get_string(js);
                if (sev && strcmp(sev, "error") == 0) sev_id = 2;
                else if (sev && strcmp(sev, "warn") == 0) sev_id = 1;
            }

            row_index[i] = (int64_t)i;
            line_col[i] = jl ? (int64_t)json_object_get_int(jl) : 0;
            col_col[i] = jc ? (int64_t)json_object_get_int(jc) : 0;
            severity_col[i] = (int64_t)sev_id;
            file_len_col[i] = (int64_t)(f ? strlen(f) : 0);
            message_len_col[i] = (int64_t)(m ? strlen(m) : 0);
        }

        const char *cols[] = {
            "row_index", "line", "col", "severity_id", "file_path_len", "message_len"
        };
        CoreTableColumnType types[] = {
            CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64,
            CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64
        };
        const void *column_data[] = {
            row_index, line_col, col_col, severity_col, file_len_col, message_len_col
        };
        r = core_dataset_add_table_typed(&dataset,
                                         "ide_diagnostics_rows_v1",
                                         cols,
                                         types,
                                         (uint32_t)(sizeof(cols) / sizeof(cols[0])),
                                         (uint32_t)diag_rows,
                                         column_data);
        free(row_index);
        free(line_col);
        free(col_col);
        free(severity_col);
        free(file_len_col);
        free(message_len_col);
        if (r.code != CORE_OK) goto fail;
    }

    {
        json_object *out = json_object_new_object();
        json_object *meta = json_object_new_object();
        json_object *items = json_object_new_array();
        if (!out || !meta || !items) {
            if (out) json_object_put(out);
            if (meta) json_object_put(meta);
            if (items) json_object_put(items);
            goto fail;
        }

        json_object_object_add(out, "profile", json_object_new_string("ide_diagnostics_dataset_v1"));
        json_object_object_add(out, "dataset_schema", json_object_new_string("ide.diagnostics_snapshot"));
        json_object_object_add(out, "schema_version", json_object_new_int(1));

        for (size_t i = 0; i < dataset.metadata_count; ++i) {
            const CoreMetadataItem *m = &dataset.metadata[i];
            if (!m->key) continue;
            switch (m->type) {
                case CORE_META_STRING:
                    json_object_object_add(meta, m->key, json_object_new_string(m->as.string_value ? m->as.string_value : ""));
                    break;
                case CORE_META_F64:
                    json_object_object_add(meta, m->key, json_object_new_double(m->as.f64_value));
                    break;
                case CORE_META_I64:
                    json_object_object_add(meta, m->key, json_object_new_int64(m->as.i64_value));
                    break;
                case CORE_META_BOOL:
                    json_object_object_add(meta, m->key, json_object_new_boolean(m->as.bool_value));
                    break;
                default:
                    break;
            }
        }
        json_object_object_add(out, "metadata", meta);
        if (!append_dataset_items_json(items, &dataset)) {
            json_object_put(out);
            goto fail;
        }
        json_object_object_add(out, "items", items);

        const char *text = json_object_to_json_string_ext(out, JSON_C_TO_STRING_PRETTY);
        r = core_io_write_all(out_path, text, strlen(text));
        json_object_put(out);
        if (r.code != CORE_OK) goto fail;
    }

    core_dataset_free(&dataset);
    json_object_put(root);
    return true;

fail:
    core_dataset_free(&dataset);
    json_object_put(root);
    return false;
}
