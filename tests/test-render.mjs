import { TestRunner } from './test-runner.mjs';

const runner = new TestRunner('Render Tools Tests');
const timestamp = Date.now();
const rtPath = `/Game/Tests/Render/RT_Test_${timestamp}`;

runner.addStep('Create Render Target', async (tools) => {
  const result = await tools.executeTool('manage_render', {
    action: 'create_render_target',
    assetPath: rtPath,
    width: 256,
    height: 256,
    format: 'RTF_R8'
  });
  return result.success;
});

runner.addStep('Lumen Update Scene', async (tools) => {
    const result = await tools.executeTool('manage_render', {
        action: 'lumen_update_scene'
    });
    return result.success;
});

runner.addStep('Cleanup Render Target', async (tools) => {
    await tools.executeTool('manage_asset', {
        action: 'delete',
        assetPath: rtPath
    });
    return true;
});

runner.run().catch(console.error);
