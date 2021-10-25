/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferPolicy, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(BufferSetSurfaceTests, givenBufferSetSurfaceThatMemoryIsUnalignedToCachelineButReadOnlyThenL3CacheShouldBeStillOn, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, WhenAllowRenderCompressionIsCalledThenTrueIsReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHardwareInfoWhenCallingIsMaxThreadsForWorkgroupWARequiredThenFalseIsReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, whenCallingGetDeviceMemoryNameThenDdrIsReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfExtraParametersAreInvalidThenFalseIsReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(CommandStreamReceiverFlushTaskXeHPAndLaterTests, givenProgramPipeControlPriorToNonPipelinedStateCommandDebugKeyAndStateSipWhenItIsRequiredThenThereIsPipeControlPriorToIt, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfTile64With3DSurfaceOnBCSIsSupportedThenTrueIsReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfBlitterForImagesIsSupportedThenFalseIsReturned, IGFX_XE_HP_CORE);
HWTEST_EXCLUDE_PRODUCT(HwHelperTest, whenGettingDefaultRevisionIdThenCorrectValueIsReturned, IGFX_XE_HP_CORE);
