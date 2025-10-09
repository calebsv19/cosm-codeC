#pragma once

/*
 * Central build configuration flags.
 * These defaults can be overridden by the build system via -D macros.
 */

#ifndef USE_VULKAN
#define USE_VULKAN 1
#endif

#ifndef ENABLE_VULKAN_VALIDATION
#define ENABLE_VULKAN_VALIDATION 1
#endif

