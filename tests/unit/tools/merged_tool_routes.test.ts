import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../src/types/tool-interfaces.js';
import type { UnrealBridge } from '../../../src/unreal-bridge.js';
import type { AutomationBridge } from '../../../src/automation/index.js';

const routeMocks = vi.hoisted(() => ({
  handleGraphTools: vi.fn(),
  handleAnimationAuthoringTools: vi.fn(),
  handleAudioAuthoringTools: vi.fn(),
  handleNiagaraAuthoringTools: vi.fn(),
}));

vi.mock('../../../src/tools/handlers/graph-handlers.js', () => ({
  handleGraphTools: routeMocks.handleGraphTools,
}));

vi.mock('../../../src/tools/handlers/animation-authoring-handlers.js', () => ({
  handleAnimationAuthoringTools: routeMocks.handleAnimationAuthoringTools,
}));

vi.mock('../../../src/tools/handlers/audio-authoring-handlers.js', () => ({
  handleAudioAuthoringTools: routeMocks.handleAudioAuthoringTools,
}));

vi.mock('../../../src/tools/handlers/niagara-authoring-handlers.js', () => ({
  handleNiagaraAuthoringTools: routeMocks.handleNiagaraAuthoringTools,
}));

import { handleConsolidatedToolCall } from '../../../src/tools/consolidated-tool-handlers.js';
import { AnimationTools } from '../../../src/tools/animation.js';

describe('Merged Tool Routes', () => {
  let mockTools: ITools;

  beforeEach(() => {
    vi.clearAllMocks();
    routeMocks.handleGraphTools.mockResolvedValue({ success: true });
    routeMocks.handleAnimationAuthoringTools.mockResolvedValue({ success: true });
    routeMocks.handleAudioAuthoringTools.mockResolvedValue({ success: true });
    routeMocks.handleNiagaraAuthoringTools.mockResolvedValue({ success: true });

    mockTools = {
      automationBridge: {
        isConnected: vi.fn().mockReturnValue(true),
        sendAutomationRequest: vi.fn(),
      },
    } as unknown as ITools;
  });

  it('routes canonical blueprint graph actions through manage_blueprint before the compatibility shim', async () => {
    await handleConsolidatedToolCall(
      'manage_blueprint',
      {
        action: 'create_node',
        blueprintPath: '/Game/IntegrationTest/BP_RouteCleanup',
        graphName: 'EventGraph',
        nodeType: 'Branch',
      },
      mockTools,
    );

    expect(routeMocks.handleGraphTools).toHaveBeenCalledWith(
      'manage_blueprint',
      'create_node',
      expect.objectContaining({
        action: 'create_node',
        blueprintPath: '/Game/IntegrationTest/BP_RouteCleanup',
        graphName: 'EventGraph',
        nodeType: 'Branch',
      }),
      mockTools,
    );
  });

  it('keeps manage_blueprint_graph as an explicit compatibility alias', async () => {
    await handleConsolidatedToolCall(
      'manage_blueprint_graph',
      {
        action: 'create_node',
        blueprintPath: '/Game/IntegrationTest/BP_RouteCleanup',
        graphName: 'EventGraph',
        nodeType: 'Branch',
      },
      mockTools,
    );

    expect(routeMocks.handleGraphTools).toHaveBeenCalledWith(
      'manage_blueprint_graph',
      'create_node',
      expect.objectContaining({
        action: 'create_node',
        blueprintPath: '/Game/IntegrationTest/BP_RouteCleanup',
      }),
      mockTools,
    );
  });

  it('routes canonical merged authoring actions through animation_physics, manage_audio, and manage_effect', async () => {
    await handleConsolidatedToolCall(
      'animation_physics',
      {
        action: 'create_animation_sequence',
        name: 'AN_AnimationRouteCleanup',
        skeletonPath: '/Game/Characters/SK_Test',
      },
      mockTools,
    );

    await handleConsolidatedToolCall(
      'manage_audio',
      {
        action: 'add_cue_node',
        assetPath: '/Game/Audio/SC_AudioRouteCleanup',
        nodeType: 'WavePlayer',
      },
      mockTools,
    );

    await handleConsolidatedToolCall(
      'manage_effect',
      {
        action: 'add_emitter_to_system',
        systemPath: '/Game/FX/NS_EffectRouteCleanup',
        emitterPath: '/Game/FX/NE_EffectRouteCleanup',
      },
      mockTools,
    );

    expect(routeMocks.handleAnimationAuthoringTools).toHaveBeenCalledWith(
      'create_animation_sequence',
      expect.objectContaining({ action: 'create_animation_sequence' }),
      mockTools,
    );
    expect(routeMocks.handleAudioAuthoringTools).toHaveBeenCalledWith(
      'add_cue_node',
      expect.objectContaining({ action: 'add_cue_node' }),
      mockTools,
    );
    expect(routeMocks.handleNiagaraAuthoringTools).toHaveBeenCalledWith(
      'add_emitter_to_system',
      expect.objectContaining({ action: 'add_emitter_to_system' }),
      mockTools,
    );
  });

  it('keeps deprecated merged tool aliases explicit and backward compatible', async () => {
    await handleConsolidatedToolCall(
      'manage_animation_authoring',
      {
        action: 'create_animation_sequence',
        name: 'AN_AnimationAlias',
        skeletonPath: '/Game/Characters/SK_Test',
      },
      mockTools,
    );

    await handleConsolidatedToolCall(
      'manage_audio_authoring',
      {
        action: 'add_cue_node',
        assetPath: '/Game/Audio/SC_AudioAlias',
        nodeType: 'WavePlayer',
      },
      mockTools,
    );

    await handleConsolidatedToolCall(
      'manage_niagara_authoring',
      {
        action: 'add_emitter_to_system',
        systemPath: '/Game/FX/NS_EffectAlias',
        emitterPath: '/Game/FX/NE_EffectAlias',
      },
      mockTools,
    );

    expect(routeMocks.handleAnimationAuthoringTools).toHaveBeenCalledWith(
      'create_animation_sequence',
      expect.objectContaining({ action: 'create_animation_sequence' }),
      mockTools,
    );
    expect(routeMocks.handleAudioAuthoringTools).toHaveBeenCalledWith(
      'add_cue_node',
      expect.objectContaining({ action: 'add_cue_node' }),
      mockTools,
    );
    expect(routeMocks.handleNiagaraAuthoringTools).toHaveBeenCalledWith(
      'add_emitter_to_system',
      expect.objectContaining({ action: 'add_emitter_to_system' }),
      mockTools,
    );
  });

  it('uses animation_physics for direct animation wrapper authoring flows', async () => {
    const sendAutomationRequest = vi.fn().mockResolvedValue({
      success: true,
      message: 'State machine created',
      result: {
        stateMachineName: 'Locomotion',
      },
    });

    const animationTools = new AnimationTools(
      {} as UnrealBridge,
      {
        sendAutomationRequest,
      } as unknown as AutomationBridge,
    );

    await animationTools.createStateMachine({
      blueprintPath: '/Game/Animations/ABP_AnimationRouteCleanup',
      machineName: 'Locomotion',
    });

    expect(sendAutomationRequest).toHaveBeenCalledWith(
      'animation_physics',
      expect.objectContaining({
        action: 'add_state_machine',
        blueprintPath: '/Game/Animations/ABP_AnimationRouteCleanup',
        stateMachineName: 'Locomotion',
      }),
      { timeoutMs: 60000 },
    );
  });
});