import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

export async function handleSequenceTools(action: string, args: any, tools: ITools) {
  const seqAction = String(action || '').trim();
  switch (seqAction) {
    case 'create': {
      const name = requireNonEmptyString(args.name, 'name', 'Missing required parameter: name');
      const res = await tools.sequenceTools.create({ name, path: args.path });
      return cleanObject(res);
    }
    case 'open': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await tools.sequenceTools.open({ path });
      return cleanObject(res);
    }
    case 'add_camera': {
      const res = await tools.sequenceTools.addCamera({ spawnable: args.spawnable });
      return cleanObject(res);
    }
    case 'add_actor': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName', 'Missing required parameter: actorName');
      const res = await tools.sequenceTools.addActor({ actorName, createBinding: args.createBinding });
      return cleanObject(res);
    }
    case 'add_actors': {
      const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames : [];
      const res = await tools.sequenceTools.addActors({ actorNames });
      return cleanObject(res);
    }
    case 'remove_actors': {
      const actorNames: string[] = Array.isArray(args.actorNames) ? args.actorNames : [];
      const res = await tools.sequenceTools.removeActors({ actorNames });
      return cleanObject(res);
    }
    case 'get_bindings': {
      const res = await tools.sequenceTools.getBindings({ path: args.path });
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
      const res = await tools.sequenceTools.getSequenceProperties({ path: args.path });
      return cleanObject(res);
    }
    case 'set_playback_speed': {
      const speed = Number(args.speed);
      if (!Number.isFinite(speed) || speed <= 0) {
        throw new Error('Invalid speed: must be a positive number');
      }
      const res = await tools.sequenceTools.setPlaybackSpeed({ speed });
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
      return cleanObject(res);
    }
    case 'delete': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await tools.sequenceTools.deleteSequence({ path });
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
    default:
      return await executeAutomationRequest(tools, 'manage_sequence', args);
  }
}
