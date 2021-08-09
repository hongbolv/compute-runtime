/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_submissions_aggregator.h"
#include "test.h"

using namespace NEO;

typedef UltCommandStreamReceiverTest CommandStreamReceiverFlushTaskXeHPPlusTests;

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, whenReprogrammingSshThenBindingTablePoolIsProgrammed) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));
    auto bindingTablePoolAlloc = getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTablePoolAlloc);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ssh.getCpuBase()), bindingTablePoolAlloc->getBindingTablePoolBaseAddress());
    EXPECT_EQ(ssh.getHeapSizeInPages(), bindingTablePoolAlloc->getBindingTablePoolBufferSize());
    EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER),
              bindingTablePoolAlloc->getSurfaceObjectControlStateIndexToMocsTables());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, whenReprogrammingSshThenBindingTablePoolIsProgrammedWithCachingOffWhenDebugKeyPresent) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DisableCachingForHeaps.set(1);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));
    auto bindingTablePoolAlloc = getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTablePoolAlloc);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ssh.getCpuBase()), bindingTablePoolAlloc->getBindingTablePoolBaseAddress());
    EXPECT_EQ(ssh.getHeapSizeInPages(), bindingTablePoolAlloc->getBindingTablePoolBufferSize());
    EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED),
              bindingTablePoolAlloc->getSurfaceObjectControlStateIndexToMocsTables());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, whenNotReprogrammingSshThenBindingTablePoolIsNotProgrammed) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));
    auto stateBaseAddress = getCommand<typename FamilyType::STATE_BASE_ADDRESS>();
    EXPECT_NE(nullptr, stateBaseAddress);
    auto bindingTablePoolAlloc = getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTablePoolAlloc);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ssh.getCpuBase()), bindingTablePoolAlloc->getBindingTablePoolBaseAddress());
    EXPECT_EQ(ssh.getHeapSizeInPages(), bindingTablePoolAlloc->getBindingTablePoolBufferSize());
    EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER),
              bindingTablePoolAlloc->getSurfaceObjectControlStateIndexToMocsTables());

    auto offset = commandStreamReceiver.getCS(0).getUsed();
    // make SBA dirty (using ioh as dsh and dsh as ioh just to force SBA reprogramming)
    commandStreamReceiver.flushTask(commandStream, 0, ioh, dsh, ssh, taskLevel, flushTaskFlags, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.getCS(0), offset);
    stateBaseAddress = hwParser.getCommand<typename FamilyType::STATE_BASE_ADDRESS>();
    EXPECT_NE(nullptr, stateBaseAddress);
    bindingTablePoolAlloc = hwParser.getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_EQ(nullptr, bindingTablePoolAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenStateBaseAddressWhenItIsRequiredThenThereIsPipeControlPriorToItWithTextureCacheFlushAndHdc) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    configureCSRtoNonDirtyState<FamilyType>(false);
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);
    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));

    auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    auto pipeControlItor = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), stateBaseAddressItor);
    EXPECT_NE(stateBaseAddressItor, pipeControlItor);
    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*pipeControlItor);
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::isDcFlushAllowed(), pipeControlCmd->getDcFlushEnable());
    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, whenNotReprogrammingSshButInitProgrammingFlagsThenBindingTablePoolIsProgrammed) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.getCS(0));
    auto stateBaseAddress = getCommand<typename FamilyType::STATE_BASE_ADDRESS>();
    EXPECT_NE(nullptr, stateBaseAddress);
    auto bindingTablePoolAlloc = getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    ASSERT_NE(nullptr, bindingTablePoolAlloc);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ssh.getCpuBase()), bindingTablePoolAlloc->getBindingTablePoolBaseAddress());
    EXPECT_EQ(ssh.getHeapSizeInPages(), bindingTablePoolAlloc->getBindingTablePoolBufferSize());
    EXPECT_EQ(pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER),
              bindingTablePoolAlloc->getSurfaceObjectControlStateIndexToMocsTables());

    auto offset = commandStreamReceiver.getCS(0).getUsed();
    commandStreamReceiver.initProgrammingFlags();
    flushTask(commandStreamReceiver);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStreamReceiver.getCS(0), offset);
    stateBaseAddress = hwParser.getCommand<typename FamilyType::STATE_BASE_ADDRESS>();
    EXPECT_NE(nullptr, stateBaseAddress);
    bindingTablePoolAlloc = hwParser.getCommand<typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    EXPECT_NE(nullptr, bindingTablePoolAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenSbaProgrammingWhenHeapsAreNotProvidedThenDontProgram) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    uint64_t instructionHeapBase = 0x10000;
    uint64_t internalHeapBase = 0x10000;
    uint64_t generalStateBase = 0x30000;
    STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(&sbaCmd,
                                                                nullptr,
                                                                nullptr,
                                                                nullptr,
                                                                generalStateBase,
                                                                true,
                                                                0,
                                                                internalHeapBase,
                                                                instructionHeapBase,
                                                                0,
                                                                true,
                                                                false,
                                                                pDevice->getGmmHelper(),
                                                                false,
                                                                MemoryCompressionState::NotApplicable,
                                                                false,
                                                                1u);

    EXPECT_FALSE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBufferSize());

    EXPECT_FALSE(sbaCmd.getIndirectObjectBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getIndirectObjectBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getIndirectObjectBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getIndirectObjectBufferSize());

    EXPECT_FALSE(sbaCmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd.getInstructionBaseAddressModifyEnable());
    EXPECT_EQ(instructionHeapBase, sbaCmd.getInstructionBaseAddress());
    EXPECT_TRUE(sbaCmd.getInstructionBufferSizeModifyEnable());
    EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, sbaCmd.getInstructionBufferSize());

    EXPECT_TRUE(sbaCmd.getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd.getGeneralStateBufferSizeModifyEnable());
    if constexpr (is64bit) {
        EXPECT_EQ(GmmHelper::decanonize(internalHeapBase), sbaCmd.getGeneralStateBaseAddress());
    } else {
        EXPECT_EQ(generalStateBase, sbaCmd.getGeneralStateBaseAddress());
    }
    EXPECT_EQ(0xfffffu, sbaCmd.getGeneralStateBufferSize());

    EXPECT_EQ(0u, sbaCmd.getBindlessSurfaceStateBaseAddress());
    EXPECT_FALSE(sbaCmd.getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getBindlessSurfaceStateSize());
}

using isXeHPOrAbove = IsAtLeastProduct<IGFX_XE_HP_SDV>;
HWTEST2_F(CommandStreamReceiverFlushTaskXeHPPlusTests, whenFlushAllCachesVariableIsSetAndAddPipeControlIsCalledThenFieldsAreProperlySet, isXeHPOrAbove) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.FlushAllCaches.set(true);

    char buff[sizeof(PIPE_CONTROL) * 3];
    LinearStream stream(buff, sizeof(PIPE_CONTROL) * 3);

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addPipeControl(stream, args);

    parseCommands<FamilyType>(stream, 0);

    PIPE_CONTROL *pipeControl = getCommand<PIPE_CONTROL>();

    ASSERT_NE(nullptr, pipeControl);

    // WA pipeControl added
    if (cmdList.size() == 2) {
        pipeControl++;
    }

    EXPECT_TRUE(pipeControl->getDcFlushEnable());
    EXPECT_TRUE(pipeControl->getRenderTargetCacheFlushEnable());
    EXPECT_TRUE(pipeControl->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getPipeControlFlushEnable());
    EXPECT_TRUE(pipeControl->getVfCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getConstantCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getStateCacheInvalidationEnable());
    // XeHP+ only field
    EXPECT_TRUE(pipeControl->getCompressionControlSurfaceCcsFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenconfigureCSRtoNonDirtyStateWhenFlushTaskIsCalledThenNoCommandsAreAdded) {
    configureCSRtoNonDirtyState<FamilyType>(true);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);
    EXPECT_EQ(0u, commandStreamReceiver.commandStream.getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenMultiOsContextCommandStreamReceiverWhenFlushTaskIsCalledThenCommandStreamReceiverStreamIsUsed) {
    configureCSRtoNonDirtyState<FamilyType>(true);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.multiOsContextCapable = true;
    commandStream.getSpace(4);

    flushTask(commandStreamReceiver);
    EXPECT_EQ(MemoryConstants::cacheLineSize, commandStreamReceiver.commandStream.getUsed());
    auto batchBufferStart = genCmdCast<typename FamilyType::MI_BATCH_BUFFER_START *>(commandStreamReceiver.commandStream.getCpuBase());
    EXPECT_NE(nullptr, batchBufferStart);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenCsrInBatchingModeWhenTaskIsSubmittedViaCsrThenBbEndCoversPaddingEnoughToFitMiBatchBufferStart) {
    auto &mockCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    mockCsr.overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    mockCsr.timestampPacketWriteEnabled = false;

    configureCSRtoNonDirtyState<FamilyType>(true);

    mockCsr.getCS(1024u);
    auto &csrCommandStream = mockCsr.commandStream;

    //we do level change that will emit PPC, fill all the space so only BB end fits.
    taskLevel++;
    auto ppcSize = MemorySynchronizationCommands<FamilyType>::getSizeForSinglePipeControl();
    auto fillSize = MemoryConstants::cacheLineSize - ppcSize - sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    csrCommandStream.getSpace(fillSize);
    auto expectedUsedSize = 2 * MemoryConstants::cacheLineSize;

    flushTask(mockCsr);

    EXPECT_EQ(expectedUsedSize, mockCsr.commandStream.getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, GivenSameTaskLevelThenDontSendPipeControl) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    // Configure the CSR to not need to submit any state or commands.
    configureCSRtoNonDirtyState<FamilyType>(true);

    flushTask(commandStreamReceiver);

    EXPECT_EQ(taskLevel, commandStreamReceiver.taskLevel);

    auto sizeUsed = commandStreamReceiver.commandStream.getUsed();
    EXPECT_EQ(sizeUsed, 0u);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenDeviceWithThreadGroupPreemptionSupportThenDontSendMediaVfeStateIfNotDirty) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::ThreadGroup));

    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    // Configure the CSR to not need to submit any state or commands.
    configureCSRtoNonDirtyState<FamilyType>(true);

    flushTask(*commandStreamReceiver);

    EXPECT_EQ(taskLevel, commandStreamReceiver->peekTaskLevel());

    auto sizeUsed = commandStreamReceiver->commandStream.getUsed();
    EXPECT_EQ(0u, sizeUsed);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenCommandStreamReceiverWithInstructionCacheRequestWhenFlushTaskIsCalledThenPipeControlWithInstructionCacheIsEmitted) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    configureCSRtoNonDirtyState<FamilyType>(true);

    commandStreamReceiver.registerInstructionCacheFlush();
    EXPECT_EQ(1u, commandStreamReceiver.recursiveLockCounter);

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorPC);
    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pipeControlCmd->getInstructionCacheInvalidateEnable());
    EXPECT_FALSE(commandStreamReceiver.requiresInstructionCacheFlush);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenHigherTaskLevelWhenTimestampPacketWriteIsEnabledThenDontAddPipeControl) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.timestampPacketWriteEnabled = true;
    commandStreamReceiver.isPreambleSent = true;
    configureCSRtoNonDirtyState<FamilyType>(true);
    commandStreamReceiver.taskLevel = taskLevel;
    taskLevel++; // submit with higher taskLevel

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, WhenForcePipeControlPriorToWalkerIsSetThenAddExtraPipeControls) {
    DebugManagerStateRestore stateResore;
    DebugManager.flags.ForcePipeControlPriorToWalker.set(true);
    DebugManager.flags.FlushAllCaches.set(true);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    configureCSRtoNonDirtyState<FamilyType>(true);
    commandStreamReceiver.taskLevel = taskLevel;

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    GenCmdList::iterator itor = cmdList.begin();
    int counterPC = 0;
    while (itor != cmdList.end()) {
        auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(*itor);
        if (pipeControl) {
            switch (counterPC) {
            case 0: // First pipe control with CS Stall
                EXPECT_EQ(bool(pipeControl->getCommandStreamerStallEnable()), true);
                EXPECT_EQ(bool(pipeControl->getDcFlushEnable()), false);
                EXPECT_EQ(bool(pipeControl->getRenderTargetCacheFlushEnable()), false);
                EXPECT_EQ(bool(pipeControl->getInstructionCacheInvalidateEnable()), false);
                EXPECT_EQ(bool(pipeControl->getTextureCacheInvalidationEnable()), false);
                EXPECT_EQ(bool(pipeControl->getPipeControlFlushEnable()), false);
                EXPECT_EQ(bool(pipeControl->getVfCacheInvalidationEnable()), false);
                EXPECT_EQ(bool(pipeControl->getConstantCacheInvalidationEnable()), false);
                EXPECT_EQ(bool(pipeControl->getStateCacheInvalidationEnable()), false);
                break;
            case 1: // Second pipe control with all flushes
                EXPECT_EQ(bool(pipeControl->getCommandStreamerStallEnable()), true);
                EXPECT_EQ(bool(pipeControl->getDcFlushEnable()), true);
                EXPECT_EQ(bool(pipeControl->getRenderTargetCacheFlushEnable()), true);
                EXPECT_EQ(bool(pipeControl->getInstructionCacheInvalidateEnable()), true);
                EXPECT_EQ(bool(pipeControl->getTextureCacheInvalidationEnable()), true);
                EXPECT_EQ(bool(pipeControl->getPipeControlFlushEnable()), true);
                EXPECT_EQ(bool(pipeControl->getVfCacheInvalidationEnable()), true);
                EXPECT_EQ(bool(pipeControl->getConstantCacheInvalidationEnable()), true);
                EXPECT_EQ(bool(pipeControl->getStateCacheInvalidationEnable()), true);
            default:
                break;
            }
            counterPC++;
        }

        ++itor;
    }

    EXPECT_EQ(counterPC, 2);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, whenSamplerCacheFlushNotRequiredThenDontSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    NEO::WorkaroundTable *waTable = &pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable;

    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.lastPreemptionMode = pDevice->getPreemptionMode();
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired);
    configureCSRtoNonDirtyState<FamilyType>(true);
    commandStreamReceiver.taskLevel = taskLevel;
    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
    flushTask(commandStreamReceiver);

    EXPECT_EQ(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushNotRequired, commandStreamReceiver.samplerCacheFlushRequired);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, whenSamplerCacheFlushBeforeAndWaSamplerCacheFlushBetweenRedescribedSurfaceReadsDasabledThenDontSendPipecontrol) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    commandStreamReceiver.setSamplerCacheFlushRequired(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore);
    configureCSRtoNonDirtyState<FamilyType>(true);
    commandStreamReceiver.taskLevel = taskLevel;
    NEO::WorkaroundTable *waTable = &pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable;

    waTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = false;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(commandStreamReceiver.commandStream.getUsed(), 0u);
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore, commandStreamReceiver.samplerCacheFlushRequired);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);

    auto itorPC = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itorPC);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, WhenFlushingTaskThenStateBaseAddressProgrammingShouldMatchTracking) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    auto gmmHelper = pDevice->getGmmHelper();
    auto stateHeapMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER);
    auto l1CacheOnMocs = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    auto &commandStreamCSR = commandStreamReceiver.commandStream;
    HardwareParse::parseCommands<FamilyType>(commandStreamCSR, 0);
    HardwareParse::findHardwareCommands<FamilyType>();

    ASSERT_NE(nullptr, cmdStateBaseAddress);
    auto &cmd = *reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStateBaseAddress);

    EXPECT_EQ(dsh.getCpuBase(), reinterpret_cast<void *>(cmd.getDynamicStateBaseAddress()));
    EXPECT_EQ(commandStreamReceiver.getMemoryManager()->getInternalHeapBaseAddress(commandStreamReceiver.rootDeviceIndex, ioh.getGraphicsAllocation()->isAllocatedInLocalMemoryPool()), cmd.getInstructionBaseAddress());
    EXPECT_EQ(ioh.getCpuBase(), reinterpret_cast<void *>(cmd.getIndirectObjectBaseAddress()));
    EXPECT_EQ(ssh.getCpuBase(), reinterpret_cast<void *>(cmd.getSurfaceStateBaseAddress()));

    EXPECT_EQ(l1CacheOnMocs, cmd.getStatelessDataPortAccessMemoryObjectControlState());
    EXPECT_EQ(stateHeapMocs, cmd.getInstructionMemoryObjectControlState());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, GivenBlockingWhenFlushingTaskThenPipeControlProgrammedCorrectly) {
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    // Configure the CSR to not need to submit any state or commands
    configureCSRtoNonDirtyState<FamilyType>(true);

    // Force a PIPE_CONTROL through a blocking flag
    auto blocking = true;
    auto &commandStreamTask = commandQueue.getCS(1024);
    auto &commandStreamCSR = commandStreamReceiver->getCS();
    commandStreamReceiver->lastSentCoherencyRequest = 0;

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.blocking = blocking;
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    commandStreamReceiver->flushTask(
        commandStreamTask,
        0,
        dsh,
        ioh,
        ssh,
        taskLevel,
        dispatchFlags,
        *pDevice);

    // Verify that taskCS got modified, while csrCS remained intact
    EXPECT_GT(commandStreamTask.getUsed(), 0u);
    EXPECT_EQ(0u, commandStreamCSR.getUsed());

    // Parse command list to verify that PC got added to taskCS
    cmdList.clear();
    parseCommands<FamilyType>(commandStreamTask, 0);
    auto itorTaskCS = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itorTaskCS);

    // Parse command list to verify that PC wasn't added to csrCS
    cmdList.clear();
    parseCommands<FamilyType>(commandStreamCSR, 0);
    auto numberOfPC = getCommandsList<PIPE_CONTROL>().size();
    EXPECT_EQ(0u, numberOfPC);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenCsrInNonDirtyStateWhenflushTaskIsCalledThenNoFlushIsCalled) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    configureCSRtoNonDirtyState<FamilyType>(true);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0, mockCsr->flushCalledCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenCsrInNonDirtyStateAndBatchingModeWhenflushTaskIsCalledWithDisabledPreemptionThenSubmissionIsNotRecorded) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>(true);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    mockCsr->flushTask(commandStream,
                       0,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0, mockCsr->flushCalledCount);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());

    //surfaces are non resident
    auto &surfacesForResidency = mockCsr->getResidencyAllocations();
    EXPECT_EQ(0u, surfacesForResidency.size());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenCsrInBatchingModeWhenRecordedBatchBufferIsBeingSubmittedThenFlushIsCalledWithRecordedCommandBuffer) {
    CommandQueueHw<FamilyType> commandQueue(nullptr, pClDevice, 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);
    mockCsr->useNewResourceImplicitFlush = false;
    mockCsr->useGpuIdleImplicitFlush = false;
    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    configureCSRtoNonDirtyState<FamilyType>(true);
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    dispatchFlags.requiresCoherency = true;

    mockCsr->lastSentCoherencyRequest = 1;

    commandStream.getSpace(4);

    mockCsr->flushTask(commandStream,
                       4,
                       dsh,
                       ioh,
                       ssh,
                       taskLevel,
                       dispatchFlags,
                       *pDevice);

    EXPECT_EQ(0, mockCsr->flushCalledCount);

    auto &surfacesForResidency = mockCsr->getResidencyAllocations();
    EXPECT_EQ(0u, surfacesForResidency.size());

    auto &cmdBufferList = mockedSubmissionsAggregator->peekCommandBuffers();
    EXPECT_FALSE(cmdBufferList.peekIsEmpty());
    auto cmdBuffer = cmdBufferList.peekHead();

    //preemption allocation + sip kernel
    size_t csrSurfaceCount = (pDevice->getPreemptionMode() == PreemptionMode::MidThread) ? 2 : 0;
    csrSurfaceCount += mockCsr->globalFenceAllocation ? 1 : 0;
    csrSurfaceCount += mockCsr->clearColorAllocation ? 1 : 0;

    EXPECT_EQ(4u + csrSurfaceCount, cmdBuffer->surfaces.size());

    //copy those surfaces
    std::vector<GraphicsAllocation *> residentSurfaces = cmdBuffer->surfaces;

    for (auto &graphicsAllocation : residentSurfaces) {
        EXPECT_TRUE(graphicsAllocation->isResident(mockCsr->getOsContext().getContextId()));
        EXPECT_EQ(1u, graphicsAllocation->getResidencyTaskCount(mockCsr->getOsContext().getContextId()));
    }

    mockCsr->flushBatchedSubmissions();

    EXPECT_FALSE(mockCsr->recordedCommandBuffer->batchBuffer.low_priority);
    EXPECT_TRUE(mockCsr->recordedCommandBuffer->batchBuffer.requiresCoherency);
    EXPECT_EQ(mockCsr->recordedCommandBuffer->batchBuffer.commandBufferAllocation, commandStream.getGraphicsAllocation());
    EXPECT_EQ(4u, mockCsr->recordedCommandBuffer->batchBuffer.startOffset);
    EXPECT_EQ(1, mockCsr->flushCalledCount);

    EXPECT_TRUE(mockedSubmissionsAggregator->peekCommandBuffers().peekIsEmpty());

    EXPECT_EQ(0u, surfacesForResidency.size());

    for (auto &graphicsAllocation : residentSurfaces) {
        EXPECT_FALSE(graphicsAllocation->isResident(mockCsr->getOsContext().getContextId()));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenNothingToFlushWhenFlushTaskCalledThenDontFlushStamp) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    configureCSRtoNonDirtyState<FamilyType>(true);

    EXPECT_EQ(0, mockCsr->flushCalledCount);
    auto previousFlushStamp = mockCsr->flushStamp->peekStamp();
    auto cmplStamp = flushTask(*mockCsr);
    EXPECT_EQ(mockCsr->flushStamp->peekStamp(), previousFlushStamp);
    EXPECT_EQ(previousFlushStamp, cmplStamp.flushStamp);
    EXPECT_EQ(0, mockCsr->flushCalledCount);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandStreamReceiverFlushTaskXeHPPlusTests, givenEpilogueRequiredFlagWhenTaskIsSubmittedDirectlyThenItPointsBackToCsr) {
    configureCSRtoNonDirtyState<FamilyType>(true);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    EXPECT_EQ(0u, commandStreamReceiver.getCmdSizeForEpilogue(dispatchFlags));

    dispatchFlags.epilogueRequired = true;
    dispatchFlags.preemptionMode = PreemptionHelper::getDefaultPreemptionMode(pDevice->getHardwareInfo());

    EXPECT_EQ(MemoryConstants::cacheLineSize, commandStreamReceiver.getCmdSizeForEpilogue(dispatchFlags));

    auto data = commandStream.getSpace(MemoryConstants::cacheLineSize);
    memset(data, 0, MemoryConstants::cacheLineSize);
    commandStreamReceiver.storeMakeResidentAllocations = true;
    commandStreamReceiver.flushTask(commandStream,
                                    0,
                                    dsh,
                                    ioh,
                                    ssh,
                                    taskLevel,
                                    dispatchFlags,
                                    *pDevice);
    auto &commandStreamReceiverStream = commandStreamReceiver.getCS(0u);

    EXPECT_EQ(MemoryConstants::cacheLineSize * 2, commandStream.getUsed());
    EXPECT_EQ(MemoryConstants::cacheLineSize, commandStreamReceiverStream.getUsed());

    parseCommands<FamilyType>(commandStream, 0);

    auto itBBend = find<typename FamilyType::MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(itBBend, cmdList.end());

    auto itBatchBufferStart = find<typename FamilyType::MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(itBatchBufferStart, cmdList.end());

    auto batchBufferStart = genCmdCast<typename FamilyType::MI_BATCH_BUFFER_START *>(*itBatchBufferStart);
    EXPECT_EQ(batchBufferStart->getBatchBufferStartAddressGraphicsaddress472(), commandStreamReceiverStream.getGraphicsAllocation()->getGpuAddress());

    parseCommands<FamilyType>(commandStreamReceiverStream, 0);

    itBBend = find<typename FamilyType::MI_BATCH_BUFFER_END *>(cmdList.begin(), cmdList.end());
    void *bbEndAddress = *itBBend;

    EXPECT_EQ(commandStreamReceiverStream.getCpuBase(), bbEndAddress);

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(commandStreamReceiverStream.getGraphicsAllocation()));
}