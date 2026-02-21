#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core/Diagnostics/diagnostics_pack_export.h"
#include "core_pack.h"

static int fail(const char *msg) {
    fprintf(stderr, "idebridge_diag_pack_export_check: %s\n", msg ? msg : "failed");
    return 1;
}

int main(void) {
    const char *diag_json =
        "{"
        "\"id\":\"req-1\","
        "\"ok\":true,"
        "\"result\":{"
        "  \"summary\":{"
        "    \"total\":5,"
        "    \"error\":2,"
        "    \"warn\":2,"
        "    \"info\":1"
        "  },"
        "  \"returned_count\":3,"
        "  \"diagnostics\":["
        "    {\"file\":\"a.c\",\"line\":1,\"col\":1,\"severity\":\"error\",\"message\":\"x\"}"
        "  ]"
        "}"
        "}";

    char tmp_template[] = "/tmp/ide_diag_packXXXXXX";
    int fd = mkstemp(tmp_template);
    if (fd < 0) return fail("mkstemp failed");
    close(fd);
    unlink(tmp_template);

    char pack_path[1024];
    if (snprintf(pack_path, sizeof(pack_path), "%s.pack", tmp_template) >= (int)sizeof(pack_path)) {
        return fail("pack path overflow");
    }

    if (!diagnostics_pack_export_from_diag_response_json(pack_path, diag_json)) {
        return fail("export failed");
    }

    CorePackReader reader;
    CoreResult r = core_pack_reader_open(pack_path, &reader);
    if (r.code != CORE_OK) {
        unlink(pack_path);
        return fail("reader open failed");
    }

    CorePackChunkInfo header_chunk;
    CorePackChunkInfo json_chunk;
    r = core_pack_reader_find_chunk(&reader, "IDHD", 0, &header_chunk);
    if (r.code != CORE_OK) {
        core_pack_reader_close(&reader);
        unlink(pack_path);
        return fail("IDHD chunk missing");
    }
    r = core_pack_reader_find_chunk(&reader, "IDJS", 0, &json_chunk);
    if (r.code != CORE_OK) {
        core_pack_reader_close(&reader);
        unlink(pack_path);
        return fail("IDJS chunk missing");
    }

    IdeDiagnosticsPackHeaderV1 header;
    memset(&header, 0, sizeof(header));
    r = core_pack_reader_read_chunk_data(&reader, &header_chunk, &header, sizeof(header));
    if (r.code != CORE_OK) {
        core_pack_reader_close(&reader);
        unlink(pack_path);
        return fail("IDHD read failed");
    }

    if (header.schema_version != 1u || header.total != 5u || header.error != 2u ||
        header.warn != 2u || header.info != 1u || header.returned_count != 3u) {
        core_pack_reader_close(&reader);
        unlink(pack_path);
        return fail("IDHD values mismatch");
    }

    core_pack_reader_close(&reader);
    unlink(pack_path);
    puts("idebridge_diag_pack_export_check: success");
    return 0;
}
