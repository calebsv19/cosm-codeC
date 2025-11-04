#ifndef CORE_CLIPBOARD_H
#define CORE_CLIPBOARD_H

#include <stdbool.h>

/**
 * Copies UTF-8 text to the operating system clipboard. A local copy is held so
 * that callers still have access if the platform clipboard is unavailable.
 */
bool clipboard_copy_text(const char* text);

/**
 * Retrieves UTF-8 text from the operating system clipboard. The caller takes
 * ownership of the returned buffer and must free it via clipboard_free_text.
 * Falls back to the last cached copy when the system clipboard is empty or
 * inaccessible. Returns NULL if no text is available.
 */
char* clipboard_paste_text(void);

/**
 * Releases buffers obtained from clipboard_paste_text.
 */
void clipboard_free_text(char* text);

/**
 * Returns the cached clipboard contents maintained inside the module. The
 * pointer remains owned by the clipboard module and must not be freed.
 */
const char* clipboard_peek_cached(void);

#endif /* CORE_CLIPBOARD_H */
