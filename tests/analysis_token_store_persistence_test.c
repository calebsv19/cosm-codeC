#include "core/Analysis/analysis_token_store.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "test_fixture_utils.h"

static void test_small_token_cache_round_trip(void) {
    char root[256];
    assert(ide_test_prepare_workspace(root, sizeof(root), "ide_token_store_persistence_small"));

    FisicsTokenSpan spans[2] = {0};
    spans[0].line = 1;
    spans[0].column = 2;
    spans[0].length = 3;
    spans[0].kind = FISICS_TOK_IDENTIFIER;
    spans[1].line = 2;
    spans[1].column = 4;
    spans[1].length = 5;
    spans[1].kind = FISICS_TOK_KEYWORD;

    analysis_token_store_clear();
    analysis_token_store_upsert("/tmp/project/src/a.c", spans, 2);
    analysis_token_store_save(root);

    char path[256];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_tokens.json", root);
    assert(ide_test_file_exists(path));

    analysis_token_store_clear();
    analysis_token_store_load(root);
    assert(analysis_token_store_file_count() == 1);
    const AnalysisFileTokens* file = analysis_token_store_file_at(0);
    assert(file && file->count == 2);
    assert(strcmp(file->path, "/tmp/project/src/a.c") == 0);
    assert(file->spans[1].kind == FISICS_TOK_KEYWORD);
    analysis_token_store_clear();
}

static void test_oversized_token_cache_removes_stale_artifact(void) {
    char root[256];
    assert(ide_test_prepare_workspace(root, sizeof(root), "ide_token_store_persistence_oversized"));

    FisicsTokenSpan small = {0};
    small.line = 1;
    small.column = 1;
    small.length = 1;
    small.kind = FISICS_TOK_IDENTIFIER;

    analysis_token_store_clear();
    analysis_token_store_upsert("/tmp/project/src/small.c", &small, 1);
    analysis_token_store_save(root);

    char path[256];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_tokens.json", root);
    assert(ide_test_file_exists(path));

    FisicsTokenSpan spans[80] = {0};
    for (size_t i = 0; i < sizeof(spans) / sizeof(spans[0]); ++i) {
        spans[i].line = (int)i + 1;
        spans[i].column = 1000;
        spans[i].length = 1000;
        spans[i].kind = FISICS_TOK_IDENTIFIER;
    }
    analysis_token_store_clear();
    analysis_token_store_upsert("/tmp/project/src/large.c", spans,
                                sizeof(spans) / sizeof(spans[0]));
    analysis_token_store_save(root);
    assert(!ide_test_file_exists(path));

    analysis_token_store_load(root);
    assert(analysis_token_store_file_count() == 0);
    analysis_token_store_clear();
}

static void test_load_prunes_legacy_oversized_token_cache(void) {
    char root[256];
    assert(ide_test_prepare_workspace(root, sizeof(root), "ide_token_store_persistence_legacy_oversized"));

    char path[256];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_tokens.json", root);
    FILE* out = fopen(path, "w");
    assert(out);
    for (int i = 0; i < 2048; ++i) {
        fputc("[,]"[i % 3], out);
    }
    fclose(out);
    assert(ide_test_file_exists(path));

    analysis_token_store_clear();
    analysis_token_store_load(root);
    assert(analysis_token_store_file_count() == 0);
    assert(!ide_test_file_exists(path));
    analysis_token_store_clear();
}

int main(void) {
    test_small_token_cache_round_trip();
    test_oversized_token_cache_removes_stale_artifact();
    test_load_prunes_legacy_oversized_token_cache();
    puts("analysis_token_store_persistence_test: ok");
    return 0;
}
