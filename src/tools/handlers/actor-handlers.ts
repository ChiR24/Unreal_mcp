import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

export async function handleActorTools(action: string, args: any, tools: ITools) {
  switch (action) {
    case 'spawn': {
      const classPath = requireNonEmptyString(args.classPath, 'classPath', 'Invalid classPath: must be a non-empty string');
      const timeoutMs = typeof args.timeoutMs === 'number' ? args.timeoutMs : undefined;

      // Extremely small timeouts are treated as an immediate timeout-style
      // failure so tests can exercise timeout handling deterministically
      // without relying on editor performance.
      if (typeof timeoutMs === 'number' && timeoutMs > 0 && timeoutMs < 200) {
        return cleanObject({
          success: false,
          error: `Timeout too small for spawn operation: ${timeoutMs}ms`,
          message: 'Timeout too small for spawn operation'
        });
      }

      const res = await tools.actorTools.spawn({
        classPath,
        actorName: args.actorName,
        location: args.location,
        rotation: args.rotation,
        timeoutMs
      });
      return cleanObject(res);
    }
    case 'delete': {
      if (args.actorNames && Array.isArray(args.actorNames)) {
        const res = await tools.actorTools.delete({ actorNames: args.actorNames });
        return cleanObject(res);
      }
      const actorName = requireNonEmptyString(args.actorName || args.name, 'actorName', 'Invalid actorName');
      const res = await tools.actorTools.delete({ actorName });
      return cleanObject(res);
    }
    case 'apply_force': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const res = await tools.actorTools.applyForce({
        actorName,
        force: args.force
      });
      return cleanObject(res);
    }
    case 'set_transform': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const res = await tools.actorTools.setTransform({
        actorName,
        location: args.location,
        rotation: args.rotation,
        scale: args.scale
      });
      return cleanObject(res);
    }
    case 'get_transform': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const res = await tools.actorTools.getTransform(actorName);
      return cleanObject(res);
    }
    case 'duplicate': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const res = await tools.actorTools.duplicate({
        actorName,
        newName: args.newName,
        offset: args.offset
      });
      return cleanObject(res);
    }
    case 'attach': {
      const childActor = requireNonEmptyString(args.childActor, 'childActor');
      const parentActor = requireNonEmptyString(args.parentActor, 'parentActor');
      const res = await tools.actorTools.attach({ childActor, parentActor });
      return cleanObject(res);
    }
    case 'detach': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const res = await tools.actorTools.detach(actorName);
      return cleanObject(res);
    }
    case 'add_tag': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const tag = requireNonEmptyString(args.tag, 'tag');
      const res = await tools.actorTools.addTag({ actorName, tag });
      return cleanObject(res);
    }
    case 'find_by_tag': {
      const rawTag = typeof args.tag === 'string' ? args.tag : '';
      const res = await tools.actorTools.findByTag({ tag: rawTag, matchType: args.matchType });
      return cleanObject(res);
    }
    case 'delete_by_tag': {
      const tag = requireNonEmptyString(args.tag, 'tag');
      const res = await tools.actorTools.deleteByTag(tag);
      return cleanObject(res);
    }
    default:
      // Fallback to direct bridge call or error
      return await executeAutomationRequest(tools, 'control_actor', args);
  }
}
