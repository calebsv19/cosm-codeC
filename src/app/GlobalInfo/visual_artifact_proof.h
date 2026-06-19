#ifndef VISUAL_ARTIFACT_PROOF_H
#define VISUAL_ARTIFACT_PROOF_H

#include <stdbool.h>

#include "engine/Render/render_pipeline.h"

bool ide_visual_artifact_proof_enabled(void);
bool ide_visual_artifact_proof_pending(void);
void ide_visual_artifact_proof_begin_frame(RenderContext* ctx);
void ide_visual_artifact_proof_end_frame(RenderContext* ctx, bool* running);
int ide_visual_artifact_proof_exit_code(void);
const char* ide_visual_artifact_proof_path(void);

#endif
