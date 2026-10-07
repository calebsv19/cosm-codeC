# core_collision3d

Version 0.1.1 (initial API 0.1.0). Renderer-free C11 geometry contract.

One allocation-free double-precision point/plane gap query. Caller declares a single Cartesian frame and supplies point in meters, unit normal, signed plane offset in meters, and nonnegative clearance in meters. The plane is normal dot point = offset. Signed distance is ((x*nx + y*ny) + z*nz) - offset; gap is signed distance minus clearance. Positive gap is the normal side beyond clearance, zero is touching, negative is penetration. Preserve arithmetic order with FP contraction disabled.

Null point/plane/output, nonfinite inputs, normal squared length differing from one by more than 1e-12, negative clearance, or nonfinite arithmetic results return false and preserve output. No normalization, coordinate conversion, tolerance snapping, allocation, response, bodies, contacts/manifolds, integration, material policy, sessions or renderer dependencies. Inputs and output use distinct typed storage. Caller owns fixed-size output. Coordinate conversion remains at each app boundary through core_space when needed; native CMS Y-up state is not rotated.

Standalone C11 and math library only. Independent tests include analytic axial/tilted planes, touching/penetration, translations, axis permutation, normal reversal, double-precision retention and refusal/output preservation. FisiCs caller/Clang implementation C ABI and copied Ball/CMS consumer probes are separately qualified. A pure library test does not prove real program adoption.

Use `make BUILD_DIR=/private/tmp/unique-query-module test`. The initial consumer scope is Ball multi-sphere wall query and CMS finite 3D cloth/static-plane gaps. Impulse response, XPBD, energy, accepted-state and UI remain app-owned. Broader rigid solver extraction is deferred. Consumer adoption must use the committed shared module and each application’s ordinary build and accepted-state tests. Module tests alone do not establish consumer adoption.

0.1.1: enforce the documented FP contraction policy inside the query translation unit so consumers need not change existing solver arithmetic flags. No API or admitted-input change.
