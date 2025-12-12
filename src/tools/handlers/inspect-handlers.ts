import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

async function resolveObjectPathFromArgs(args: any, tools: ITools): Promise<string | undefined> {
  // Priority 1: Direct objectPath
  const direct = typeof args.objectPath === 'string' && args.objectPath.trim().length > 0
    ? args.objectPath.trim()
    : undefined;
  if (direct) {
    return direct;
  }

  // Priority 2: actorName - pass directly, C++ plugin will resolve
  if (typeof args.actorName === 'string' && args.actorName.trim().length > 0) {
    return args.actorName.trim();
  }

  // Priority 3: Try to resolve from 'name' field using actor tools
  const name = typeof args.name === 'string' ? args.name.trim() : '';
  if (!name || !tools.actorTools || typeof (tools.actorTools as any).findByName !== 'function') {
    return undefined;
  }

  try {
    const res: any = await (tools.actorTools as any).findByName(name);
    const container: any = res && (res.result || res);
    const actors = container && Array.isArray(container.actors) ? container.actors : [];
    if (actors.length > 0) {
      const first = actors[0];
      if (first && typeof first.path === 'string' && first.path.trim().length > 0) {
        return first.path.trim();
      }
    }
  } catch {
  }

  // Fallback: return name directly for C++ plugin to resolve
  return name.length > 0 ? name : undefined;
}


function getActorNameFromArgs(args: any): string | undefined {
  if (typeof args.actorName === 'string' && args.actorName.trim().length > 0) {
    return args.actorName.trim();
  }
  if (typeof args.name === 'string' && args.name.trim().length > 0) {
    return args.name.trim();
  }
  return undefined;
}

function looksLikeUnrealObjectPath(value: string): boolean {
  const v = value.trim();
  if (!v) return false;
  // Heuristics: object paths commonly contain ':' (subobject), '/Game', '/Script', or 'PersistentLevel'.
  return v.includes(':') || v.startsWith('/Game') || v.startsWith('/Script') || v.includes('PersistentLevel');
}

async function resolveComponentObjectPathFromArgs(args: any, tools: ITools): Promise<string> {
  const componentName = typeof args.componentName === 'string' ? args.componentName.trim() : '';
  const componentPath = typeof args.componentPath === 'string' ? args.componentPath.trim() : '';

  const direct = componentPath || (looksLikeUnrealObjectPath(componentName) ? componentName : '');
  if (direct) return direct;

  const actorName = getActorNameFromArgs(args);
  if (!actorName) {
    throw new Error('Invalid actorName: required to resolve componentName');
  }
  if (!componentName) {
    throw new Error('Invalid componentName: must be a non-empty string');
  }

  const compsRes: any = await tools.actorTools.getComponents(actorName);
  const components: any[] = Array.isArray(compsRes?.components) ? compsRes.components : [];
  const needle = componentName.toLowerCase();

  const match = components.find((c) => String(c?.name || '').toLowerCase() === needle)
    ?? components.find((c) => String(c?.path || '').toLowerCase() === needle)
    ?? components.find((c) => String(c?.path || '').toLowerCase().endsWith(`:${needle}`));

  const path = typeof match?.path === 'string' ? match.path.trim() : '';
  if (!path) {
    throw new Error(`Component not found on actor '${actorName}': ${componentName}`);
  }

  return path;
}

export async function handleInspectTools(action: string, args: any, tools: ITools) {
  switch (action) {
    case 'inspect_object': {
      const objectPath = await resolveObjectPathFromArgs(args, tools);
      if (!objectPath) {
        throw new Error('Invalid objectPath: must be a non-empty string');
      }

      const payload = {
        ...args,
        objectPath,
        action: 'inspect_object',
        detailed: true
      };

      const res: any = await executeAutomationRequest(
        tools,
        'inspect',
        payload,
        'Automation bridge not available for inspect operations'
      );

      if (res && res.success === false) {
        const errorCode = String(res.error || '').toUpperCase();
        const msg = String(res.message || '');
        if (errorCode === 'OBJECT_NOT_FOUND' || msg.toLowerCase().includes('object not found')) {
          return cleanObject({
            success: false,
            handled: true,
            notFound: true,
            error: res.error,
            message: res.message || 'Object not found'
          });
        }
      }

      return cleanObject(res);
    }
    case 'get_property': {
      const objectPath = await resolveObjectPathFromArgs(args, tools);
      const propertyName =
        (typeof args.propertyName === 'string' && args.propertyName.trim().length > 0)
          ? args.propertyName.trim()
          : (typeof args.propertyPath === 'string' ? args.propertyPath.trim() : '');

      if (!objectPath) {
        throw new Error('Invalid objectPath: must be a non-empty string');
      }
      if (!propertyName) {
        throw new Error('Invalid propertyName: must be a non-empty string');
      }

      const res = await tools.introspectionTools.getProperty({
        objectPath,
        propertyName
      });
      return cleanObject(res);
    }
    case 'set_property': {
      const objectPath = await resolveObjectPathFromArgs(args, tools);
      const propertyName =
        (typeof args.propertyName === 'string' && args.propertyName.trim().length > 0)
          ? args.propertyName.trim()
          : (typeof args.propertyPath === 'string' ? args.propertyPath.trim() : '');

      if (!objectPath) {
        throw new Error('Invalid objectPath: must be a non-empty string');
      }
      if (!propertyName) {
        throw new Error('Invalid propertyName: must be a non-empty string');
      }

      const res: any = await tools.introspectionTools.setProperty({
        objectPath,
        propertyName,
        value: args.value
      });

      if (res && res.success === false) {
        const errorCode = String(res.error || '').toUpperCase();
        if (errorCode === 'PROPERTY_NOT_FOUND') {
          return cleanObject({
            ...res,
            error: 'UNKNOWN_PROPERTY'
          });
        }
      }

      return cleanObject(res);
    }

    case 'get_components': {
      const actorName = getActorNameFromArgs(args);
      if (!actorName) {
        throw new Error('Invalid actorName');
      }
      return cleanObject(await tools.actorTools.getComponents(actorName));
    }
    case 'get_component_property': {
      const componentObjectPath = await resolveComponentObjectPathFromArgs(args, tools);
      const propertyPath =
        (typeof args.propertyName === 'string' && args.propertyName.trim().length > 0)
          ? args.propertyName.trim()
          : (typeof args.propertyPath === 'string' ? args.propertyPath.trim() : '');
      if (!propertyPath) {
        throw new Error('Invalid propertyName: must be a non-empty string');
      }

      const res = await tools.introspectionTools.getProperty({
        objectPath: componentObjectPath,
        propertyName: propertyPath
      });
      return cleanObject(res);
    }
    case 'set_component_property': {
      const componentObjectPath = await resolveComponentObjectPathFromArgs(args, tools);
      const propertyPath =
        (typeof args.propertyName === 'string' && args.propertyName.trim().length > 0)
          ? args.propertyName.trim()
          : (typeof args.propertyPath === 'string' ? args.propertyPath.trim() : '');
      if (!propertyPath) {
        throw new Error('Invalid propertyName: must be a non-empty string');
      }

      const res = await tools.introspectionTools.setProperty({
        objectPath: componentObjectPath,
        propertyName: propertyPath,
        value: args.value
      });
      return cleanObject(res);
    }
    case 'get_metadata': {
      const actorName = getActorNameFromArgs(args) ?? args.objectPath;
      return cleanObject(await tools.actorTools.getMetadata(actorName));
    }
    case 'add_tag': {
      const actorName = getActorNameFromArgs(args) ?? args.objectPath;
      const rawTag = typeof args.tag === 'string' ? args.tag.trim() : '';
      if (!rawTag) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'tag is required',
          actorName,
          tag: rawTag
        });
      }
      return cleanObject(await tools.actorTools.addTag({
        actorName,
        tag: rawTag
      }));
    }
    case 'find_by_tag':
      return cleanObject(await tools.actorTools.findByTag({
        tag: args.tag
      }));
    case 'create_snapshot': {
      const actorName = getActorNameFromArgs(args) ?? args.objectPath;
      return cleanObject(await tools.actorTools.createSnapshot({
        actorName,
        snapshotName: args.snapshotName
      }));
    }
    case 'restore_snapshot': {
      const actorName = getActorNameFromArgs(args) ?? args.objectPath;
      return cleanObject(await tools.actorTools.restoreSnapshot({
        actorName,
        snapshotName: args.snapshotName
      }));
    }
    case 'export': {
      const actorName = getActorNameFromArgs(args) ?? args.objectPath;
      const destinationPath = args.destinationPath || args.outputPath;
      return cleanObject(await tools.actorTools.exportActor({
        actorName,
        destinationPath
      }));
    }
    case 'delete_object': {
      const actorName = getActorNameFromArgs(args) ?? args.objectPath;
      try {
        const res = await tools.actorTools.delete({
          actorName
        });
        return cleanObject(res);
      } catch (err: any) {
        const msg = String(err?.message || err || '');
        const lower = msg.toLowerCase();
        if (lower.includes('actor not found')) {
          return cleanObject({
            success: false,
            error: 'NOT_FOUND',
            handled: true,
            message: msg,
            deleted: actorName,
            notFound: true
          });
        }
        throw err;
      }
    }
    case 'list_objects':
      return cleanObject(await tools.actorTools.listActors());
    case 'find_by_class': {
      const className =
        (typeof args.className === 'string' && args.className.trim().length > 0)
          ? args.className.trim()
          : (typeof args.classPath === 'string' ? args.classPath.trim() : '');
      const res: any = await tools.introspectionTools.findObjectsByClass(className);
      if (!res || res.success === false) {
        // Return proper failure state
        return cleanObject({
          success: false,
          error: res?.error || 'OPERATION_FAILED',
          message: res?.message || 'find_by_class failed',
          className,
          objects: [],
          count: 0
        });
      }
      return cleanObject(res);
    }
    case 'get_bounding_box': {
      const actorName = getActorNameFromArgs(args) ?? args.objectPath;
      try {
        const res = await tools.actorTools.getBoundingBox(actorName);
        return cleanObject(res);
      } catch (err: any) {
        const msg = String(err?.message || err || '');
        const lower = msg.toLowerCase();
        if (lower.includes('actor not found')) {
          return cleanObject({
            success: false,
            error: 'NOT_FOUND',
            handled: true,
            message: msg,
            actorName,
            notFound: true
          });
        }
        throw err;
      }
    }
    case 'inspect_class': {
      const className =
        (typeof args.className === 'string' && args.className.trim().length > 0)
          ? args.className.trim()
          : (typeof args.classPath === 'string' ? args.classPath.trim() : '');
      const res: any = await tools.introspectionTools.getCDO(className);
      if (!res || res.success === false) {
        // Return proper failure state
        return cleanObject({
          success: false,
          error: res?.error || 'OPERATION_FAILED',
          message: res?.message || 'inspect_class failed',
          className,
          cdo: res?.cdo ?? null
        });
      }
      return cleanObject(res);
    }
    default:
      // Fallback to generic automation request if action not explicitly handled
      const res = await executeAutomationRequest(tools, 'inspect', args, 'Automation bridge not available for inspect operations');
      return cleanObject(res);
  }
}
