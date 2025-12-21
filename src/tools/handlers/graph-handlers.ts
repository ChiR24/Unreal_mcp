import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { GraphArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleGraphTools(toolName: string, action: string, args: GraphArgs, tools: ITools) {
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
            throw new Error(`Unknown graph tool: ${toolName}`);
    }
}

async function handleBlueprintGraph(action: string, args: GraphArgs, tools: ITools) {
    const processedArgs = { ...args, subAction: action };

    // Default graphName
    if (!processedArgs.graphName) {
        processedArgs.graphName = 'EventGraph';
    }

    // Fix Issue 1: Map FunctionCall to CallFunction
    if (processedArgs.nodeType === 'FunctionCall') {
        processedArgs.nodeType = 'CallFunction';
    }

    // Fix Issue 2 & 3: Map memberName to specific names based on nodeType
    if (processedArgs.memberName) {
        if (processedArgs.nodeType === 'VariableGet' || processedArgs.nodeType === 'VariableSet') {
            if (!processedArgs.variableName) processedArgs.variableName = processedArgs.memberName;
        } else if (processedArgs.nodeType === 'Event' || processedArgs.nodeType === 'CustomEvent' || (processedArgs.nodeType && processedArgs.nodeType.startsWith('K2Node_Event'))) {
            if (!processedArgs.eventName) processedArgs.eventName = processedArgs.memberName;
            // CustomEvent uses eventName (mapped to CustomFunctionName) or customEventName in some contexts, 
            // but C++ CustomEvent handler uses 'eventName' payload field.
        } else if (processedArgs.nodeType === 'CallFunction' || processedArgs.nodeType === 'K2Node_CallFunction') {
            // C++ uses 'memberName' for CallFunction, so this is fine.
        }
    }

    // Fix Issue 5: Map memberClass/componentClass to targetClass for Cast nodes
    if ((processedArgs.memberClass || processedArgs.componentClass) &&
        (processedArgs.nodeType === 'Cast' || (processedArgs.nodeType && processedArgs.nodeType.startsWith('CastTo')))) {
        if (!processedArgs.targetClass) processedArgs.targetClass = processedArgs.memberClass || processedArgs.componentClass;
    }

    // Fix Issue 6: Support connect_pins parameter mapping
    // Input: nodeId, pinName, linkedTo (TargetNode.Pin)
    if (action === 'connect_pins') {
        // Map source
        if (!processedArgs.fromNodeId && processedArgs.nodeId) {
            processedArgs.fromNodeId = processedArgs.nodeId;
        }
        if (!processedArgs.fromPinName && processedArgs.pinName) {
            processedArgs.fromPinName = processedArgs.pinName;
        }

        // Map target from linkedTo
        if (!processedArgs.toNodeId && processedArgs.linkedTo) {
            if (processedArgs.linkedTo.includes('.')) {
                const parts = processedArgs.linkedTo.split('.');
                processedArgs.toNodeId = parts[0];
                processedArgs.toPinName = parts.slice(1).join('.');
            }
        }
    }

    // Support Node.Pin format for connect_pins (existing logic preserved/enhanced)
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

async function handleNiagaraGraph(action: string, args: GraphArgs, tools: ITools) {
    const payload = { ...args, subAction: action };
    // Map systemPath to assetPath if missing
    if (payload.systemPath && !payload.assetPath) {
        payload.assetPath = payload.systemPath;
    }
    const res: any = await executeAutomationRequest(tools, 'manage_niagara_graph', payload, 'Automation bridge not available');
    return cleanObject({ ...res, ...(res.result || {}) });
}

async function handleMaterialGraph(action: string, args: GraphArgs, tools: ITools) {
    const res: any = await executeAutomationRequest(tools, 'manage_material_graph', { ...args, subAction: action }, 'Automation bridge not available');
    return cleanObject({ ...res, ...(res.result || {}) });
}

async function handleBehaviorTree(action: string, args: GraphArgs, tools: ITools) {
    const res: any = await executeAutomationRequest(tools, 'manage_behavior_tree', { ...args, subAction: action }, 'Automation bridge not available');
    return cleanObject({ ...res, ...(res.result || {}) });
}
