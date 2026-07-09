import { describe, expect, it } from 'vitest';

import {
  evaluateAssertions,
  selectCaptureValue,
  selectCaptureValues,
  withServerTimeout,
} from '../test-runner-response-utils.mjs';

describe('test runner response helpers', () => {
  it('accepts numeric assertions within an explicit tolerance', () => {
    const result = evaluateAssertions(
      {
        assertions: [
          {
            path: 'structuredContent.result.rotation.pitch',
            approximately: 35,
            tolerance: 0.001,
            label: 'restored pitch',
          },
        ],
      },
      {
        structuredContent: {
          result: {
            rotation: {
              pitch: 35.000000000000014,
            },
          },
        },
      },
    );

    expect(result).toEqual({ passed: true });
  });

  it('rejects numeric assertions outside their explicit tolerance', () => {
    const result = evaluateAssertions(
      {
        assertions: [
          {
            path: 'structuredContent.result.rotation.pitch',
            approximately: 35,
            tolerance: 0.001,
            label: 'restored pitch',
          },
        ],
      },
      {
        structuredContent: {
          result: {
            rotation: {
              pitch: 35.01,
            },
          },
        },
      },
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('within 0.001');
  });

  it('captures a selected field from the matching array item', () => {
    const value = selectCaptureValue(
      {
        result: {
          components: [
            { name: 'Sprite', class: 'BillboardComponent' },
            { name: 'SkyLightComponent0', class: 'SkyLightComponent' },
          ],
        },
      },
      {
        fromField: 'result.components',
        where: { path: 'class', includes: 'SkyLightComponent' },
        selectField: 'name',
      },
    );

    expect(value).toBe('SkyLightComponent0');
  });

  it('captures multiple cleanup paths from one response', () => {
    const values = selectCaptureValues(
      {
        result: {
          sequencePackagePath:
            '/Game/Cinematics/Takes/2026-06-10/Scene_1_07',
          subsceneFolderPath:
            '/Game/Cinematics/Takes/2026-06-10/Scene_1_07_Subscenes',
        },
      },
      [
        {
          key: 'takeSequencePackagePath',
          fromField: 'result.sequencePackagePath',
        },
        {
          key: 'takeSubsceneFolderPath',
          fromField: 'result.subsceneFolderPath',
        },
      ],
    );

    expect(values).toEqual([
      {
        key: 'takeSequencePackagePath',
        value: '/Game/Cinematics/Takes/2026-06-10/Scene_1_07',
      },
      {
        key: 'takeSubsceneFolderPath',
        value:
          '/Game/Cinematics/Takes/2026-06-10/Scene_1_07_Subscenes',
      },
    ]);
  });

  it('checks string fragments before dependent test actions run', () => {
    const result = evaluateAssertions(
      {
        assertions: [
          {
            path: 'structuredContent.result.directionalLightActorPath',
            includes: 'SnapshotSun_123',
            label: 'snapshot directional light selection',
          },
        ],
      },
      {
        structuredContent: {
          result: {
            directionalLightActorPath:
              '/Game/Test.Test:PersistentLevel.SnapshotSun_123',
          },
        },
      },
    );

    expect(result).toEqual({ passed: true });
  });

  it('does not let a generic error satisfy a requested error code', async () => {
    const { evaluateExpectation } = await import('../test-runner.mjs');
    const result = evaluateExpectation(
      { expected: 'error|mrq_resource_limit_exceeded' },
      {
        isError: true,
        content: [{ type: 'text', text: 'Error: actor was not found' }],
        structuredContent: {
          success: false,
          errorCode: 'ACTOR_NOT_FOUND',
        },
      },
    );

    expect(result.passed).toBe(false);
  });

  it.each([0, -1, 3_600_001])(
    'preserves an explicit server timeout value of %s',
    (timeoutMs) => {
      expect(
        withServerTimeout(
          {
            name: 'manage_sequence',
            arguments: { action: 'start_render', timeoutMs },
          },
          5000,
        ),
      ).toEqual({
        name: 'manage_sequence',
        arguments: { action: 'start_render', timeoutMs },
      });
    },
  );

  it('injects the harness timeout when the tool call omits timeoutMs', () => {
    expect(
      withServerTimeout(
        {
          name: 'manage_sequence',
          arguments: { action: 'start_render' },
        },
        5000,
      ),
    ).toEqual({
      name: 'manage_sequence',
      arguments: { action: 'start_render', timeoutMs: 5000 },
    });
  });
});
