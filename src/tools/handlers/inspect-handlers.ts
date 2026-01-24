import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, InspectArgs, ComponentInfo, HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import { normalizeArgs, resolveObjectPath, extractString, extractOptionalString } from './argument-helper.js';

/** Response from introspection operations */
interface InspectResponse {
  success?: boolean;
  error?: string;
  message?: string;
  components?: ComponentInfo[];
  value?: unknown;
  objects?: unknown[];
  cdo?: unknown;
  [key: string]: unknown;
}

async function resolveComponentObjectPathFromArgs(args: HandlerArgs, tools: ITools): Promise<string> {
  const argsTyped = args as InspectArgs;
  const componentName = typeof argsTyped.componentName === 'string' ? argsTyped.componentName.trim() : '';
  const componentPath = typeof (argsTyped as Record<string, unknown>).componentPath === 'string' 
    ? ((argsTyped as Record<string, unknown>).componentPath as string).trim() 
    : '';

  // Direct path provided
  const direct = componentPath || (
    (componentName.includes(':') || componentName.includes('.')) &&
      (componentName.startsWith('/Game') || componentName.startsWith('/Script') || componentName.startsWith('/Engine'))
      ? componentName
      : ''
  );
  if (direct) return direct;

  const actorName = await resolveObjectPath(args, tools, { pathKeys: [], actorKeys: ['actorName', 'name', 'objectPath'] });
  if (!actorName) {
    return '';
  }
  if (!componentName) {
    return '';
  }

  // Use inspect:get_components to find the exact component path
  const compsRes = await executeAutomationRequest(
    tools,
    'inspect',
    {
      action: 'get_components',
      actorName: actorName,
      objectPath: actorName
    },
    'Failed to get components'
  ) as InspectResponse;

  let components: ComponentInfo[] = [];
  if (compsRes.success) {
    components = Array.isArray(compsRes?.components) ? compsRes.components : [];
  }

  const needle = componentName.toLowerCase();

  if (components.length > 0) {
    // 1. Exact Name/Path Match
    let match = components.find((c) => String(c?.name || '').toLowerCase() === needle)
      ?? components.find((c) => String(c?.objectPath || '').toLowerCase() === needle)
      ?? components.find((c) => String(c?.objectPath || '').toLowerCase().endsWith(`:${needle}`))
      ?? components.find((c) => String(c?.objectPath || '').toLowerCase().endsWith(`.${needle}`));

    // 2. Fuzzy/StartsWith Match (e.g. "StaticMeshComponent" -> "StaticMeshComponent0")
    if (!match) {
      match = components.find((c) => String(c?.name || '').toLowerCase().startsWith(needle));
    }

    // RESOLUTION LOGIC FIX:
    // If we have a match, we MUST use its path OR its name.
    // We cannot fall back to 'needle' or 'args.componentName' if we found a better specific match.
    if (match) {
      if (typeof match.objectPath === 'string' && match.objectPath.trim().length > 0) {
        return match.objectPath.trim();
      }
      if (typeof match.name === 'string' && match.name.trim().length > 0) {
        // Construct path from the MATCHED name, not the requested name
        return `${actorName}.${match.name}`;
      }
    }
  }

  // Fallback: Construct path manually using original request
  // Use dot notation for subobjects
  return `${actorName}.${componentName}`;
}


export async function handleInspectTools(action: string, args: HandlerArgs, tools: ITools): Promise<HandlerResult> {
  const argsTyped = args as InspectArgs;
  
  switch (action) {
    case 'inspect_object': {
      const objectPath = await resolveObjectPath(args, tools);
      if (!objectPath) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'Invalid objectPath: must be a non-empty string'
        });
      }

      const payload = {
        ...args,
        objectPath,
        action: 'inspect_object',
        detailed: true
      };

      const res = await executeAutomationRequest(
        tools,
        'inspect_object',
        payload,
        'Automation bridge not available for inspect operations'
      ) as InspectResponse;

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
      const objectPath = await resolveObjectPath(args, tools);
      const params = normalizeArgs(args, [{ key: 'propertyName', aliases: ['propertyPath'], required: true }]);
      const propertyName = extractString(params, 'propertyName');

      if (!objectPath) {
        throw new Error('Invalid objectPath: must be a non-empty string');
      }

      const res = await tools.introspectionTools.getProperty({
        objectPath,
        propertyName
      }) as InspectResponse;

      // Smart Lookup: If property not found on the Actor, try to find it on components
      if (!res.success && (res.error === 'PROPERTY_NOT_FOUND' || String(res.error).includes('not found'))) {
        const actorName = await resolveObjectPath(args, tools, { pathKeys: [], actorKeys: ['actorName', 'name', 'objectPath'] });
        if (actorName) {
          const triedPaths: string[] = [];

          // Strategy 1: Check RootComponent (Most common for transform/mobility)
          try {
            const rootRes = await tools.introspectionTools.getProperty({
              objectPath: actorName,
              propertyName: 'RootComponent'
            }) as InspectResponse;

            // Check if we got a valid object path string or object with path
            const rootValue = rootRes.value as Record<string, unknown> | string | undefined;
            const rootPath = typeof rootValue === 'string' 
              ? rootValue 
              : (typeof rootValue === 'object' && rootValue ? (rootValue.path || rootValue.objectPath) as string : undefined);

            if (rootRes.success && rootPath && typeof rootPath === 'string' && rootPath.length > 0 && rootPath !== 'None') {
              triedPaths.push(rootPath);
              const propRes = await tools.introspectionTools.getProperty({
                objectPath: rootPath,
                propertyName
              }) as InspectResponse;
              if (propRes.success) {
                return cleanObject({
                  ...propRes,
                  message: `Resolved property '${propertyName}' on RootComponent (Smart Lookup)`,
                  foundOnComponent: 'RootComponent'
                });
              }
            }
          } catch (_e) { /* Ignore RootComponent lookup errors */ }

          try {
            // Strategy 2: Iterate all components
            // Use ActorTools directly with the input/original name (args.objectPath)
            const shortName = String(argsTyped.objectPath || '').trim();
            const compsRes = await tools.actorTools.getComponents(shortName) as InspectResponse;

            if (compsRes.success && (Array.isArray(compsRes.components) || Array.isArray(compsRes))) {
              const list: ComponentInfo[] = Array.isArray(compsRes.components) 
                ? compsRes.components 
                : (Array.isArray(compsRes) ? compsRes as unknown as ComponentInfo[] : []);
              const triedPathsInner: string[] = [];
              for (const comp of list) {
                // Use path if available, otherwise construct it (ActorPath.ComponentName)
                // Note: C++ Inspect handler might miss 'path', so we fallback.
                const compName = comp.name;
                const compPath = comp.objectPath || (compName ? `${actorName}.${compName}` : undefined);

                if (!compPath) continue;
                triedPathsInner.push(compPath);

                // Quick check: Try to get property on component
                const compRes = await tools.introspectionTools.getProperty({
                  objectPath: compPath,
                  propertyName
                }) as InspectResponse;

                if (compRes.success) {
                  return cleanObject({
                    ...compRes,
                    message: `Resolved property '${propertyName}' on component '${comp.name}' (Smart Lookup)`,
                    foundOnComponent: comp.name
                  });
                }
              }
              // End of loop - if we're here, nothing found
              return cleanObject({
                ...res,
                message: res.message + ` (Smart Lookup failed. Tried: ${triedPathsInner.length} paths. First: ${triedPathsInner[0]}. Components: ${list.map((c) => c.name).join(',')})`,
                smartLookupTriedPaths: triedPathsInner
              });
            } else {
              return cleanObject({
                ...res,
                message: res.message + ' (Smart Lookup failed: get_components returned ' + (compsRes.success ? 'success but no list' : 'failure: ' + compsRes.error) + ' | Name: ' + shortName + ' Path: ' + actorName + ')',
                smartLookupGetComponentsError: compsRes
              });
            }
          } catch (_e: unknown) {
            const errorMsg = _e instanceof Error ? _e.message : String(_e);
            return cleanObject({
              ...res,
              message: res.message + ' (Smart Lookup exception: ' + errorMsg + ')',
              error: res.error
            });
          }
        }
      }
      return cleanObject(res);
    }
    case 'set_property': {
      const objectPath = await resolveObjectPath(args, tools);
      const params = normalizeArgs(args, [
        { key: 'propertyName', aliases: ['propertyPath'], required: true },
        { key: 'value' }
      ]);
      const propertyName = extractString(params, 'propertyName');
      const value = params.value;

      if (!objectPath) {
        throw new Error('Invalid objectPath: must be a non-empty string');
      }

      const res = await tools.introspectionTools.setProperty({
        objectPath,
        propertyName,
        value
      }) as InspectResponse;

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
      const actorName = await resolveObjectPath(args, tools, { pathKeys: [], actorKeys: ['actorName', 'name', 'objectPath'] });
      if (!actorName) {
        throw new Error('Invalid actorName');
      }

      const res = await executeAutomationRequest(
        tools,
        'inspect',
        {
          action: 'get_components',
          actorName: actorName,
          objectPath: actorName
        },
        'Failed to get components'
      ) as InspectResponse;

      return cleanObject(res);
    }
    case 'get_component_property': {
      const componentObjectPath = await resolveComponentObjectPathFromArgs(args, tools);
      const params = normalizeArgs(args, [
        { key: 'propertyName', aliases: ['propertyPath'], required: true }
      ]);
      const propertyName = extractString(params, 'propertyName');

      const res = await tools.introspectionTools.getProperty({
        objectPath: componentObjectPath,
        propertyName
      });
      return cleanObject(res);
    }
    case 'set_component_property': {
      const componentObjectPath = await resolveComponentObjectPathFromArgs(args, tools);
      const params = normalizeArgs(args, [
        { key: 'propertyName', aliases: ['propertyPath'], required: true },
        { key: 'value' }
      ]);
      const propertyName = extractString(params, 'propertyName');
      const value = params.value;

      const res = await tools.introspectionTools.setProperty({
        objectPath: componentObjectPath,
        propertyName,
        value
      });
      return cleanObject(res);
    }
    case 'get_metadata': {
      const actorName = await resolveObjectPath(args, tools);
      if (!actorName) {
        return cleanObject({ success: false, error: 'INVALID_ARGUMENT', message: 'Invalid actorName' });
      }
      return cleanObject(await tools.actorTools.getMetadata(actorName));
    }
    case 'add_tag': {
      const actorName = await resolveObjectPath(args, tools);
      const params = normalizeArgs(args, [
        { key: 'tag', required: true }
      ]);
      const tag = extractString(params, 'tag');
 
      if (!actorName) {
        return cleanObject({ success: false, error: 'INVALID_ARGUMENT', message: 'Invalid actorName' });
      }
      return cleanObject(await tools.actorTools.addTag({
        actorName,
        tag
      }));
    }
    case 'find_by_tag': {
      const params = normalizeArgs(args, [{ key: 'tag' }]);
      const tag = extractOptionalString(params, 'tag') ?? '';
      return cleanObject(await tools.actorTools.findByTag({
        tag
      }));
    }
    case 'create_snapshot': {
      const actorName = await resolveObjectPath(args, tools);
      if (!actorName) {
        return cleanObject({ success: false, error: 'INVALID_ARGUMENT', message: 'actorName is required for create_snapshot' });
      }
      const snapshotName = typeof argsTyped.snapshotName === 'string' ? argsTyped.snapshotName : '';
      return cleanObject(await tools.actorTools.createSnapshot({
        actorName,
        snapshotName
      }));
    }
    case 'restore_snapshot': {
      const actorName = await resolveObjectPath(args, tools);
      if (!actorName) {
        return cleanObject({ success: false, error: 'INVALID_ARGUMENT', message: 'actorName is required for restore_snapshot' });
      }
      const snapshotName = typeof argsTyped.snapshotName === 'string' ? argsTyped.snapshotName : '';
      return cleanObject(await tools.actorTools.restoreSnapshot({
        actorName,
        snapshotName
      }));
    }
    case 'export': {
      const actorName = await resolveObjectPath(args, tools);
      if (!actorName) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'actorName may be required for export depending on context (exporting actor requires it)'
        });
      }
      const params = normalizeArgs(args, [
        { key: 'destinationPath', aliases: ['outputPath'] }
      ]);
      const destinationPath = extractOptionalString(params, 'destinationPath');
      return cleanObject(await tools.actorTools.exportActor({
        actorName: actorName || '',
        destinationPath
      }));
    }
    case 'delete_object': {
      const actorName = await resolveObjectPath(args, tools);
      if (!actorName) {
        return cleanObject({ success: false, error: 'INVALID_ARGUMENT', message: 'actorName is required for delete_object' });
      }
      try {
        const res = await tools.actorTools.delete({
          actorName
        });
        return cleanObject(res);
      } catch (err: unknown) {
        const msg = String(err instanceof Error ? err.message : err);
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
        return cleanObject({ success: false, error: 'OPERATION_FAILED', message: msg });
      }
    }
    case 'list_objects':
      return cleanObject(await tools.actorTools.listActors(args as { filter?: string }));
    case 'find_by_class': {
      const params = normalizeArgs(args, [
        { key: 'className', aliases: ['classPath'], required: true }
      ]);
      const className = extractString(params, 'className');
      const res = await tools.introspectionTools.findObjectsByClass(className) as InspectResponse;
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
      const actorName = await resolveObjectPath(args, tools);
      if (!actorName) {
        return cleanObject({ success: false, error: 'INVALID_ARGUMENT', message: 'actorName is required for get_bounding_box' });
      }
      try {
        const res = await tools.actorTools.getBoundingBox(actorName);
        return cleanObject(res);
      } catch (err: unknown) {
        const msg = String(err instanceof Error ? err.message : err);
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
        return cleanObject({ success: false, error: 'OPERATION_FAILED', message: msg });
      }
    }
    case 'inspect_class': {
      const params = normalizeArgs(args, [
        { key: 'className', aliases: ['classPath'], required: true }
      ]);
      let className = extractString(params, 'className');

      // Basic smart resolution for common classes if path is incomplete
      // E.g. "Landscape" -> "/Script/Landscape.Landscape" or "/Script/Engine.Landscape"
      if (className && !className.includes('/') && !className.includes('.')) {
        if (className === 'Landscape') {
          className = '/Script/Landscape.Landscape';
        } else if (['Actor', 'Pawn', 'Character', 'StaticMeshActor'].includes(className)) {
          className = `/Script/Engine.${className}`;
        }
      }

      const res = await tools.introspectionTools.getCDO(className) as InspectResponse;
      if (!res || res.success === false) {
        // If first try failed and it looked like a short name, maybe try standard engine path?
        const originalClassName = typeof argsTyped.className === 'string' ? argsTyped.className : '';
        if (originalClassName && !originalClassName.includes('/') && !className.startsWith('/Script/')) {
          const retryName = `/Script/Engine.${originalClassName}`;
          const resRetry = await tools.introspectionTools.getCDO(retryName) as InspectResponse;
          if (resRetry && resRetry.success) {
            return cleanObject(resRetry);
          }
        }

        // Return proper failure state
        return cleanObject({
          success: false,
          error: res?.error || 'OPERATION_FAILED',
          message: res?.message || `inspect_class failed for '${className}'`,
          className,
          cdo: res?.cdo ?? null
        });
      }
      return cleanObject(res);
    }
    default:
      // Fallback to generic automation request if action not explicitly handled
      const res = await executeAutomationRequest(tools, 'inspect', args, 'Automation bridge not available for inspect operations');
      return cleanObject(res) as HandlerResult;
  }
}
