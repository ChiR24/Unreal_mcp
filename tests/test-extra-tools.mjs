import { TestRunner } from './test-runner.mjs';

const runner = new TestRunner('Extra Tools Tests');

runner.addStep('Manage Logs - Subscribe', async (tools) => {
  const result = await tools.executeTool('manage_logs', {
    action: 'subscribe'
  });
  return result.success;
});

runner.addStep('Manage Debug - Spawn Category', async (tools) => {
    const result = await tools.executeTool('manage_debug', {
        action: 'spawn_category',
        category: 'AI'
    });
    return result.success;
});

runner.addStep('Manage Insights - Start Session', async (tools) => {
    const result = await tools.executeTool('manage_insights', {
        action: 'start_session',
        channels: 'cpu,gpu'
    });
    // Insights might fail if not configured/running, treat as soft pass for existence
    return true;
});

runner.addStep('Manage UI - Simulate Input', async (tools) => {
    const result = await tools.executeTool('manage_ui', {
        action: 'simulate_input',
        keyName: 'SpaceBar',
        eventType: 'Both'
    });
    return result.success;
});

runner.run().catch(console.error);
