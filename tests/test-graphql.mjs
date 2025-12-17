#!/usr/bin/env node
/**
 * GraphQL API Test Suite
 *
 * Tests the GraphQL server functionality
 */

import { createServer } from 'http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { GraphQLServer } from '../dist/graphql/server.js';
import { UnrealBridge } from '../dist/unreal-bridge.js';
import { AutomationBridge } from '../dist/automation/index.js';

// Test automation bridge for verifying GraphQL integration
class TestAutomationBridge {
  constructor() {
    this.handlers = new Map();
    this.connected = false;
  }

  on(event, handler) {
    this.handlers.set(event, handler);
  }

  off(event, handler) {
    const existing = this.handlers.get(event);
    if (!existing) return;
    if (!handler || existing === handler) {
      this.handlers.delete(event);
    }
  }

  isConnected() {
    return this.connected;
  }

  start() {
    // Simulate connection
    setTimeout(() => {
      this.connected = true;
      const handler = this.handlers.get('connected');
      if (handler) {
        handler({ metadata: {}, port: 8090, protocol: 'mcp-automation' });
      }
    }, 100);
  }

  stop() {
    // Cleanup
  }

  sendAutomationRequest(action, params, options) {
    // Return test data based on action
    switch (action) {
      case 'list_assets':
        return Promise.resolve({
          success: true,
          result: {
            assets: [
              { name: 'Material1', path: '/Game/Materials/Mat1', class: 'Material' },
              { name: 'Material2', path: '/Game/Materials/Mat2', class: 'Material' }
            ],
            totalCount: 2
          }
        });

      case 'list_actors':
        return Promise.resolve({
          success: true,
          result: {
            actors: [
              { name: 'Cube', class: 'StaticMeshActor', location: { x: 0, y: 0, z: 100 } },
              { name: 'Sphere', class: 'StaticMeshActor', location: { x: 100, y: 0, z: 100 } }
            ]
          }
        });

      case 'get_blueprint':
        return Promise.resolve({
          success: true,
          result: {
            name: 'BP_Test',
            path: '/Game/Blueprints/BP_Test',
            parentClass: 'Actor',
            variables: [{ name: 'Health', type: 'Float', defaultValue: 100 }],
            functions: [{ name: 'TakeDamage', inputs: [{ name: 'Damage', type: 'Float' }] }],
            events: [{ name: 'BeginPlay', type: 'Event' }],
            components: []
          }
        });

      case 'list_blueprints':
        return Promise.resolve({
          success: true,
          result: {
            blueprints: [
              { name: 'BP_Player', path: '/Game/Blueprints/BP_Player', parentClass: 'Character' },
              { name: 'BP_Enemy', path: '/Game/Blueprints/BP_Enemy', parentClass: 'Character' }
            ],
            totalCount: 2
          }
        });

      case 'search':
        return Promise.resolve({
          success: true,
          result: {
            results: [
              { name: 'Material1', path: '/Game/Materials/Mat1', class: 'Material' },
              { name: 'Cube', class: 'StaticMeshActor' }
            ]
          }
        });

      default:
        return Promise.resolve({ success: true, result: {} });
    }
  }
}

const log = {
  info: (...args) => console.log('[INFO]', ...args),
  warn: (...args) => console.warn('[WARN]', ...args),
  error: (...args) => console.error('[ERROR]', ...args),
  debug: (...args) => console.debug('[DEBUG]', ...args)
};

async function testGraphQLServer() {
  log.info('Starting GraphQL server test...');

  const bridge = new UnrealBridge();
  const testAutomationBridge = new TestAutomationBridge();
  bridge.setAutomationBridge(testAutomationBridge);

  const graphQLServer = new GraphQLServer(bridge, testAutomationBridge, {
    enabled: true,
    port: 4000,
    host: '127.0.0.1'
  });

  try {
    await graphQLServer.start();
    log.info('✅ GraphQL server started successfully');

    // Give the server a moment to start
    await new Promise(resolve => setTimeout(resolve, 500));

    log.info('GraphQL server is running at http://127.0.0.1:4000/graphql');
    log.info('You can test it with: curl -X POST -H "Content-Type: application/json" -d \'{"query": "{ assets { edges { node { name path } } } }"\' http://127.0.0.1:4000/graphql');

    // Run tests
    const results = await runGraphQLTests();

    // Auto-exit after tests complete
    log.info('\nTests completed. Shutting down...');
    await graphQLServer.stop();

    if (results.failed > 0) {
      log.info(`\n❌ ${results.failed}/${results.total} tests failed`);
      process.exit(1);
    } else {
      log.info(`\n✅ All ${results.passed}/${results.total} tests passed`);
      process.exit(0);
    }
  } catch (error) {
    log.error('Failed to start GraphQL server:', error);
    process.exit(1);
  }
}

async function runGraphQLTests() {
  log.info('\n=== Running GraphQL Tests ===\n');

  const tests = [
    {
      name: 'List Assets',
      query: `
        {
          assets {
            edges {
              node {
                name
                path
                class
              }
            }
            totalCount
          }
        }
      `
    },
    {
      name: 'List Actors',
      query: `
        {
          actors {
            edges {
              node {
                name
                class
                location {
                  x
                  y
                  z
                }
              }
            }
          }
        }
      `
    },
    {
      name: 'Get Blueprint',
      query: `
        {
          blueprint(path: "/Game/Blueprints/BP_Test") {
            name
            path
            parentClass
            variables {
              name
              type
              defaultValue
            }
            functions {
              name
              inputs {
                name
                type
              }
            }
            events {
              name
              type
            }
          }
        }
      `
    },
    {
      name: 'List Blueprints',
      query: `
        {
          blueprints {
            edges {
              node {
                name
                path
                parentClass
              }
            }
            totalCount
          }
        }
      `
    },
    {
      name: 'Search',
      query: `
        {
          search(query: "Material", type: ASSETS) {
            ... on Asset {
              name
              path
              class
            }
          }
        }
      `
    },
    {
      name: 'Error Handling - Invalid Query',
      query: `
        {
          invalidField {
            name
          }
        }
      `,
      expectError: true
    },
    {
      name: 'Pagination Test',
      query: `
        {
          assets(pagination: { limit: 1, offset: 0 }) {
            edges {
              node {
                name
              }
              cursor
            }
            pageInfo {
              hasNextPage
              hasPreviousPage
            }
            totalCount
          }
        }
      `
    }
  ];

  let passed = 0;
  let failed = 0;

  for (const test of tests) {
    try {
      log.info(`Testing: ${test.name}`);

      const response = await fetch('http://127.0.0.1:4000/graphql', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ query: test.query })
      });

      const result = await response.json();

      if (result.errors) {
        if (test.expectError) {
          log.info(`  ✅ ${test.name} - Expected error received`);
          passed++;
        } else {
          log.error(`  ❌ ${test.name} - GraphQL Errors:`, JSON.stringify(result.errors));
          failed++;
        }
      } else if (result.data) {
        if (test.expectError) {
          log.error(`  ❌ ${test.name} - Expected error but got data`);
          failed++;
        } else {
          log.info(`  ✅ ${test.name} - Success`);
          passed++;
        }
      } else {
        log.error(`  ❌ ${test.name} - No data returned`);
        failed++;
      }
    } catch (error) {
      log.error(`  ❌ ${test.name} - Error:`, error.message);
      failed++;
    }
  }

  log.info('\n=== Test Results ===');
  log.info(`Passed: ${passed}/${tests.length}`);
  log.info(`Failed: ${failed}/${tests.length}`);

  return { passed, failed, total: tests.length };
}

// Handle graceful shutdown
process.on('SIGINT', () => {
  log.info('\nShutting down...');
  process.exit(0);
});

const thisFilePath = fileURLToPath(import.meta.url);
const isMainModule = (() => {
  const argvPath = process.argv[1];
  if (!argvPath) return false;
  const resolvedArgvPath = path.resolve(argvPath);
  return thisFilePath === resolvedArgvPath;
})();

if (isMainModule) {
  testGraphQLServer().catch(error => {
    log.error('Test failed:', error);
    process.exit(1);
  });
}
