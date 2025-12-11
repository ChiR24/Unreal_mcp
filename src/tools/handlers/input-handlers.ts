import { ITools } from '../../types/tool-interfaces.js';
import { cleanObject } from '../../utils/safe-json.js';
import { ResponseFactory } from '../../utils/response-factory.js';
import { InputTools } from '../input.js';

export async function handleInputTools(
    action: string,
    args: any,
    tools: ITools
) {
    const inputTools = tools.inputTools as InputTools;
    if (!inputTools) {
        return ResponseFactory.error('Input tools not available');
    }

    switch (action) {
        case 'create_input_action':
            return cleanObject(await inputTools.createInputAction(args.name, args.path));
        case 'create_input_mapping_context':
            return cleanObject(await inputTools.createInputMappingContext(args.name, args.path));
        case 'add_mapping':
            return cleanObject(await inputTools.addMapping(args.contextPath, args.actionPath, args.key));
        case 'remove_mapping':
            return cleanObject(await inputTools.removeMapping(args.contextPath, args.actionPath));
        default:
            return ResponseFactory.error(`Unknown input action: ${action}`);
    }
}
