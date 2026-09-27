import { describe, expect, it, vi } from 'vitest';
import { AssetResources } from './assets.js';
import { AutomationBridge } from '../automation/index.js';
import { UnrealBridge } from '../unreal-bridge.js';

function createAssetResources(result: Record<string, unknown>) {
  const sendAutomationRequest = vi.fn(async (_action: string, _payload?: Record<string, unknown>) => ({ success: true, result }));
  const automationBridge = new AutomationBridge({ enabled: false });
  vi.spyOn(automationBridge, 'isConnected').mockReturnValue(true);
  vi.spyOn(automationBridge, 'sendAutomationRequest').mockImplementation(sendAutomationRequest);
  const unrealBridge = new UnrealBridge();
  unrealBridge.setAutomationBridge(automationBridge);
  return { resources: new AssetResources(unrealBridge), sendAutomationRequest };
}

describe('AssetResources.list', () => {
  it('asks manage_asset to list the requested directory by its canonical path field', async () => {
    const { resources, sendAutomationRequest } = createAssetResources({ folders_list: [], assets: [] });

    await resources.list('/Game/MyFolder', 25);

    expect(sendAutomationRequest).toHaveBeenCalledTimes(1);
    const [tool, payload] = sendAutomationRequest.mock.calls[0] as [string, Record<string, unknown>];
    expect(tool).toBe('manage_asset');
    expect(payload).toMatchObject({ action: 'list', path: '/Game/MyFolder', limit: 25, recursive: false });
    expect(payload).not.toHaveProperty('directory');
  });

  it('returns folders first, then assets, as Name/Path/Class rows', async () => {
    const { resources } = createAssetResources({
      folders_list: [{ n: 'Sub', p: '/Game/MyFolder/Sub' }],
      assets: [{ n: 'A', p: '/Game/MyFolder/A', c: 'StaticMesh' }]
    });

    const listed = await resources.list('/Game/MyFolder');

    expect(listed).toMatchObject({ success: true, count: 2, folders: 1, files: 1 });
    expect(listed.assets).toEqual([
      { Name: 'Sub', Path: '/Game/MyFolder/Sub', Class: 'Folder', isFolder: true },
      { Name: 'A', Path: '/Game/MyFolder/A', Class: 'StaticMesh' }
    ]);
  });
});
