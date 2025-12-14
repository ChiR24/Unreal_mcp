import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleGraphTools(toolName: string, action: string, args: any, tools: ITools) {
    // Common validation
    if (!args.assetPath && !args.blueprintPath && !args.systemPath) {
        // Some actions might not need a path if they operate on "currently open" asset, 
        // but generally we want an asset path.
    }

    // Dispatch based on tool name
    switch (toolName) {
        case 'manage_blueprint_graph':
            return handleBlueprintGraph(action, args, tools);
        case 'manage_niagara_graph':
            return handleNiagaraGraph(action, args, tools);
        case 'manage_material_graph':
            return handleMaterialGraph(action, args, tools);
        case 'manage_behavior_tree':
            return handleBehaviorTree(action, args, tools);
        default:
            return { success: false, error: 'UNKNOWN_TOOL', message: `Unknown graph tool: ${toolName}` };
    }
}

async function handleBlueprintGraph(action: string, args: any, tools: ITools) {
    const processedArgs = { ...args, subAction: action };

    // Default graphName
    if (!processedArgs.graphName) {
        processedArgs.graphName = 'EventGraph';
    }

    // Support Node.Pin format for connect_pins
    if (action === 'connect_pins') {
        if (processedArgs.fromNodeId && processedArgs.fromNodeId.includes('.') && !processedArgs.fromPinName) {
            const parts = processedArgs.fromNodeId.split('.');
            processedArgs.fromNodeId = parts[0];
            processedArgs.fromPinName = parts.slice(1).join('.');
        }
        if (processedArgs.toNodeId && processedArgs.toNodeId.includes('.') && !processedArgs.toPinName) {
            const parts = processedArgs.toNodeId.split('.');
            processedArgs.toNodeId = parts[0];
            processedArgs.toPinName = parts.slice(1).join('.');
        }
    }

    const res: any = await executeAutomationRequest(tools, 'manage_blueprint_graph', processedArgs, 'Automation bridge not available');
    return cleanObject({ ...res, ...(res.result || {}) });
}

async function handleNiagaraGraph(action: string, args: any, tools: ITools) {
    const payload = { ...args, subAction: action };
    // Map systemPath to assetPath if missing
    if (payload.systemPath && !payload.assetPath) {
        payload.assetPath = payload.systemPath;
    }
    const res: any = await executeAutomationRequest(tools, 'manage_niagara_graph', payload, 'Automation bridge not available');
    return cleanObject({ ...res, ...(res.result || {}) });
}

async function handleMaterialGraph(action: string, args: any, tools: ITools) {
    const res: any = await executeAutomationRequest(tools, 'manage_material_graph', { ...args, subAction: action }, 'Automation bridge not available');
    return cleanObject({ ...res, ...(res.result || {}) });
}

async function handleBehaviorTree(action: string, args: any, tools: ITools) {
    const res: any = await executeAutomationRequest(tools, 'manage_behavior_tree', { ...args, subAction: action }, 'Automation bridge not available');
    return cleanObject({ ...res, ...(res.result || {}) });
}
