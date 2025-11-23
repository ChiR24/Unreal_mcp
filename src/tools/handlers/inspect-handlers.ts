import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleInspectTools(action: string, args: any, tools: ITools) {
  switch (action) {
    case 'inspect_object':
      return cleanObject(await tools.introspectionTools.inspectObject({
        objectPath: args.objectPath,
        detailed: true // Default to detailed for inspect_object
      }));
    case 'get_property':
      return cleanObject(await tools.introspectionTools.getProperty({
        objectPath: args.objectPath || args.actorName,
        propertyName: args.propertyName
      }));
    case 'set_property':
      return cleanObject(await tools.introspectionTools.setProperty({
        objectPath: args.objectPath || args.actorName,
        propertyName: args.propertyName,
        value: args.value
      }));

    case 'get_components':
      return cleanObject(await tools.actorTools.getComponents(args.actorName || args.objectPath));
    case 'get_component_property':
      return cleanObject(await tools.introspectionTools.getComponentProperty({
        objectPath: args.objectPath || args.actorName,
        componentName: args.componentName,
        propertyName: args.propertyName
      }));
    case 'set_component_property':
      return cleanObject(await tools.introspectionTools.setComponentProperty({
        objectPath: args.objectPath || args.actorName,
        componentName: args.componentName,
        propertyName: args.propertyName,
        value: args.value
      }));
    case 'get_metadata':
      return cleanObject(await tools.actorTools.getMetadata(args.actorName || args.objectPath));
    case 'add_tag':
      return cleanObject(await tools.actorTools.addTag({
        actorName: args.actorName || args.objectPath,
        tag: args.tag
      }));
    case 'find_by_tag':
      return cleanObject(await tools.actorTools.findByTag({
        tag: args.tag
      }));
    case 'create_snapshot':
      return cleanObject(await tools.actorTools.createSnapshot({
        actorName: args.actorName || args.objectPath,
        snapshotName: args.snapshotName
      }));
    case 'restore_snapshot':
      return cleanObject(await tools.actorTools.restoreSnapshot({
        actorName: args.actorName || args.objectPath,
        snapshotName: args.snapshotName
      }));
    case 'export':
      return cleanObject(await tools.actorTools.exportActor({
        actorName: args.actorName || args.objectPath,
        destinationPath: args.destinationPath
      }));
    case 'delete_object':
      return cleanObject(await tools.actorTools.delete({
        actorName: args.actorName || args.objectPath
      }));
    case 'list_objects':
      return cleanObject(await tools.actorTools.listActors());
    case 'find_by_class':
      return cleanObject(await tools.introspectionTools.findObjectsByClass(args.className));
    case 'get_bounding_box':
      return cleanObject(await tools.actorTools.getBoundingBox(args.actorName || args.objectPath));
    case 'inspect_class':
      return cleanObject(await tools.introspectionTools.getCDO(args.className));
    default:
      // Fallback to generic automation request if action not explicitly handled
      const res = await executeAutomationRequest(tools, 'inspect', args, 'Automation bridge not available for inspect operations');
      return cleanObject(res);
  }
}
