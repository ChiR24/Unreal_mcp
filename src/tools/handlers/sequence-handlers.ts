import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

const managedSequences = new Set<string>();
const deletedSequences = new Set<string>();

function normalizeSequencePath(path: unknown): string | undefined {
  if (typeof path !== 'string') return undefined;
  const trimmed = path.trim();
  return trimmed.length > 0 ? trimmed : undefined;
}

function markSequenceCreated(path: unknown) {
  const norm = normalizeSequencePath(path);
  if (!norm) return;
  deletedSequences.delete(norm);
  managedSequences.add(norm);
}

function markSequenceDeleted(path: unknown) {
  const norm = normalizeSequencePath(path);
  if (!norm) return;
  managedSequences.delete(norm);
  deletedSequences.add(norm);
}



export async function handleSequenceTools(action: string, args: any, tools: ITools) {
  const seqAction = String(action || '').trim();
  switch (seqAction) {
    case 'create': {
      const name = requireNonEmptyString(args.name, 'name', 'Missing required parameter: name');
      const res = await tools.sequenceTools.create({ name, path: args.path });

      let sequencePath: string | undefined;
      if (res && (res as any).result && typeof (res as any).result.sequencePath === 'string') {
        sequencePath = (res as any).result.sequencePath;
      } else if (typeof args.path === 'string' && args.path.trim().length > 0) {
        const basePath = args.path.trim().replace(/\/$/, '');
        sequencePath = `${basePath}/${name}`;
      }
      if (sequencePath && res && (res as any).success !== false) {
        markSequenceCreated(sequencePath);
      }

      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();
      if (res && (res as any).success === false && (errorCode === 'FACTORY_NOT_AVAILABLE' || msgLower.includes('ulevelsequencefactorynew not available'))) {
        const path = sequencePath || (typeof args.path === 'string' ? args.path : undefined);
        return cleanObject({
          success: false,
          error: 'FACTORY_NOT_AVAILABLE',
          message: (res as any).message || 'Sequence creation failed: factory not available',
          action: 'create',
          name,
          path,
          sequencePath,
          handled: true
        });
      }

      return cleanObject(res);
    }
    case 'open': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await tools.sequenceTools.open({ path });
      return cleanObject(res);
    }
    case 'add_camera': {
      const res = await tools.sequenceTools.addCamera({ spawnable: args.spawnable, path: args.path });
      return cleanObject(res);
    }
    case 'add_actor': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const path = typeof args.path === 'string' ? args.path.trim() : '';
      const payload = {
        ...args,
        actorName,
        path: path || args.path,
        subAction: 'add_actor'
      };

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload);

      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();

      if (res && (res as any).success === false && path) {
        const isInvalidSequence = errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence_add_actor requires a sequence path') || msgLower.includes('sequence not found');
        if (isInvalidSequence) {
          return cleanObject({
            success: false,
            error: 'NOT_FOUND',
            message: (res as any).message || 'Sequence not found',
            action: 'add_actor',
            path,
            actorName
          });
        }
      }

      const results = res && (res as any).result && Array.isArray((res as any).result.results)
        ? (res as any).result.results as any[]
        : undefined;
      if (results && results.length) {
        const failed = results.find((item) => item && item.success === false && typeof item.error === 'string');
        if (failed) {
          const errText = String(failed.error).toLowerCase();
          if (errText.includes('actor not found')) {
            return cleanObject({
              success: false,
              error: 'NOT_FOUND',
              message: failed.error,
              action: 'add_actor',
              path: path || undefined,
              actorName
            });
          }
        }
      }

      return cleanObject(res);
    }
    case 'add_actors': {
      const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames : [];
      const res = await tools.sequenceTools.addActors({ actorNames, path: args.path });
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();
      if (actorNames.length === 0 && res && (res as any).success === false && errorCode === 'INVALID_ARGUMENT') {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: (res as any).message || 'Invalid argument: actorNames required',
          action: 'add_actors',
          actorNames
        });
      }
      if (res && (res as any).success === false && msgLower.includes('actor not found')) {
        return cleanObject({
          success: false,
          error: 'NOT_FOUND',
          message: (res as any).message || 'Actor not found',
          action: 'add_actors',
          actorNames
        });
      }
      return cleanObject(res);
    }
    case 'remove_actors': {
      const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames : [];
      const res = await tools.sequenceTools.removeActors({ actorNames, path: args.path });
      return cleanObject(res);
    }
    case 'get_bindings': {
      const path = typeof args.path === 'string' ? args.path : undefined;
      const res = await tools.sequenceTools.getBindings({ path });
      return cleanObject(res);
    }
    case 'add_keyframe': {
      const path = typeof args.path === 'string' ? args.path.trim() : '';
      const actorName = typeof args.actorName === 'string' ? args.actorName : undefined;
      const property = typeof args.property === 'string' ? args.property : undefined;
      const frame = typeof args.frame === 'number' ? args.frame : Number(args.frame);

      const payload = {
        ...args,
        path: path || args.path,
        actorName,
        property,
        frame,
        subAction: 'add_keyframe'
      };

      // Fix: Map common property names to internal names
      if (property === 'Location') {
        payload.property = 'Transform';
        payload.value = { location: args.value };
      } else if (property === 'Rotation') {
        payload.property = 'Transform';
        payload.value = { rotation: args.value };
      } else if (property === 'Scale') {
        payload.property = 'Transform';
        payload.value = { scale: args.value };
      }

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload);
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();

      // Keep explicit INVALID_ARGUMENT for missing frame as a real error
      if (errorCode === 'INVALID_ARGUMENT' || msgLower.includes('frame number is required')) {
        return cleanObject(res);
      }

      if (res && (res as any).success === false) {
        const isBindingIssue = errorCode === 'BINDING_NOT_FOUND' || msgLower.includes('binding not found');
        const isUnsupported = errorCode === 'UNSUPPORTED_PROPERTY' || msgLower.includes('unsupported property') || msgLower.includes('invalid_sequence_type');
        const isInvalidSeq = errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence not found') || msgLower.includes('requires a sequence path');

        if (path && isInvalidSeq) {
          return cleanObject({
            success: false,
            error: 'NOT_FOUND',
            message: (res as any).message || 'Sequence not found',
            action: 'add_keyframe',
            path,
            actorName,
            property,
            frame
          });
        }

        // Preserve plugin-provided failure for binding / unsupported-property cases
        if (path && (isBindingIssue || isUnsupported)) {
          return cleanObject(res);
        }
      }

      return cleanObject(res);
    }
    case 'add_spawnable_from_class': {
      const className = requireNonEmptyString(args.className, 'className', 'Missing required parameter: className');
      const res = await tools.sequenceTools.addSpawnableFromClass({ className, path: args.path });
      return cleanObject(res);
    }
    case 'play': {
      const res = await tools.sequenceTools.play({ path: args.path, startTime: args.startTime, loopMode: args.loopMode });
      return cleanObject(res);
    }
    case 'pause': {
      const res = await tools.sequenceTools.pause({ path: args.path });
      return cleanObject(res);
    }
    case 'stop': {
      const res = await tools.sequenceTools.stop({ path: args.path });
      return cleanObject(res);
    }
    case 'set_properties': {
      const res = await tools.sequenceTools.setSequenceProperties({
        path: args.path,
        frameRate: args.frameRate,
        lengthInFrames: args.lengthInFrames,
        playbackStart: args.playbackStart,
        playbackEnd: args.playbackEnd
      });
      return cleanObject(res);
    }
    case 'get_properties': {
      const path = typeof args.path === 'string' ? args.path : undefined;
      const res = await tools.sequenceTools.getSequenceProperties({ path });
      return cleanObject(res);
    }
    case 'set_playback_speed': {
      const speed = Number(args.speed);
      if (!Number.isFinite(speed) || speed <= 0) {
        throw new Error('Invalid speed: must be a positive number');
      }
      // Try setting speed
      let res = await tools.sequenceTools.setPlaybackSpeed({ speed, path: args.path });

      // Fix: Auto-open if editor not open
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      if ((!res || res.success === false) && errorCode === 'EDITOR_NOT_OPEN' && args.path) {
        // Attempt to open the sequence
        await tools.sequenceTools.open({ path: args.path });

        // Wait a short moment for editor to initialize on game thread
        await new Promise(resolve => setTimeout(resolve, 1000));

        // Retry
        res = await tools.sequenceTools.setPlaybackSpeed({ speed, path: args.path });
      }

      return cleanObject(res);
    }
    case 'list': {
      const res = await tools.sequenceTools.list({ path: args.path });
      return cleanObject(res);
    }
    case 'duplicate': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const destDir = requireNonEmptyString(args.destinationPath, 'destinationPath', 'Missing required parameter: destinationPath');
      const newName = requireNonEmptyString(args.newName || path.split('/').pop(), 'newName', 'Missing required parameter: newName');
      const baseDir = destDir.replace(/\/$/, '');
      const destPath = `${baseDir}/${newName}`;
      const res = await tools.sequenceTools.duplicate({ path, destinationPath: destPath });
      return cleanObject(res);
    }
    case 'rename': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const newName = requireNonEmptyString(args.newName, 'newName', 'Missing required parameter: newName');
      const res = await tools.sequenceTools.rename({ path, newName });
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();
      if (res && (res as any).success === false && (errorCode === 'OPERATION_FAILED' || msgLower.includes('failed to rename sequence'))) {
        // Return actual failure, not best-effort success - rename is a destructive operation
        return cleanObject({
          success: false,
          error: 'OPERATION_FAILED',
          message: (res as any).message || 'Failed to rename sequence',
          action: 'rename',
          path,
          newName
        });
      }
      return cleanObject(res);
    }
    case 'delete': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await tools.sequenceTools.deleteSequence({ path });

      if (res && (res as any).success !== false) {
        markSequenceDeleted(path);
      }
      return cleanObject(res);
    }
    case 'get_metadata': {
      const res = await tools.sequenceTools.getMetadata({ path: args.path });
      return cleanObject(res);
    }
    case 'set_metadata': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const metadata = (args.metadata && typeof args.metadata === 'object') ? args.metadata : {};
      const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: path, metadata });
      return cleanObject(res);
    }
    case 'add_track': {
      // Forward add_track to the C++ plugin - it requires MovieScene API
      const path = typeof args.path === 'string' ? args.path.trim() : '';
      const trackType = typeof args.trackType === 'string' ? args.trackType : '';
      const trackName = typeof args.trackName === 'string' ? args.trackName : '';
      const actorName = typeof args.actorName === 'string' ? args.actorName : undefined;

      // Fix: Check if actor is bound before adding track
      if (actorName) {
        const bindingsRes = await tools.sequenceTools.getBindings({ path });
        if (bindingsRes && bindingsRes.success) {
          const bindings = (bindingsRes.bindings as any[]) || [];
          const isBound = bindings.some((b: any) => b.name === actorName);
          if (!isBound) {
            return cleanObject({
              success: false,
              error: 'BINDING_NOT_FOUND',
              message: `Actor '${actorName}' is not bound to this sequence. Please call 'add_actor' first.`,
              action: 'add_track',
              path,
              actorName
            });
          }
        }
      }

      const payload = {
        ...args,
        path: path || args.path,
        trackType,
        trackName,
        actorName,
        subAction: 'add_track'
      };

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload);
      return cleanObject(res);
    }
    case 'add_section': {
      // Forward add_section to C++
      const payload = { ...args, subAction: 'add_section' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'remove_track': {
      // Forward remove_track to C++
      const payload = { ...args, subAction: 'remove_track' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'set_track_muted': {
      const payload = { ...args, subAction: 'set_track_muted' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'set_track_solo': {
      const payload = { ...args, subAction: 'set_track_solo' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'set_track_locked': {
      const payload = { ...args, subAction: 'set_track_locked' };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'list_tracks': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await tools.sequenceTools.listTracks({ path });
      return cleanObject(res);
    }
    case 'set_work_range': {
      const start = Number(args.start);
      const end = Number(args.end);
      // Validate start/end are numbers
      if (!Number.isFinite(start)) throw new Error('Invalid start: must be a number');
      if (!Number.isFinite(end)) throw new Error('Invalid end: must be a number');

      const res = await tools.sequenceTools.setWorkRange({
        path: args.path,
        start,
        end
      });
      return cleanObject(res);
    }
    default:
      // Ensure subAction is set for compatibility with C++ handler expectations
      if (args.action && !args.subAction) {
        args.subAction = args.action;
      }
      return await executeAutomationRequest(tools, 'manage_sequence', args);
  }
}
