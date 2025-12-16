import { UnrealAutomationClient } from '../src/unreal-client.js';
import { runUnrealTests, assert } from './run-unreal-tool-tests.mjs';

runUnrealTests('manage_blueprint_graph_events', [
    {
        name: 'Create Event Node',
        action: async (client) => {
            const blueprintPath = '/Game/Tests/Integration/BP_EventTest';

            // 1. Create a blueprint to test with
            await client.sendRequest('blueprint_create', {
                path: blueprintPath,
                parentClass: 'Actor',
                type: 'Normal'
            });

            // 2. Add BeginPlay event node
            const result = await client.sendRequest('manage_blueprint_graph', {
                blueprintPath: blueprintPath,
                graphName: 'EventGraph',
                subAction: 'create_node',
                nodeType: 'Event',
                eventName: 'ReceiveBeginPlay',
                x: 200,
                y: 200
            });

            assert(result.success, 'Failed to create Event node');
            assert(result.nodeId, 'Missing nodeId');

            // 3. Cleanup
            await client.sendRequest('delete_asset', { path: blueprintPath });
        }
    }
]);
