import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock, exportEnvironmentSnapshotMock, importEnvironmentSnapshotMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true, result: {} })),
  exportEnvironmentSnapshotMock: vi.fn(async () => ({ success: true })),
  importEnvironmentSnapshotMock: vi.fn(async () => ({ success: true }))
}));

vi.mock('./common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('./common-handlers.js')>('./common-handlers.js');
  return {
    ...actual,
    executeAutomationRequest: executeAutomationRequestMock
  };
});

vi.mock('../../utils/environment-snapshot.js', () => ({
  exportEnvironmentSnapshot: exportEnvironmentSnapshotMock,
  importEnvironmentSnapshot: importEnvironmentSnapshotMock
}));

import { handleEnvironmentTools } from './environment-handlers.js';

describe('handleEnvironmentTools path normalization', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
    exportEnvironmentSnapshotMock.mockClear();
    importEnvironmentSnapshotMock.mockClear();
  });

  it('normalizes landscape material path aliases before dispatch', async () => {
    await handleEnvironmentTools('create_landscape', {
      action: 'create_landscape',
      name: 'TestLandscape',
      materialPath: 'Content/MCPTest/Materials/M_Landscape'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'create_landscape',
      expect.objectContaining({
        materialPath: '/Game/MCPTest/Materials/M_Landscape'
      })
    );
  });

  it('normalizes foliage asset path aliases before dispatch', async () => {
    await handleEnvironmentTools('add_foliage', {
      action: 'add_foliage',
      name: 'TestFoliage',
      meshPath: 'Engine/BasicShapes/Sphere'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'add_foliage_type',
      expect.objectContaining({
        meshPath: '/Engine/BasicShapes/Sphere'
      })
    );
  });

  it('normalizes existing foliage type aliases before dispatch', async () => {
    await handleEnvironmentTools('paint_foliage', {
      action: 'paint_foliage',
      foliageType: 'Game/Foliage/TestFoliage',
      locations: [{ x: 0, y: 0, z: 100 }]
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'paint_foliage',
      expect.objectContaining({
        foliageType: '/Game/Foliage/TestFoliage'
      })
    );
  });

  it('normalizes procedural foliage nested mesh paths before dispatch', async () => {
    await handleEnvironmentTools('create_procedural_foliage', {
      action: 'create_procedural_foliage',
      volumeName: 'TestProceduralFoliage',
      foliageTypes: [{ meshPath: 'Content/Foliage/SM_Bush', density: 1 }]
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'create_procedural_foliage',
      expect.objectContaining({
        foliageTypes: [expect.objectContaining({ meshPath: '/Game/Foliage/SM_Bush' })],
        types: [expect.objectContaining({ meshPath: '/Game/Foliage/SM_Bush' })]
      })
    );
  });

  it('normalizes generate_lods asset path arrays before dispatch', async () => {
    await handleEnvironmentTools('generate_lods', {
      action: 'generate_lods',
      assetPaths: ['Engine/BasicShapes/Sphere', 'Content/MCPTest/SM_Rock'],
      numLODs: 2
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action: 'generate_lods',
        assetPaths: ['/Engine/BasicShapes/Sphere', '/Game/MCPTest/SM_Rock']
      }),
      'Bridge unavailable'
    );
  });

  it('preserves filesystem snapshot paths', async () => {
    await handleEnvironmentTools('export_snapshot', {
      action: 'export_snapshot',
      path: './tmp/unreal-mcp/build-environment',
      filename: 'snapshot.json'
    }, {} as never);

    expect(exportEnvironmentSnapshotMock).toHaveBeenCalledWith({
      path: './tmp/unreal-mcp/build-environment',
      filename: 'snapshot.json'
    });
  });
});
