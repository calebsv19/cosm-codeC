#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/Diagnostics/diagnostics_core_data_export.h"
#include "core_io.h"

static int fail(const char *msg) {
    fprintf(stderr, "idebridge_diag_core_data_export_check: %s\n", msg ? msg : "failed");
    return 1;
}

int main(void) {
    const char *diag_json =
        "{"
        "\"id\":\"req-2\","
        "\"ok\":true,"
        "\"result\":{"
        "  \"summary\":{\"total\":4,\"error\":1,\"warn\":2,\"info\":1},"
        "  \"returned_count\":3,"
        "  \"truncated\":true,"
        "  \"diagnostics\":["
        "    {\"file\":\"a.c\",\"line\":2,\"col\":3,\"severity\":\"error\",\"message\":\"m1\"},"
        "    {\"file\":\"b.c\",\"line\":4,\"col\":1,\"severity\":\"warn\",\"message\":\"m2\"},"
        "    {\"file\":\"c.c\",\"line\":9,\"col\":6,\"severity\":\"info\",\"message\":\"m3\"}"
        "  ]"
        "}"
        "}";

    char tmp_template[] = "/tmp/ide_diag_datasetXXXXXX";
    int fd = mkstemp(tmp_template);
    if (fd < 0) return fail("mkstemp failed");
    close(fd);
    unlink(tmp_template);

    char json_path[1024];
    if (snprintf(json_path, sizeof(json_path), "%s.json", tmp_template) >= (int)sizeof(json_path)) {
        return fail("dataset path overflow");
    }

    if (!diagnostics_core_data_export_from_diag_response_json(json_path, diag_json)) {
        return fail("export failed");
    }

    CoreBuffer buffer = {0};
    if (core_io_read_all(json_path, &buffer).code != CORE_OK || !buffer.data || buffer.size == 0) {
        unlink(json_path);
        return fail("read back failed");
    }

    const char *text = (const char *)buffer.data;
    if (!strstr(text, "ide_diagnostics_dataset_v1") ||
        !strstr(text, "ide_diagnostics_summary_v1") ||
        !strstr(text, "ide_diagnostics_rows_v1") ||
        !strstr(text, "\"truncated\"") ||
        !strstr(text, "\"diagnostics_count\"")) {
        core_io_buffer_free(&buffer);
        unlink(json_path);
        return fail("dataset contract markers missing");
    }

    core_io_buffer_free(&buffer);
    unlink(json_path);
    puts("idebridge_diag_core_data_export_check: success");
    return 0;
}
