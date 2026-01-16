import { cleanObject } from '../../utils/safe-json.js';
import { ITools, StandardActionResponse } from '../../types/tool-interfaces.js';
import type { HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

/** Extended response with common sequence fields */
interface SequenceActionResponse extends StandardActionResponse {
  result?: {
    sequencePath?: string;
    results?: Array<{ success?: boolean; error?: string }>;
    [key: string]: unknown;
  };
  bindings?: Array<{ name?: string;[key: string]: unknown }>;
  message?: string;
}

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
  deletedSequences.delete(norm);
}

/** Helper to safely get string from error/message */
function getErrorString(res: SequenceActionResponse | null | undefined): string {
  if (!res) return '';
  return typeof res.error === 'string' ? res.error : '';
}

function getMessageString(res: SequenceActionResponse | null | undefined): string {
  if (!res) return '';
  return typeof res.message === 'string' ? res.message : '';
}



export async function handleSequenceTools(action: string, args: Record<string, unknown>, tools: ITools) {
  const seqAction = String(action || '').trim();
  switch (seqAction) {
    case 'create': {
      const name = requireNonEmptyString(args.name, 'name', 'Missing required parameter: name');
      const res = await tools.sequenceTools.create({ name, path: args.path as string | undefined }) as SequenceActionResponse;

      let sequencePath: string | undefined;
      if (res && res.result && typeof res.result.sequencePath === 'string') {
        sequencePath = res.result.sequencePath;
      } else if (typeof args.path === 'string' && args.path.trim().length > 0) {
        const basePath = args.path.trim().replace(/\/$/, '');
        sequencePath = `${basePath}/${name}`;
      }
      if (sequencePath && res && res.success !== false) {
        markSequenceCreated(sequencePath);
      }

      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();
      if (res && res.success === false && (errorCode === 'FACTORY_NOT_AVAILABLE' || msgLower.includes('ulevelsequencefactorynew not available'))) {
        const path = sequencePath || (typeof args.path === 'string' ? args.path : undefined);
        return cleanObject({
          success: false,
          error: 'FACTORY_NOT_AVAILABLE',
          message: res.message || 'Sequence creation failed: factory not available',
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
      const res = await tools.sequenceTools.addCamera({ spawnable: args.spawnable as boolean | undefined, path: args.path as string | undefined });
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

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload) as SequenceActionResponse;

      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();

      if (res && res.success === false && path) {
        const isInvalidSequence = errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence_add_actor requires a sequence path') || msgLower.includes('sequence not found');
        if (isInvalidSequence) {
          return cleanObject({
            success: false,
            error: 'NOT_FOUND',
            message: res.message || 'Sequence not found',
            action: 'add_actor',
            path,
            actorName
          });
        }
      }

      const results = res && res.result && Array.isArray(res.result.results)
        ? res.result.results
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
      const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames as string[] : [];
      const res = await tools.sequenceTools.addActors({ actorNames, path: args.path as string | undefined }) as SequenceActionResponse;
      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();
      if (actorNames.length === 0 && res && res.success === false && errorCode === 'INVALID_ARGUMENT') {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: res.message || 'Invalid argument: actorNames required',
          action: 'add_actors',
          actorNames
        });
      }
      if (res && res.success === false && msgLower.includes('actor not found')) {
        return cleanObject({
          success: false,
          error: 'NOT_FOUND',
          message: res.message || 'Actor not found',
          action: 'add_actors',
          actorNames
        });
      }
      return cleanObject(res);
    }
    case 'remove_actors': {
      const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames as string[] : [];
      const res = await tools.sequenceTools.removeActors({ actorNames, path: args.path as string | undefined });
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

      const payload: Record<string, unknown> = {
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

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload) as SequenceActionResponse;
      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();

      // Keep explicit INVALID_ARGUMENT for missing frame as a real error
      if (errorCode === 'INVALID_ARGUMENT' || msgLower.includes('frame number is required')) {
        return cleanObject(res);
      }

      if (res && res.success === false) {
        const isBindingIssue = errorCode === 'BINDING_NOT_FOUND' || msgLower.includes('binding not found');
        const isUnsupported = errorCode === 'UNSUPPORTED_PROPERTY' || msgLower.includes('unsupported property') || msgLower.includes('invalid_sequence_type');
        const isInvalidSeq = errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence not found') || msgLower.includes('requires a sequence path');

        if (path && isInvalidSeq) {
          return cleanObject({
            success: false,
            error: 'NOT_FOUND',
            message: res.message || 'Sequence not found',
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
      const res = await tools.sequenceTools.addSpawnableFromClass({ className, path: args.path as string | undefined });
      return cleanObject(res);
    }
    case 'play': {
      const res = await tools.sequenceTools.play({ path: args.path as string | undefined, startTime: args.startTime as number | undefined, loopMode: args.loopMode as 'once' | 'loop' | 'pingpong' | undefined });
      return cleanObject(res);
    }
    case 'pause': {
      const res = await tools.sequenceTools.pause({ path: args.path as string | undefined });
      return cleanObject(res);
    }
    case 'stop': {
      const res = await tools.sequenceTools.stop({ path: args.path as string | undefined });
      return cleanObject(res);
    }
    case 'set_properties': {
      const res = await tools.sequenceTools.setSequenceProperties({
        path: args.path as string | undefined,
        frameRate: args.frameRate as number | undefined,
        lengthInFrames: args.lengthInFrames as number | undefined,
        playbackStart: args.playbackStart as number | undefined,
        playbackEnd: args.playbackEnd as number | undefined
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
      let res = await tools.sequenceTools.setPlaybackSpeed({ speed, path: args.path as string | undefined }) as SequenceActionResponse;

      // Fix: Auto-open if editor not open
      const errorCode = getErrorString(res).toUpperCase();
      if ((!res || res.success === false) && errorCode === 'EDITOR_NOT_OPEN' && args.path) {
        // Attempt to open the sequence
        await tools.sequenceTools.open({ path: args.path as string });

        // Wait a short moment for editor to initialize on game thread
        await new Promise(resolve => setTimeout(resolve, 1000));

        // Retry
        res = await tools.sequenceTools.setPlaybackSpeed({ speed, path: args.path as string | undefined }) as SequenceActionResponse;
      }

      return cleanObject(res);
    }
    case 'list': {
      const res = await tools.sequenceTools.list({ path: args.path as string | undefined });
      return cleanObject(res);
    }
    case 'duplicate': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const destDir = requireNonEmptyString(args.destinationPath, 'destinationPath', 'Missing required parameter: destinationPath');
      const defaultNewName = path.split('/').pop() || '';
      const newName = requireNonEmptyString(args.newName || defaultNewName, 'newName', 'Missing required parameter: newName');
      const baseDir = destDir.replace(/\/$/, '');
      const destPath = `${baseDir}/${newName}`;
      const res = await tools.sequenceTools.duplicate({ path, destinationPath: destPath });
      return cleanObject(res);
    }
    case 'rename': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const newName = requireNonEmptyString(args.newName, 'newName', 'Missing required parameter: newName');
      const res = await tools.sequenceTools.rename({ path, newName }) as SequenceActionResponse;
      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();
      if (res && res.success === false && (errorCode === 'OPERATION_FAILED' || msgLower.includes('failed to rename sequence'))) {
        // Return actual failure, not best-effort success - rename is a destructive operation
        return cleanObject({
          success: false,
          error: 'OPERATION_FAILED',
          message: res.message || 'Failed to rename sequence',
          action: 'rename',
          path,
          newName
        });
      }
      return cleanObject(res);
    }
    case 'delete': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await tools.sequenceTools.deleteSequence({ path }) as SequenceActionResponse;

      if (res && res.success !== false) {
        markSequenceDeleted(path);
      }
      return cleanObject(res);
    }
    case 'get_metadata': {
      const res = await tools.sequenceTools.getMetadata({ path: args.path as string });
      return cleanObject(res);
    }
    case 'set_metadata': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const metadata = (args.metadata && typeof args.metadata === 'object') ? args.metadata as HandlerResult : {};
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
        const bindingsRes = await tools.sequenceTools.getBindings({ path }) as SequenceActionResponse;
        if (bindingsRes && bindingsRes.success) {
          const bindings = bindingsRes.bindings || [];
          const isBound = bindings.some((b) => b.name === actorName);
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
        path: args.path as string | undefined,
        start,
        end
      });
      return cleanObject(res);
    }
    // Wave 4.11-4.20: Sequencer Enhancement Actions
    case 'create_media_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const payload = {
        ...args,
        path,
        subAction: 'create_media_track'
      };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'configure_sequence_streaming': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const payload = {
        ...args,
        path,
        subAction: 'configure_sequence_streaming'
      };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'create_event_trigger_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const payload = {
        ...args,
        path,
        subAction: 'create_event_trigger_track'
      };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'add_procedural_camera_shake': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const payload = {
        ...args,
        path,
        subAction: 'add_procedural_camera_shake'
      };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'configure_sequence_lod': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const payload = {
        ...args,
        path,
        subAction: 'configure_sequence_lod'
      };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'create_camera_cut_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const payload = {
        ...args,
        path,
        subAction: 'create_camera_cut_track'
      };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'configure_mrq_settings': {
      const payload = {
        ...args,
        subAction: 'configure_mrq_settings'
      };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'batch_render_sequences': {
      const sequencePaths = Array.isArray(args.sequencePaths) ? args.sequencePaths : [];
      if (sequencePaths.length === 0) {
        throw new Error('Missing required parameter: sequencePaths (array of sequence paths)');
      }
      const payload = {
        ...args,
        sequencePaths,
        subAction: 'batch_render_sequences'
      };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'get_sequence_bindings': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const payload = {
        ...args,
        path,
        subAction: 'get_sequence_bindings'
      };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    case 'configure_audio_track': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const payload = {
        ...args,
        path,
        subAction: 'configure_audio_track'
      };
      return cleanObject(await executeAutomationRequest(tools, 'manage_sequence', payload));
    }
    default:
      // Ensure subAction is set for compatibility with C++ handler expectations
      if (args.action && !args.subAction) {
        args.subAction = args.action;
      }
      return await executeAutomationRequest(tools, 'manage_sequence', args);
  }
}
