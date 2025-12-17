#!/usr/bin/env node
/**
 * WebAssembly Integration Test Suite
 *
 * Tests the WebAssembly integration and fallback mechanisms
 */

import { wasmIntegration } from '../dist/wasm/index.js';
import { pathToFileURL } from 'node:url';

const log = {
  info: (...args) => console.log('[INFO]', ...args),
  warn: (...args) => console.warn('[WARN]', ...args),
  error: (...args) => console.error('[ERROR]', ...args),
  debug: (...args) => console.debug('[DEBUG]', ...args)
};

async function testWASMIntegration() {
  log.info('Starting WebAssembly integration test...\n');

  // Test 1: Environment Check
  log.info('=== Test 1: Environment Check ===');
  const isSupported = wasmIntegration.constructor.isSupported();
  log.info(`WebAssembly supported: ${isSupported}`);

  if (!isSupported) {
    log.warn('WebAssembly not supported, testing TypeScript fallbacks only');
  }

  // Test 2: Initialization
  log.info('\n=== Test 2: Initialization ===');
  try {
    await wasmIntegration.initialize();
    log.info('✅ Initialization successful');

    if (wasmIntegration.isReady()) {
      log.info('✅ WASM module is ready');
    } else {
      log.warn('⚠️ WASM module not ready, using TypeScript fallbacks');
    }
  } catch (error) {
    log.error('❌ Initialization failed:', error.message);
    log.info('This is expected if WASM module is not built yet');
  }

  // Test 3: Property Parsing
  log.info('\n=== Test 3: Property Parsing ===');
  const testJson = JSON.stringify({
    name: 'TestAsset',
    value: 42,
    location: { x: 100, y: 200, z: 300 },
    tags: ['tag1', 'tag2'],
    properties: {
      material: { name: 'Mat1', intensity: 5000 }
    }
  });

  try {
    const start = performance.now();
    const result = await wasmIntegration.parseProperties(testJson);
    const duration = performance.now() - start;

    log.info(`✅ Property parsing completed in ${duration.toFixed(2)}ms`);
    log.info(`Result has ${Object.keys(result).length} properties`);

    if (result && typeof result === 'object') {
      log.info('Result structure is valid');
    }
  } catch (error) {
    log.error('❌ Property parsing failed:', error.message);
  }

  // Test 4: Transform Calculations
  log.info('\n=== Test 4: Transform Calculations ===');
  try {
    const location = [100, 200, 300];
    const rotation = [0, 90, 0];
    const scale = [1, 1, 1];

    const matrix = wasmIntegration.composeTransform(location, rotation, scale);

    log.info(`✅ Transform composition successful`);
    log.info(`Matrix length: ${matrix.length} (expected: 9)`);

    if (matrix.length === 9) {
      log.info('✅ Matrix has correct length');
    }

    // Test matrix decomposition
    const decomposed = wasmIntegration.decomposeMatrix(matrix);
    log.info(`✅ Matrix decomposition successful`);
    log.info(`Decomposed: [${decomposed.slice(0, 3).join(', ')}, ...]`);
  } catch (error) {
    log.error('❌ Transform calculation failed:', error.message);
  }

  // Test 5: Vector Operations
  log.info('\n=== Test 5: Vector Operations ===');
  try {
    const v1 = [1, 2, 3];
    const v2 = [4, 5, 6];
    const result = wasmIntegration.vectorAdd(v1, v2);

    log.info(`✅ Vector addition successful`);
    log.info(`${v1.join(', ')} + ${v2.join(', ')} = [${result.join(', ')}]`);

    // Verify result
    const expected = [5, 7, 9];
    const isCorrect = result.every((val, idx) => Math.abs(val - expected[idx]) < 0.001);

    if (isCorrect) {
      log.info('✅ Vector addition result is correct');
    } else {
      log.warn('⚠️ Vector addition result may be incorrect');
    }
  } catch (error) {
    log.error('❌ Vector operation failed:', error.message);
  }

  // Test 6: Dependency Resolution
  log.info('\n=== Test 6: Dependency Resolution ===');
  const testDependencies = {
    'AssetA': ['AssetB', 'AssetC'],
    'AssetB': ['AssetC'],
    'AssetC': []
  };

  try {
    const result = await wasmIntegration.resolveDependencies('AssetA', testDependencies);

    log.info(`✅ Dependency resolution successful`);
    log.info(`Found ${result.dependencies?.length || 0} dependencies`);
    log.info(`Asset path: ${result.asset}`);

    if (result.dependencies && result.dependencies.length > 0) {
      log.info('First dependency:', result.dependencies[0].path);
    }
  } catch (error) {
    log.error('❌ Dependency resolution failed:', error.message);
  }

  // Test 7: Performance Metrics
  log.info('\n=== Test 7: Performance Metrics ===');
  const metrics = wasmIntegration.getMetrics();

  log.info(`Total operations: ${metrics.totalOperations}`);
  log.info(`WASM operations: ${metrics.wasmOperations}`);
  log.info(`TypeScript operations: ${metrics.tsOperations}`);
  log.info(`Average time: ${metrics.averageTime.toFixed(2)}ms`);

  if (metrics.totalOperations > 0) {
    log.info('✅ Performance metrics collected');
  }

  // Print detailed performance report
  log.info('\n=== Performance Report ===');
  log.info(wasmIntegration.reportPerformance());

  // Test 9: Circular Dependency Detection
  log.info('\n=== Test 9: Circular Dependency Detection ===');
  const circularDeps = {
    'AssetA': ['AssetB'],
    'AssetB': ['AssetC'],
    'AssetC': ['AssetA'] // Creates a cycle
  };

  try {
    const circular = await wasmIntegration.findCircularDependencies(circularDeps);
    log.info(`✅ Circular dependency detection completed`);
    log.info(`Found ${circular.length} circular dependency chains`);
    if (circular.length > 0) {
      log.info(`Cycle: ${circular[0].join(' -> ')}`);
    }
  } catch (error) {
    log.error('❌ Circular dependency detection failed:', error.message);
  }

  // Test 10: Topological Sort
  log.info('\n=== Test 10: Topological Sort ===');
  const sortDeps = {
    'D': [],
    'C': ['D'],
    'B': ['C', 'D'],
    'A': ['B', 'C']
  };

  try {
    const sorted = await wasmIntegration.topologicalSort(sortDeps);
    log.info(`✅ Topological sort completed`);
    log.info(`Order: ${sorted.join(' -> ')}`);
    // D should come before C, C before B, B before A
    const dIdx = sorted.indexOf('D');
    const cIdx = sorted.indexOf('C');
    const bIdx = sorted.indexOf('B');
    const aIdx = sorted.indexOf('A');
    if (dIdx < cIdx && cIdx < bIdx && bIdx < aIdx) {
      log.info('✅ Sort order is correct');
    } else {
      log.warn('⚠️ Sort order may be incorrect');
    }
  } catch (error) {
    log.error('❌ Topological sort failed:', error.message);
  }

  // Test 11: Edge Cases
  log.info('\n=== Test 11: Edge Cases ===');

  // Empty JSON
  try {
    const emptyResult = await wasmIntegration.parseProperties('{}');
    log.info(`✅ Empty JSON parsing: ${Object.keys(emptyResult).length === 0 ? 'passed' : 'result has keys'}`);
  } catch (error) {
    log.error('❌ Empty JSON parsing failed:', error.message);
  }

  // Invalid JSON (should fallback gracefully)
  try {
    await wasmIntegration.parseProperties('not valid json');
    log.warn('⚠️ Invalid JSON did not throw - fallback handled');
  } catch (error) {
    log.info(`✅ Invalid JSON correctly rejected: ${error.message.substring(0, 50)}...`);
  }

  // Zero vectors
  try {
    const zeroAdd = wasmIntegration.vectorAdd([0, 0, 0], [0, 0, 0]);
    const isZero = zeroAdd.every(v => v === 0);
    log.info(`✅ Zero vector addition: ${isZero ? 'passed' : 'unexpected result'}`);
  } catch (error) {
    log.error('❌ Zero vector test failed:', error.message);
  }

  // Test 12: Performance Benchmark
  log.info('\n=== Test 12: Performance Benchmark ===');
  const iterations = 100;
  const largeJson = JSON.stringify({
    assets: Array.from({ length: 50 }, (_, i) => ({
      name: `Asset${i}`,
      path: `/Game/Assets/Asset${i}`,
      properties: { value: i * 100, enabled: i % 2 === 0 }
    }))
  });

  const benchStart = performance.now();
  for (let i = 0; i < iterations; i++) {
    await wasmIntegration.parseProperties(largeJson);
  }
  const benchDuration = performance.now() - benchStart;
  log.info(`✅ Parsed ${iterations} large JSON objects in ${benchDuration.toFixed(2)}ms`);
  log.info(`Average: ${(benchDuration / iterations).toFixed(3)}ms per parse`);

  // Vector math benchmark
  const vecStart = performance.now();
  for (let i = 0; i < 1000; i++) {
    wasmIntegration.vectorAdd([i, i + 1, i + 2], [i * 2, i * 3, i * 4]);
  }
  const vecDuration = performance.now() - vecStart;
  log.info(`✅ 1000 vector additions in ${vecDuration.toFixed(2)}ms`);

  // Final Summary
  log.info('\n=== Test Summary ===');
  log.info('All tests completed. Check the output above for details.');
  log.info('');
  log.info('Note: Some tests may use TypeScript fallbacks if WASM is not available.');
  log.info('This is expected behavior and the integration will still work correctly.');
  log.info('');
  log.info('To use WebAssembly:');
  log.info('1. Build the WASM module: cd wasm && wasm-pack build --target web');
  log.info('2. Copy the pkg/ directory to src/wasm/');
  log.info('3. Set environment variable: WASM_ENABLED=true');
  log.info('4. Run tests again');

  process.exit(0);
}

const mainScriptUrl = pathToFileURL(process.argv[1]).href;

if (import.meta.url === mainScriptUrl) {
  testWASMIntegration().catch(error => {
    log.error('Test failed with error:', error);
    process.exit(1);
  });
}
