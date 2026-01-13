import { BenchmarkRunner } from './benchmark-runner.mjs';

const runner = new BenchmarkRunner('Unreal MCP Performance Baseline');

// 1. Actor List (Small)
runner.addBenchmark('List Actors (All)', async (tools) => {
  await tools.executeTool('control_actor', { action: 'list' });
}, 10);

// 2. Asset Query
runner.addBenchmark('List Assets (/Engine/BasicShapes)', async (tools) => {
  await tools.executeTool('manage_asset', { action: 'list_assets', path: '/Engine/BasicShapes' });
}, 10);

// 3. Property Get (Reflection)
runner.addBenchmark('Get Actor Transform', async (tools) => {
  // Assuming 'Floor' exists in default map, or we spawn one
  // For baseline, we can try to spawn a temp actor if needed, but let's assume a known actor or handle error
  // Better: Spawn one first if not present
  // For simplicity, we'll try to get transform of a likely actor, or just fail cleanly
  try {
     // Try creating a temporary actor for the benchmark session if possible, but 
     // usually benchmarks run in an existing level. 
     // Let's assume 'Floor' or similar exists, or spawn 'BenchCube'
     await tools.executeTool('control_actor', { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'BenchCube' });
  } catch (e) {
     // Ignore if already exists
  }
  
  await tools.executeTool('control_actor', { action: 'get_transform', actorName: 'BenchCube' });
}, 20);

runner.run();
