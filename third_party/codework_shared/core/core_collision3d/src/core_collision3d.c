#include "core_collision3d.h"
#include <math.h>
/* Keep this query arithmetic independent of the host solver build flags. */
#pragma STDC FP_CONTRACT OFF
bool core_collision3d_plane_gap(const CoreCollision3DVec3* point_m,
    const CoreCollision3DPlane* plane,double clearance_m,CoreCollision3DPlaneGap* out) {
    if(!point_m||!plane||!out)return false;
    const CoreCollision3DVec3 p=*point_m,n=plane->normal;
    if(!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z)||!isfinite(n.x)||!isfinite(n.y)||!isfinite(n.z)||!isfinite(plane->offset_m)||!isfinite(clearance_m)||clearance_m<0)return false;
    const double n2=(n.x*n.x+n.y*n.y)+n.z*n.z;
    if(!isfinite(n2)||fabs(n2-1)>1e-12)return false;
    CoreCollision3DPlaneGap candidate;
    candidate.signed_distance_m=((p.x*n.x+p.y*n.y)+p.z*n.z)-plane->offset_m;
    candidate.gap_m=candidate.signed_distance_m-clearance_m;
    if(!isfinite(candidate.signed_distance_m)||!isfinite(candidate.gap_m))return false;
    *out=candidate;
    return true;
}
