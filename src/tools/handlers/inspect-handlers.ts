import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';
import { normalizeArgs, resolveObjectPath } from './argument-helper.js';

async function resolveComponentObjectPathFromArgs(args: any, tools: ITools): Promise<string> {
  const componentName = typeof args.componentName === 'string' ? args.componentName.trim() : '';
  const componentPath = typeof args.componentPath === 'string' ? args.componentPath.trim() : '';

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
    throw new Error('Invalid actorName: required to resolve componentName');
  }
  if (!componentName) {
    throw new Error('Invalid componentName: must be a non-empty string');
  }

  // Use inspect:get_components to find the exact component path
  const compsRes: any = await executeAutomationRequest(
    tools,
    'inspect',
    {
      action: 'get_components',
      actorName: actorName,
      objectPath: actorName
    },
    'Failed to get components'
  );

  let components: any[] = [];
  if (compsRes.success) {
    components = Array.isArray(compsRes?.components) ? compsRes.components : [];
  }

  const needle = componentName.toLowerCase();

  if (components.length > 0) {
    // 1. Exact Name/Path Match
    let match = components.find((c) => String(c?.name || '').toLowerCase() === needle)
      ?? components.find((c) => String(c?.path || '').toLowerCase() === needle)
      ?? components.find((c) => String(c?.path || '').toLowerCase().endsWith(`:${needle}`))
      ?? components.find((c) => String(c?.path || '').toLowerCase().endsWith(`.${needle}`));

    // 2. Fuzzy/StartsWith Match (e.g. "StaticMeshComponent" -> "StaticMeshComponent0")
    if (!match) {
      match = components.find((c) => String(c?.name || '').toLowerCase().startsWith(needle));
    }

    // RESOLUTION LOGIC FIX:
    // If we have a match, we MUST use its path OR its name.
    // We cannot fall back to 'needle' or 'args.componentName' if we found a better specific match.
    if (match) {
      if (typeof match.path === 'string' && match.path.trim().length > 0) {
        return match.path.trim();
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


export async function handleInspectTools(action: string, args: any, tools: ITools) {
  switch (action) {
    case 'inspect_object': {
      const objectPath = await resolveObjectPath(args, tools);
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
      const objectPath = await resolveObjectPath(args, tools);
      const propertyName = normalizeArgs(args, [{ key: 'propertyName', aliases: ['propertyPath'], required: true }]).propertyName;

      if (!objectPath) {
        throw new Error('Invalid objectPath: must be a non-empty string');
      }

      const res = await tools.introspectionTools.getProperty({
        objectPath,
        propertyName
      });

      // Smart Lookup: If property not found on the Actor, try to find it on components
      if (!res.success && (res.error === 'PROPERTY_NOT_FOUND' || String(res.error).includes('not found'))) {
        const actorName = await resolveObjectPath(args, tools, { pathKeys: [], actorKeys: ['actorName', 'name', 'objectPath'] });
        if (actorName) {
          const triedPaths: string[] = [];

          // Strategy 1: Check RootComponent (Most common for transform/mobility)
          try {
            const rootRes: any = await tools.introspectionTools.getProperty({
              objectPath: actorName,
              propertyName: 'RootComponent'
            });

            // Check if we got a valid object path string or object with path
            const rootPath = typeof rootRes.value === 'string' ? rootRes.value : (rootRes.value?.path || rootRes.value?.objectPath);

            if (rootRes.success && rootPath && typeof rootPath === 'string' && rootPath.length > 0 && rootPath !== 'None') {
              triedPaths.push(rootPath);
              const propRes: any = await tools.introspectionTools.getProperty({
                objectPath: rootPath,
                propertyName
              });
              if (propRes.success) {
                return cleanObject({
                  ...propRes,
                  message: `Resolved property '${propertyName}' on RootComponent (Smart Lookup)`,
                  foundOnComponent: 'RootComponent'
                });
              }
            }
          } catch (e) { /* Ignore RootComponent lookup errors */ }

          try {
            // Strategy 2: Iterate all components
            // Use ActorTools directly with the input/original name (args.objectPath)
            const shortName = String(args.objectPath || '').trim();
            const compsRes: any = await tools.actorTools.getComponents(shortName);

            if (compsRes.success && (Array.isArray(compsRes.components) || Array.isArray(compsRes))) {
              const list = Array.isArray(compsRes.components) ? compsRes.components : (Array.isArray(compsRes) ? compsRes : []);
              const triedPaths: string[] = [];
              for (const comp of list) {
                // Use path if available, otherwise construct it (ActorPath.ComponentName)
                // Note: C++ Inspect handler might miss 'path', so we fallback.
                const compName = comp.name;
                const compPath = comp.path || (compName ? `${actorName}.${compName}` : undefined);

                if (!compPath) continue;
                triedPaths.push(compPath);

                // Quick check: Try to get property on component
                const compRes: any = await tools.introspectionTools.getProperty({
                  objectPath: compPath,
                  propertyName
                });

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
                message: res.message + ` (Smart Lookup failed. Tried: ${triedPaths.length} paths. First: ${triedPaths[0]}. Components: ${list.map((c: any) => c.name).join(',')})`,
                smartLookupTriedPaths: triedPaths
              });
            } else {
              return cleanObject({
                ...res,
                message: res.message + ' (Smart Lookup failed: get_components returned ' + (compsRes.success ? 'success but no list' : 'failure: ' + compsRes.error) + ' | Name: ' + shortName + ' Path: ' + actorName + ')',
                smartLookupGetComponentsError: compsRes
              });
            }
          } catch (_e: any) {
            return cleanObject({
              ...res,
              message: res.message + ' (Smart Lookup exception: ' + _e.message + ')',
              error: _e
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

      if (!objectPath) {
        throw new Error('Invalid objectPath: must be a non-empty string');
      }

      const res: any = await tools.introspectionTools.setProperty({
        objectPath,
        propertyName: params.propertyName,
        value: params.value
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
      const actorName = await resolveObjectPath(args, tools, { pathKeys: [], actorKeys: ['actorName', 'name', 'objectPath'] });
      if (!actorName) {
        throw new Error('Invalid actorName');
      }

      const res: any = await executeAutomationRequest(
        tools,
        'inspect',
        {
          action: 'get_components',
          actorName: actorName,
          objectPath: actorName
        },
        'Failed to get components'
      );

      return cleanObject(res);
    }
    case 'get_component_property': {
      const componentObjectPath = await resolveComponentObjectPathFromArgs(args, tools);
      const params = normalizeArgs(args, [
        { key: 'propertyName', aliases: ['propertyPath'], required: true }
      ]);

      const res = await tools.introspectionTools.getProperty({
        objectPath: componentObjectPath,
        propertyName: params.propertyName
      });
      return cleanObject(res);
    }
    case 'set_component_property': {
      const componentObjectPath = await resolveComponentObjectPathFromArgs(args, tools);
      const params = normalizeArgs(args, [
        { key: 'propertyName', aliases: ['propertyPath'], required: true },
        { key: 'value' }
      ]);

      const res = await tools.introspectionTools.setProperty({
        objectPath: componentObjectPath,
        propertyName: params.propertyName,
        value: params.value
      });
      return cleanObject(res);
    }
    case 'get_metadata': {
      const actorName = await resolveObjectPath(args, tools);
      if (!actorName) throw new Error('Invalid actorName');
      return cleanObject(await tools.actorTools.getMetadata(actorName));
    }
    case 'add_tag': {
      const actorName = await resolveObjectPath(args, tools);
      const params = normalizeArgs(args, [
        { key: 'tag', required: true }
      ]);

      if (!actorName) throw new Error('Invalid actorName');
      return cleanObject(await tools.actorTools.addTag({
        actorName,
        tag: params.tag
      }));
    }
    case 'find_by_tag':
      const params = normalizeArgs(args, [{ key: 'tag' }]);
      return cleanObject(await tools.actorTools.findByTag({
        tag: params.tag
      }));
    case 'create_snapshot': {
      const actorName = await resolveObjectPath(args, tools);
      if (!actorName) throw new Error('actorName is required for create_snapshot');
      return cleanObject(await tools.actorTools.createSnapshot({
        actorName,
        snapshotName: args.snapshotName
      }));
    }
    case 'restore_snapshot': {
      const actorName = await resolveObjectPath(args, tools);
      if (!actorName) throw new Error('actorName is required for restore_snapshot');
      return cleanObject(await tools.actorTools.restoreSnapshot({
        actorName,
        snapshotName: args.snapshotName
      }));
    }
    case 'export': {
      const actorName = await resolveObjectPath(args, tools);
      if (!actorName) throw new Error('actorName may be required for export depending on context (exporting actor requires it)');
      const params = normalizeArgs(args, [
        { key: 'destinationPath', aliases: ['outputPath'] }
      ]);
      return cleanObject(await tools.actorTools.exportActor({
        actorName: actorName || '',
        destinationPath: params.destinationPath
      }));
    }
    case 'delete_object': {
      const actorName = await resolveObjectPath(args, tools);
      try {
        if (!actorName) throw new Error('actorName is required for delete_object');
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
      return cleanObject(await tools.actorTools.listActors(args));
    case 'find_by_class': {
      const params = normalizeArgs(args, [
        { key: 'className', aliases: ['classPath'], required: true }
      ]);
      const res: any = await tools.introspectionTools.findObjectsByClass(params.className);
      if (!res || res.success === false) {
        // Return proper failure state
        return cleanObject({
          success: false,
          error: res?.error || 'OPERATION_FAILED',
          message: res?.message || 'find_by_class failed',
          className: params.className,
          objects: [],
          count: 0
        });
      }
      return cleanObject(res);
    }
    case 'get_bounding_box': {
      const actorName = await resolveObjectPath(args, tools);
      try {
        if (!actorName) throw new Error('actorName is required for get_bounding_box');
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
      const params = normalizeArgs(args, [
        { key: 'className', aliases: ['classPath'], required: true }
      ]);
      let className = params.className;

      // Basic smart resolution for common classes if path is incomplete
      // E.g. "Landscape" -> "/Script/Landscape.Landscape" or "/Script/Engine.Landscape"
      if (className && !className.includes('/') && !className.includes('.')) {
        if (className === 'Landscape') {
          className = '/Script/Landscape.Landscape';
        } else if (['Actor', 'Pawn', 'Character', 'StaticMeshActor'].includes(className)) {
          className = `/Script/Engine.${className}`;
        }
      }

      const res: any = await tools.introspectionTools.getCDO(className);
      if (!res || res.success === false) {
        // If first try failed and it looked like a short name, maybe try standard engine path?
        if (args.className && !args.className.includes('/') && !className.startsWith('/Script/')) {
          const retryName = `/Script/Engine.${args.className}`;
          const resRetry: any = await tools.introspectionTools.getCDO(retryName);
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
      return cleanObject(res);
  }
}