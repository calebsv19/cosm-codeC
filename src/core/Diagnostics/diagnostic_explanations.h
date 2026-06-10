#ifndef DIAGNOSTIC_EXPLANATIONS_H
#define DIAGNOSTIC_EXPLANATIONS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int codeId;
    char* codeName;
    int categoryId;
    char* categoryName;
    char* stage;
    char* description;
    char* commonCauses;
    char* nextAction;
} DiagnosticExplanation;

void diagnostic_explanations_clear(void);
bool diagnostic_explanations_refresh_from_fisics_metadata(void);
bool diagnostic_explanations_load_json_text(const char* jsonText);
bool diagnostic_explanations_save(const char* workspaceRoot);
bool diagnostic_explanations_load(const char* workspaceRoot);
size_t diagnostic_explanations_count(void);
const DiagnosticExplanation* diagnostic_explanations_at(size_t index);
const DiagnosticExplanation* diagnostic_explanations_find_by_code(int codeId);
const DiagnosticExplanation* diagnostic_explanations_find_by_name(const char* codeName);

#endif
