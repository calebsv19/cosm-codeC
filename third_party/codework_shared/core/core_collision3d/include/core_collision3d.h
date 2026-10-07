#ifndef CORE_COLLISION3D_H
#define CORE_COLLISION3D_H
#include <stdbool.h>
/* One allocation-free query: one caller-declared Cartesian frame, meters. */
typedef struct CoreCollision3DVec3 { double x,y,z; } CoreCollision3DVec3;
typedef struct CoreCollision3DPlane { CoreCollision3DVec3 normal; double offset_m; } CoreCollision3DPlane;
typedef struct CoreCollision3DPlaneGap { double signed_distance_m,gap_m; } CoreCollision3DPlaneGap;
/* Positive gap is beyond clearance on the normal side; zero is touching.
 * No conversion, normalization, tolerance snapping, allocation or response.
 * False rejects null/nonfinite/non-unit/negative-clearance/overflow input.
 * Failure preserves caller-owned output. Inputs and output use distinct storage. */
bool core_collision3d_plane_gap(const CoreCollision3DVec3* point_m,
    const CoreCollision3DPlane* plane,double clearance_m,CoreCollision3DPlaneGap* out);
#endif
