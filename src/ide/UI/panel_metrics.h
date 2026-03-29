#ifndef IDE_UI_PANEL_METRICS_H
#define IDE_UI_PANEL_METRICS_H

int ide_ui_dense_row_height(void);
int ide_ui_dense_header_height(void);
int ide_ui_tree_indent_width(void);

#define IDE_UI_DENSE_ROW_HEIGHT ide_ui_dense_row_height()
#define IDE_UI_DENSE_HEADER_HEIGHT ide_ui_dense_header_height()
#define IDE_UI_TREE_INDENT_WIDTH ide_ui_tree_indent_width()

#endif // IDE_UI_PANEL_METRICS_H
