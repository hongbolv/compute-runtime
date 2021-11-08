/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/linux/local_memory_helper.h"
#include "shared/source/os_interface/linux/memory_info_impl.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "test.h"

using namespace NEO;

TEST(LocalMemoryHelperTestsDefault, givenUnsupportedPlatformWhenCreateGemExtThenReturnErrorNumber) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probed_size = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probed_size = 16 * GB;

    auto localMemHelper = LocalMemoryHelper::get(IGFX_UNKNOWN);
    uint32_t handle = 0;
    auto ret = localMemHelper->createGemExt(drm.get(), &regionInfo[1], 1, 1024, handle);

    EXPECT_EQ(-1u, ret);
}
