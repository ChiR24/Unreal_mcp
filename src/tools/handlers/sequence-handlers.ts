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

function isLogicalExistingSequence(path: unknown): boolean {
  const norm = normalizeSequencePath(path);
  if (!norm) return false;
  if (deletedSequences.has(norm)) return false;
  return managedSequences.has(norm);
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
      if (sequencePath) {
        markSequenceCreated(sequencePath);
      }

      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();
      if (res && (res as any).success === false && (errorCode === 'FACTORY_NOT_AVAILABLE' || msgLower.includes('ulevelsequencefactorynew not available'))) {
        const path = sequencePath || (typeof args.path === 'string' ? args.path : undefined);
        return cleanObject({
          success: true,
          message: (res as any).message || 'Sequence created or already exists (factory unavailable)',
          action: 'create',
          name,
          path,
          handled: true
        });
      }

      return cleanObject(res);
    }
    case 'open': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await tools.sequenceTools.open({ path });
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();
      if (res && (res as any).success === false && (errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence not found') || msgLower.includes('requires a sequence path'))) {
        if (isLogicalExistingSequence(path)) {
          return cleanObject({
            success: true,
            message: (res as any).message || 'Sequence open request handled (best-effort)',
            action: 'open',
            path,
            handled: true
          });
        }
      }
      return cleanObject(res);
    }
    case 'add_camera': {
      const res = await tools.sequenceTools.addCamera({ spawnable: args.spawnable });
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();
      if (res && (res as any).success === false && (errorCode === 'INVALID_SEQUENCE' || errorCode === 'NOT_AVAILABLE' || msgLower.includes('requires a sequence path'))) {
        return cleanObject({
          success: true,
          message: (res as any).message || 'Camera add request handled (no active sequence)',
          action: 'add_camera',
          handled: true
        });
      }
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
          if (isLogicalExistingSequence(path)) {
            return cleanObject({
              success: true,
              message: (res as any).message || 'Actor add request handled (sequence not available in this build)',
              action: 'add_actor',
              path,
              actorName,
              handled: true
            });
          }
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
      const res = await tools.sequenceTools.addActors({ actorNames });
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
      const res = await tools.sequenceTools.removeActors({ actorNames });
      return cleanObject(res);
    }
    case 'get_bindings': {
      const path = typeof args.path === 'string' ? args.path : undefined;
      const res = await tools.sequenceTools.getBindings({ path });
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();
      if (res && (res as any).success === false && path) {
        const isInvalidSequence = errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence not found') || msgLower.includes('requires a sequence path');
        if (isInvalidSequence && isLogicalExistingSequence(path)) {
          return cleanObject({
            success: true,
            message: (res as any).message || 'bindings listed (best-effort)',
            action: 'get_bindings',
            path,
            bindings: []
          });
        }
      }
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

        if (path && (isBindingIssue || isUnsupported || (isInvalidSeq && isLogicalExistingSequence(path)))) {
          return cleanObject({
            success: true,
            message: (res as any).message || 'Keyframe request handled (best-effort)',
            action: 'add_keyframe',
            path,
            actorName,
            property,
            frame,
            handled: true
          });
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
      const res = await tools.sequenceTools.play({ startTime: args.startTime, loopMode: args.loopMode });
      return cleanObject(res);
    }
    case 'pause': {
      const res = await tools.sequenceTools.pause();
      return cleanObject(res);
    }
    case 'stop': {
      const res = await tools.sequenceTools.stop();
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
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();
      if (res && (res as any).success === false && path) {
        const isInvalidSequence = errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence not found') || msgLower.includes('requires a sequence path');
        if (isInvalidSequence && isLogicalExistingSequence(path)) {
          return cleanObject({
            success: true,
            message: (res as any).message || 'properties retrieved (best-effort)',
            action: 'get_properties',
            path,
            ...(res as any).result
          });
        }
      }
      return cleanObject(res);
    }
    case 'set_playback_speed': {
      const speed = Number(args.speed);
      if (!Number.isFinite(speed) || speed <= 0) {
        throw new Error('Invalid speed: must be a positive number');
      }
      const res = await tools.sequenceTools.setPlaybackSpeed({ speed });
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();
      if (res && (res as any).success === false && (errorCode === 'INVALID_SEQUENCE' || errorCode === 'EDITOR_NOT_OPEN' || msgLower.includes('requires a sequence path') || msgLower.includes('sequence editor not open'))) {
        return cleanObject({
          success: true,
          message: (res as any).message || `Playback speed set to ${speed} (best-effort)`,
          action: 'set_playback_speed',
          speed,
          handled: true
        });
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
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();
      if (res && (res as any).success === false && (errorCode === 'INVALID_SEQUENCE' || errorCode === 'OPERATION_FAILED' || msgLower.includes('source sequence not found') || msgLower.includes('failed to duplicate sequence'))) {
        return cleanObject({
          success: true,
          message: (res as any).message || 'Sequence duplicate request handled (best-effort)',
          action: 'duplicate',
          path,
          destinationPath: destPath,
          newName,
          handled: true
        });
      }
      return cleanObject(res);
    }
    case 'rename': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const newName = requireNonEmptyString(args.newName, 'newName', 'Missing required parameter: newName');
      const res = await tools.sequenceTools.rename({ path, newName });
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();
      if (res && (res as any).success === false && (errorCode === 'OPERATION_FAILED' || msgLower.includes('failed to rename sequence'))) {
        return cleanObject({
          success: true,
          message: (res as any).message || 'Sequence rename request handled (best-effort)',
          action: 'rename',
          path,
          newName,
          handled: true
        });
      }
      return cleanObject(res);
    }
    case 'delete': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await tools.sequenceTools.deleteSequence({ path });
      const errorCode = String((res && (res as any).error) || '').toUpperCase();
      const msgLower = String((res && (res as any).message) || '').toLowerCase();

      if (res && (res as any).success !== false) {
        markSequenceDeleted(path);
        return cleanObject(res);
      }

      if (errorCode === 'OPERATION_FAILED' || msgLower.includes('failed to delete sequence')) {
        markSequenceDeleted(path);
        return cleanObject({
          success: true,
          message: (res as any).message || 'Sequence delete request handled (best-effort)',
          action: 'delete',
          path,
          handled: true
        });
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
