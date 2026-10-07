#include "core_collision3d.h"

typedef CoreCollision3DVec3 QueryVec3;
typedef CoreCollision3DPlaneGap QueryResult;
static bool query_plane(QueryVec3 p,QueryVec3 n,double offset,double clearance,QueryResult* out) {
    CoreCollision3DPlane plane={n,offset};
    return core_collision3d_plane_gap(&p,&plane,clearance,out);
}

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
int main(void){
 QueryResult r={7,8};
 assert(query_plane((QueryVec3){2,3,4},(QueryVec3){0,1,0},1,.25,&r)&&r.signed_distance_m==2&&r.gap_m==1.75);
 assert(query_plane((QueryVec3){2,3,4},(QueryVec3){0,0,1},3,1,&r)&&r.gap_m==0);
 assert(query_plane((QueryVec3){2,3,4},(QueryVec3){-1,0,0},-1,0,&r)&&r.gap_m==-1);
 for(int i=-100;i<=100;i++){
  double x=i*.125;QueryResult a,b;
  assert(query_plane((QueryVec3){x,2,3},(QueryVec3){1,0,0},.5,.25,&a));
  assert(a.gap_m==x-.75);
  assert(query_plane((QueryVec3){x+4,2,3},(QueryVec3){1,0,0},4.5,.25,&b)&&a.gap_m==b.gap_m);
  assert(query_plane((QueryVec3){2,3,x},(QueryVec3){0,0,1},.5,.25,&b)&&a.gap_m==b.gap_m);
  assert(query_plane((QueryVec3){x,2,3},(QueryVec3){-1,0,0},-.5,0,&b)&&b.signed_distance_m==-a.signed_distance_m);
 }
 QueryResult tilted;
 assert(query_plane((QueryVec3){3,4,2},(QueryVec3){.6,.8,0},1,.25,&tilted)&&fabs(tilted.signed_distance_m-4)<1e-14&&fabs(tilted.gap_m-3.75)<1e-14);
 QueryResult translated;
 assert(query_plane((QueryVec3){8,4,2},(QueryVec3){.6,.8,0},4,.25,&translated)&&fabs(translated.gap_m-tilted.gap_m)<1e-14);
 assert(query_plane((QueryVec3){16777217,0,0},(QueryVec3){1,0,0},16777216,0,&r)&&r.gap_m==1);
 assert(query_plane((QueryVec3){0,0,0},(QueryVec3){sqrt(1+5e-13),0,0},0,0,&r));
 assert(!query_plane((QueryVec3){0,0,0},(QueryVec3){sqrt(1+2e-12),0,0},0,0,&r));
 CoreCollision3DPlane plane={{0,1,0},0};CoreCollision3DVec3 point={0,1,0};
 assert(!core_collision3d_plane_gap(NULL,&plane,0,&r));
 assert(!core_collision3d_plane_gap(&point,NULL,0,&r));
 assert(!core_collision3d_plane_gap(&point,&plane,0,NULL));
 QueryResult before=r;
 assert(!query_plane((QueryVec3){NAN,0,0},(QueryVec3){1,0,0},0,0,&r));
 assert(!query_plane((QueryVec3){0,0,0},(QueryVec3){0,0,0},0,0,&r));
 assert(!query_plane((QueryVec3){0,0,0},(QueryVec3){2,0,0},0,0,&r));
 assert(!query_plane((QueryVec3){0,0,0},(QueryVec3){1,0,0},0,-1,&r));
 assert(!query_plane((QueryVec3){DBL_MAX,0,0},(QueryVec3){1,0,0},-DBL_MAX,0,&r));
 assert(r.signed_distance_m==before.signed_distance_m&&r.gap_m==before.gap_m);
 puts("PASS analytic axes/touching/penetration, translation, axis permutation, normal reversal, finite/unit/clearance/overflow refusals and output preservation");
 return 0;
}
