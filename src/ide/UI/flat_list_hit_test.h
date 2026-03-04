#ifndef IDE_UI_FLAT_LIST_HIT_TEST_H
#define IDE_UI_FLAT_LIST_HIT_TEST_H

typedef int (*UIFlatListRowHeightFn)(int rowIndex, void* context);

static inline int ui_flat_list_hit_test_variable(int mouseY,
                                                 int firstY,
                                                 float offset,
                                                 int rowCount,
                                                 UIFlatListRowHeightFn rowHeightForIndex,
                                                 void* context) {
    if (rowCount <= 0 || !rowHeightForIndex) return -1;

    int y = firstY - (int)offset;
    for (int i = 0; i < rowCount; ++i) {
        int height = rowHeightForIndex(i, context);
        if (height <= 0) continue;
        int blockBottom = y + height;
        if (mouseY >= y && mouseY < blockBottom) {
            return i;
        }
        y = blockBottom;
    }
    return -1;
}

#endif
