import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { ITools } from '../../../types/tools/tool-interfaces.js';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true }))
}));

vi.mock('../foundation/dispatch/common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('../foundation/dispatch/common-handlers.js')>('../foundation/dispatch/common-handlers.js');
  return {
    ...actual,
    executeAutomationRequest: executeAutomationRequestMock
  };
});

import { handleSequenceTools } from './sequence-handlers.js';
import { manageSequenceToolDefinition } from '../../definitions/utility/manage-sequence-tool.js';

const tools = {
  systemTools: {
    executeConsoleCommand: vi.fn(async () => ({ success: true })),
    getProjectSettings: vi.fn(async () => ({}))
  },
  assetResources: {
    list: vi.fn(async () => ({}))
  }
} satisfies ITools;

describe('handleSequenceTools path normalization', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
  });

  it('normalizes sequence creation path aliases before dispatch', async () => {
    await handleSequenceTools('create', {
      action: 'create',
      name: 'SEQ_Test',
      path: 'Game/MCPTest/Sequences'
    }, tools);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      tools,
      'manage_sequence',
      expect.objectContaining({
        subAction: 'create',
        path: '/Game/MCPTest/Sequences'
      })
    );
  });

  it('normalizes duplicate source and destination path aliases before dispatch', async () => {
    await handleSequenceTools('duplicate', {
      action: 'duplicate',
      path: 'Game/MCPTest/Sequences/SEQ_Test',
      destinationPath: 'Content/MCPTest/Duplicates',
      newName: 'SEQ_Copy'
    }, tools);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      tools,
      'manage_sequence',
      expect.objectContaining({
        subAction: 'duplicate',
        path: '/Game/MCPTest/Sequences/SEQ_Test',
        destinationPath: '/Game/MCPTest/Duplicates/SEQ_Copy'
      })
    );
  });

  it('routes cinematics and media actions through manage_sequence with subAction', async () => {
    await handleSequenceTools('create_render_job', {
      action: 'create_render_job',
      sequencePath: 'Game/MCPTest/Cinematics/SEQ_Master',
      mapPath: 'Content/MCPTest/Maps/M_Cinematics',
      outputDirectory: '/tmp/cinematics-mrq',
      renderJobName: 'CinematicsJob'
    }, tools);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      tools,
      'manage_sequence',
      expect.objectContaining({
        subAction: 'create_render_job',
        sequencePath: '/Game/MCPTest/Cinematics/SEQ_Master',
        mapPath: '/Game/MCPTest/Maps/M_Cinematics',
        outputDirectory: '/tmp/cinematics-mrq'
      })
    );
  });

  it('forwards the MRQ timeout to Unreal with transport grace', async () => {
    await handleSequenceTools('start_render', {
      action: 'start_render',
      jobId: 'render-job-id',
      timeoutMs: 60000
    }, tools);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      tools,
      'manage_sequence',
      expect.objectContaining({
        subAction: 'start_render',
        timeoutMs: 60000
      }),
      undefined,
      {
        timeoutMs: 95000,
        forwardTimeoutMsToUnreal: true
      }
    );
  });

  it('uses a bounded default timeout for MRQ completion', async () => {
    await handleSequenceTools('start_render', {
      action: 'start_render',
      jobId: 'render-job-id'
    }, tools);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      tools,
      'manage_sequence',
      expect.objectContaining({ timeoutMs: 300000 }),
      undefined,
      {
        timeoutMs: 335000,
        forwardTimeoutMsToUnreal: true
      }
    );
  });

  it.each([0, -1, 3600001])(
    'preserves invalid MRQ timeout %s so Unreal returns the canonical error',
    async (timeoutMs) => {
      await handleSequenceTools('start_render', {
        action: 'start_render',
        jobId: 'render-job-id',
        timeoutMs
      }, tools);

      expect(executeAutomationRequestMock).toHaveBeenCalledWith(
        tools,
        'manage_sequence',
        expect.objectContaining({
          subAction: 'start_render',
          timeoutMs
        }),
        undefined,
        {
          timeoutMs: 335000,
          forwardTimeoutMsToUnreal: true
        }
      );
    }
  );

  it('normalizes live cinematics and media UE asset paths but leaves dead aliases unnormalized', async () => {
    await handleSequenceTools('create_media_texture', {
      action: 'create_media_texture',
      mediaPlayerPath: 'Game/MCPTest/Media/MP_Cinematics',
      mediaSourcePath: 'Content/MCPTest/Media/MS_Cinematics',
      mediaTexturePath: 'MCPTest/Media/MT_Cinematics',
      filePath: '/tmp/cinematics/source.mp4'
    }, tools);

    const payload = (executeAutomationRequestMock.mock.calls[0] as unknown as [ITools, string, Record<string, unknown>])[2];
    expect(payload).toEqual(expect.objectContaining({
      subAction: 'create_media_texture',
      mediaPlayerPath: '/Game/MCPTest/Media/MP_Cinematics',
      mediaSourcePath: '/Game/MCPTest/Media/MS_Cinematics',
      filePath: '/tmp/cinematics/source.mp4'
    }));
    expect(payload.mediaTexturePath).toBe('MCPTest/Media/MT_Cinematics');
  });

  it('does not normalize verified-dead media path aliases (schema-drift guard)', async () => {
    await handleSequenceTools('create_media_texture', {
      action: 'create_media_texture',
      mediaTexturePath: 'MCPTest/Media/MT_Cinematics',
      mediaPlaylistPath: 'Content/MCPTest/Media/MPL_Cinematics'
    }, tools);

    const payload = (executeAutomationRequestMock.mock.calls[0] as unknown as [ITools, string, Record<string, unknown>])[2];
    expect(payload.mediaTexturePath).toBe('MCPTest/Media/MT_Cinematics');
    expect(payload.mediaPlaylistPath).toBe('Content/MCPTest/Media/MPL_Cinematics');
    expect(payload).not.toEqual(expect.objectContaining({
      mediaTexturePath: '/Game/MCPTest/Media/MT_Cinematics'
    }));
    expect(payload).not.toEqual(expect.objectContaining({
      mediaPlaylistPath: '/Game/MCPTest/Media/MPL_Cinematics'
    }));
  });

  it('normalizes every advertised cinematics and media UE path alias before dispatch', async () => {
    await handleSequenceTools('add_shot_track', {
      action: 'add_shot_track',
      assetPath: 'Content/MCPTest/Cinematics/SEQ_Asset',
      sequencePath: 'Game/MCPTest/Cinematics/SEQ_Master',
      masterSequencePath: 'MCPTest/Cinematics/SEQ_Master',
      subsequencePath: 'Content/MCPTest/Cinematics/SEQ_Sub',
      shotSequencePath: 'MCPTest/Cinematics/SEQ_Shot',
      mapPath: 'Content/MCPTest/Maps/M_Cinematics',
      playlistPath: 'Game/MCPTest/Media/MPL_Cinematics',
      playerPath: 'MCPTest/Media/MP_Cinematics',
      sourcePath: 'Content/MCPTest/Media/MS_Cinematics',
      defaultSourcePath: 'MCPTest/Media/MS_Default',
      sourcePaths: [
        'Content/MCPTest/Media/MS_Cinematics_A',
        'MCPTest/Media/MS_Cinematics_B'
      ],
      platformSources: {
        Linux: 'Content/MCPTest/Media/MS_Linux',
        Windows: 'MCPTest/Media/MS_Windows'
      },
      animationSequencePath: 'Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle',
      cameraShakeClass: 'Script/EngineCameras.DefaultCameraShakeBase',
      takeSequencePath: 'Content/MCPTest/Cinematics/SEQ_Take',
      executorClass:
        'Script/MovieRenderPipelineEditor.MoviePipelinePIEExecutor',
      burnIn: {
        enabled: true,
        classPath: 'Script/MovieRenderPipelineCore.MoviePipelineBurnInWidget'
      },
      filePaths: ['/tmp/cinematics/source.webm'],
      urls: ['https://example.invalid/cinematics.m3u8']
    }, tools);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      tools,
      'manage_sequence',
      expect.objectContaining({
        subAction: 'add_shot_track',
        assetPath: '/Game/MCPTest/Cinematics/SEQ_Asset',
        sequencePath: '/Game/MCPTest/Cinematics/SEQ_Master',
        masterSequencePath: '/Game/MCPTest/Cinematics/SEQ_Master',
        subsequencePath: '/Game/MCPTest/Cinematics/SEQ_Sub',
        shotSequencePath: '/Game/MCPTest/Cinematics/SEQ_Shot',
        mapPath: '/Game/MCPTest/Maps/M_Cinematics',
        playlistPath: '/Game/MCPTest/Media/MPL_Cinematics',
        playerPath: '/Game/MCPTest/Media/MP_Cinematics',
        sourcePath: '/Game/MCPTest/Media/MS_Cinematics',
        defaultSourcePath: '/Game/MCPTest/Media/MS_Default',
        sourcePaths: [
          '/Game/MCPTest/Media/MS_Cinematics_A',
          '/Game/MCPTest/Media/MS_Cinematics_B'
        ],
        platformSources: {
          Linux: '/Game/MCPTest/Media/MS_Linux',
          Windows: '/Game/MCPTest/Media/MS_Windows'
        },
        animationSequencePath: '/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle',
        cameraShakeClass: '/Script/EngineCameras.DefaultCameraShakeBase',
        takeSequencePath: '/Game/MCPTest/Cinematics/SEQ_Take',
        executorClass:
          '/Script/MovieRenderPipelineEditor.MoviePipelinePIEExecutor',
        burnIn: {
          enabled: true,
          classPath:
            '/Script/MovieRenderPipelineCore.MoviePipelineBurnInWidget'
        },
        filePaths: ['/tmp/cinematics/source.webm'],
        urls: ['https://example.invalid/cinematics.m3u8']
      })
    );
  });
});

describe('manage_sequence value and frame-rate schema', () => {
  it('advertises every value shape accepted by sequence handlers', () => {
    const properties = manageSequenceToolDefinition.inputSchema.properties;
    if (typeof properties !== 'object' || properties === null || Array.isArray(properties)) {
      throw new TypeError('manage_sequence properties schema is unavailable');
    }

    expect(properties).toEqual(expect.objectContaining({
      value: {
        description: 'Generic value (any type).'
      },
      frameRate: {
        type: ['number', 'string'],
        description: expect.stringContaining('24000/1001')
      }
    }));
  });

  it('does not advertise particle asset assignment for activation tracks', () => {
    const properties = manageSequenceToolDefinition.inputSchema.properties;
    if (typeof properties !== 'object' || properties === null || Array.isArray(properties)) {
      throw new TypeError('manage_sequence properties schema is unavailable');
    }

    expect(Object.hasOwn(properties, 'particleSystemPath')).toBe(false);
  });
});
