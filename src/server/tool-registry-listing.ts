import type { ToolDefinition } from '../tools/catalog/consolidated-tool-definitions.js';
import { unrealGatewayToolDefinition } from '../tools/catalog/unreal-gateway-definition.js';

// The single public MCP tool. `unreal` is the only addressable entry point; the
// canonical 23 parent tools stay internal. configure may change internal
// enable/disable state but must never alter this advertised list.
export function buildGatewayToolDefinition(): ToolDefinition {
    return {
        name: unrealGatewayToolDefinition.name,
        description: unrealGatewayToolDefinition.description,
        category: 'core',
        inputSchema: unrealGatewayToolDefinition.inputSchema,
        outputSchema: unrealGatewayToolDefinition.outputSchema
    };
}
