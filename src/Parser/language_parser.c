#include "language_parser.h"
#include "../Diagnostics/diagnostics_engine.h"
#include <stdlib.h>
#include <string.h>

static ParsedFile lastParsed;

void initLanguageParser() {
    lastParsed.filePath = NULL;
    lastParsed.sourceCode = NULL;
    lastParsed.status = PARSE_STATUS_INCOMPLETE;
}

void shutdownLanguageParser() {
    lastParsed.filePath = NULL;
    lastParsed.sourceCode = NULL;
}

void parseFile(const char* filePath, const char* sourceCode) {
    lastParsed.filePath = filePath;
    lastParsed.sourceCode = sourceCode;
    lastParsed.status = PARSE_STATUS_OK;

    clearDiagnostics();  // Clear previous run

    // Dummy parsing logic:
    if (strstr(sourceCode, "error")) {
        addDiagnostic(filePath, 1, 5, "Simulated syntax error: unexpected token", DIAG_SEVERITY_ERROR);
        lastParsed.status = PARSE_STATUS_ERROR;
    }
}

const ParsedFile* getLastParsedFile() {
    return &lastParsed;
}

bool parserHasErrors() {
    return (lastParsed.status == PARSE_STATUS_ERROR);
}

