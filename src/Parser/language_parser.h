#ifndef LANGUAGE_PARSER_H
#define LANGUAGE_PARSER_H

#include <stdbool.h>

typedef enum {
    PARSE_STATUS_OK,
    PARSE_STATUS_ERROR,
    PARSE_STATUS_INCOMPLETE
} ParseStatus;

typedef struct {
    const char* filePath;
    const char* sourceCode;
    ParseStatus status;
    // Future: AST pointer, symbol table, tokens, etc.
} ParsedFile;

void initLanguageParser();
void shutdownLanguageParser();

void parseFile(const char* filePath, const char* sourceCode);
const ParsedFile* getLastParsedFile();
bool parserHasErrors();

#endif

