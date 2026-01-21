import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import type { HandlerArgs, InputArgs, HandlerResult } from '../../types/handler-types.js';
import { InputTools } from '../input.js';

// Helper to split assetPath into name and path components
function splitAssetPath(assetPath: string): { name: string; path: string } {
    const lastSlash = assetPath.lastIndexOf('/');
    if (lastSlash >= 0) {
        return {
            path: assetPath.substring(0, lastSlash),
            name: assetPath.substring(lastSlash + 1)
        };
    }
    return { name: assetPath, path: '/Game/Input' };
}

export async function handleInputTools(
    action: string,
    args: HandlerArgs,
    tools: ITools
): Promise<HandlerResult> {
    const argsTyped = args as InputArgs & { actionPath?: string; contextPath?: string; assetPath?: string };
    const inputTools = tools.inputTools as InputTools;
    if (!inputTools) {
        return ResponseFactory.error('Input tools not available');
    }

    switch (action) {
        case 'create_input_action': {
            // Accept name+path OR actionPath OR assetPath
            let name = argsTyped.name || '';
            let path = argsTyped.path || '';
            if ((!name || !path) && (argsTyped.actionPath || argsTyped.assetPath)) {
                const split = splitAssetPath(argsTyped.actionPath || argsTyped.assetPath || '');
                name = name || split.name;
                path = path || split.path;
            }
            return cleanObject(await inputTools.createInputAction(name, path)) as HandlerResult;
        }
        case 'create_input_mapping_context': {
            // Accept name+path OR contextPath OR assetPath
            let name = argsTyped.name || '';
            let path = argsTyped.path || '';
            if ((!name || !path) && (argsTyped.contextPath || argsTyped.assetPath)) {
                const split = splitAssetPath(argsTyped.contextPath || argsTyped.assetPath || '');
                name = name || split.name;
                path = path || split.path;
            }
            return cleanObject(await inputTools.createInputMappingContext(name, path)) as HandlerResult;
        }
        case 'add_mapping':
            return cleanObject(await inputTools.addMapping(argsTyped.contextPath ?? '', argsTyped.actionPath ?? '', argsTyped.key ?? '')) as HandlerResult;
        case 'remove_mapping':
            return cleanObject(await inputTools.removeMapping(argsTyped.contextPath ?? '', argsTyped.actionPath ?? '')) as HandlerResult;
        default:
            return ResponseFactory.error(`Unknown input action: ${action}`);
    }
}
