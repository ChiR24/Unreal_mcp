import { createRequire } from 'node:module';
import { describe, expect, it } from 'vitest';
import {
  STDIO_REPLAY_ACTION_LABELS,
  validateStdioReplayEvidenceDocument
} from '../../scripts/cinematics-live-evidence.mjs';
import { evaluateExpectation } from '../test-runner.mjs';

const require = createRequire(import.meta.url);
const {
  summarizeCinematicsCoverage
} = require(
  '../mcp-tools/utility/cinematics-media-cases.cjs'
);

describe('test runner specific error alternatives', () => {
  it('rejects an unrelated error when a specific error alternative is required', () => {
    const result = evaluateExpectation(
      { expected: 'error|invalid_sequence' },
      {
        isError: true,
        structuredContent: {
          success: false,
          error: 'ACTOR_NOT_FOUND',
          message: 'Actor not found'
        }
      }
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('invalid_sequence');
  });

  it('does not treat 1006 inside response metadata as a connection crash', () => {
    const result = evaluateExpectation(
      { expected: 'error|asset_not_found|not found' },
      {
        isError: true,
        structuredContent: {
          requestId: 'request-1006-metadata',
          success: false,
          error: 'ASSET_NOT_FOUND',
          message: 'No assets deleted. 1 path not found.'
        }
      }
    );

    expect(result.passed).toBe(true);
  });

  it('rejects a transport crash reported only in MCP text content', () => {
    const result = evaluateExpectation(
      { expected: 'error' },
      {
        isError: true,
        content: [{
          type: 'text',
          text: 'WebSocket closed with code 1006 before the response completed'
        }]
      }
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('Crash/connection loss');
  });

  it('rejects a socket hang up reported only in MCP text content', () => {
    const result = evaluateExpectation(
      { expected: 'error' },
      {
        isError: true,
        content: [{ type: 'text', text: 'socket hang up' }]
      }
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('Crash/connection loss');
  });

  it('rejects standalone close code 1006 in an error field', () => {
    const result = evaluateExpectation(
      { expected: 'error' },
      {
        isError: true,
        structuredContent: {
          success: false,
          error: '1006',
          message: 'Transport failed'
        }
      }
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('Crash/connection loss');
  });

  it('does not treat successful content mentioning not connected as a crash', () => {
    const result = evaluateExpectation(
      { expected: 'success' },
      {
        isError: false,
        content: [{
          type: 'text',
          text: 'Success: not connected nodes were skipped'
        }]
      }
    );

    expect(result.passed).toBe(true);
  });

  it('requires an object error pattern to match the actual error code', () => {
    const result = evaluateExpectation(
      {
        expected: {
          condition: 'error',
          errorPattern: 'SECURITY_VIOLATION'
        }
      },
      {
        isError: true,
        structuredContent: {
          success: false,
          error: 'MRQ_JOB_NOT_FOUND',
          message: 'Movie Render Queue job not found'
        }
      }
    );

    expect(result.passed).toBe(false);
    expect(result.reason).toContain('Error pattern not matched');
  });
});

describe('cinematics and media coverage accounting', () => {
  it('ignores known legacy support actions', () => {
    const coverage = summarizeCinematicsCoverage([{
      toolName: 'manage_sequence',
      arguments: { action: 'add_actor' }
    }]);

    expect(coverage.extraActions).toEqual([]);
  });

  it('reports unknown manage_sequence actions as extras', () => {
    const coverage = summarizeCinematicsCoverage([{
      toolName: 'manage_sequence',
      arguments: { action: 'create_media_playre' }
    }]);

    expect(coverage.extraActions).toEqual(['create_media_playre']);
  });
});

function replayEvidence(overrides = {}) {
  const actionRecords = Object.values(STDIO_REPLAY_ACTION_LABELS)
    .flat()
    .map((label) => ({
      label,
      passed: true,
      structuredContent: { success: true }
    }));
  return {
    generatedAt: '2026-06-09T10:05:00.000Z',
    allPassed: true,
    replayName: 'CinematicsDirectReplay_1',
    replayPath:
      '/data/Game/MCPtest/Saved/Demos/CinematicsDirectReplay_1.replay',
    replayRemoved: true,
    records: [
      { label: 'start PIE', passed: true, structuredContent: { success: true } },
      ...actionRecords,
      {
        label: 'stop PIE cleanup',
        passed: true,
        structuredContent: { success: true }
      }
    ],
    ...overrides
  };
}

const editorIdentity = {
  pid: 4242,
  command: '/data/UnrealEngine/Engine/Binaries/Linux/UnrealEditor',
  projectPath: '/data/Game/MCPtest/MCPtest.uproject',
  startedAtMs: Date.parse('2026-06-09T10:00:00.000Z')
};

describe('cinematics and media stdio replay evidence', () => {
  it('verifies every delegated action against the current editor project', () => {
    const result = validateStdioReplayEvidenceDocument(replayEvidence(), {
      artifactPath: '/tmp/cinematics-stdio.json',
      projectPath: editorIdentity.projectPath,
      editorIdentity,
      nowMs: Date.parse('2026-06-09T10:10:00.000Z')
    });

    expect(result.verifiedActions.map(({ action }) => action)).toEqual(
      Object.keys(STDIO_REPLAY_ACTION_LABELS)
    );
    expect(result.verifiedActions.every(({ status }) => status === 'passed'))
      .toBe(true);
  });

  it('rejects evidence that omits a delegated action', () => {
    const evidence = replayEvidence();
    evidence.records = evidence.records.filter(
      ({ label }) => label !== 'seek demo playback'
    );

    expect(() => validateStdioReplayEvidenceDocument(evidence, {
      artifactPath: '/tmp/cinematics-stdio.json',
      projectPath: editorIdentity.projectPath,
      editorIdentity,
      nowMs: Date.parse('2026-06-09T10:10:00.000Z')
    })).toThrow(/seek_demo/);
  });

  it('rejects evidence from before the current editor session', () => {
    expect(() => validateStdioReplayEvidenceDocument(
      replayEvidence({ generatedAt: '2026-06-09T09:59:00.000Z' }),
      {
        artifactPath: '/tmp/cinematics-stdio.json',
        projectPath: editorIdentity.projectPath,
        editorIdentity,
        nowMs: Date.parse('2026-06-09T10:10:00.000Z')
      }
    )).toThrow(/predates the live editor/);
  });
});
