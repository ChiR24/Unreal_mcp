import { UnrealBridge } from '../unreal-bridge.js';
import { Logger } from '../utils/logger.js';

export interface LevelSequence {
  path: string;
  name: string;
  duration?: number;
  frameRate?: number;
  bindings?: SequenceBinding[];
}

export interface SequenceBinding {
  id: string;
  name: string;
  type: 'actor' | 'camera' | 'spawnable';
  tracks?: SequenceTrack[];
}

export interface SequenceTrack {
  name: string;
  type: string;
  sections?: any[];
}

export class SequenceTools {
  private log = new Logger('SequenceTools');
  private sequenceCache = new Map<string, LevelSequence>();
  private retryAttempts = 3;
  private retryDelay = 1000;
  
  constructor(private bridge: UnrealBridge) {}

  /**
   * Execute with retry logic for transient failures
   */
  private async executeWithRetry<T>(
    operation: () => Promise<T>,
    operationName: string
  ): Promise<T> {
    let lastError: any;
    
    for (let attempt = 1; attempt <= this.retryAttempts; attempt++) {
      try {
        return await operation();
      } catch (error: any) {
        lastError = error;
        this.log.warn(`${operationName} attempt ${attempt} failed: ${error.message || error}`);
        
        if (attempt < this.retryAttempts) {
          await new Promise(resolve => 
            setTimeout(resolve, this.retryDelay * attempt)
          );
        }
      }
    }
    
    throw lastError;
  }

  /**
   * Parse Python execution result with better error handling
   */
  private parsePythonResult(resp: any, operationName: string): any {
    let out = '';
    if (resp?.LogOutput && Array.isArray((resp as any).LogOutput)) {
      out = (resp as any).LogOutput.map((l: any) => l.Output || '').join('');
    } else if (typeof resp === 'string') {
      out = resp;
    } else {
      out = JSON.stringify(resp);
    }
    
    const m = out.match(/RESULT:({.*})/);
    if (m) {
      try {
        return JSON.parse(m[1]);
      } catch (e) {
        this.log.error(`Failed to parse ${operationName} result: ${e}`);
      }
    }
    
    // Check for common error patterns
    if (out.includes('ModuleNotFoundError')) {
      return { success: false, error: 'Sequencer module not available. Ensure Sequencer is enabled.' };
    }
    if (out.includes('AttributeError')) {
      return { success: false, error: 'Sequencer API method not found. Check Unreal Engine version compatibility.' };
    }
    
    return { success: false, error: `${operationName} did not return a valid result: ${out.substring(0, 200)}` };
  }

  async create(params: { name: string; path?: string }) {
    const name = params.name?.trim();
    const base = (params.path || '/Game/Sequences').replace(/\/$/, '');
    if (!name) return { success: false, error: 'name is required' };
    const py = `\nimport unreal, json\nname = r"${name}"\nbase = r"${base}"\nfull = f"{base}/{name}"\ntry:\n    # Ensure directory exists\n    try:\n        if not unreal.EditorAssetLibrary.does_directory_exist(base):\n            unreal.EditorAssetLibrary.make_directory(base)\n    except Exception:\n        pass\n\n    if unreal.EditorAssetLibrary.does_asset_exist(full):\n        print('RESULT:' + json.dumps({'success': True, 'sequencePath': full, 'existing': True}))\n    else:\n        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()\n        factory = unreal.LevelSequenceFactoryNew()\n        seq = asset_tools.create_asset(asset_name=name, package_path=base, asset_class=unreal.LevelSequence, factory=factory)\n        if seq:\n            unreal.EditorAssetLibrary.save_asset(full)\n            print('RESULT:' + json.dumps({'success': True, 'sequencePath': full}))\n        else:\n            print('RESULT:' + json.dumps({'success': False, 'error': 'Create returned None'}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'createSequence'
    );
    
    const result = this.parsePythonResult(resp, 'createSequence');
    
    // Cache the sequence if successful
    if (result.success && result.sequencePath) {
      const sequence: LevelSequence = {
        path: result.sequencePath,
        name: name
      };
      this.sequenceCache.set(sequence.path, sequence);
    }
    
    return result;
  }

  async open(params: { path: string }) {
    const py = `\nimport unreal, json\npath = r"${params.path}"\ntry:\n    seq = unreal.load_asset(path)\n    if not seq:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'Sequence not found'}))\n    else:\n        unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(seq)\n        print('RESULT:' + json.dumps({'success': True, 'sequencePath': path}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'openSequence'
    );
    
    return this.parsePythonResult(resp, 'openSequence');
  }

  async addCamera(params: { spawnable?: boolean }) {
    const py = `\nimport unreal, json\ntry:\n    ls = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)\n    if not ls:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'LevelSequenceEditorSubsystem unavailable'}))\n    else:\n        cam = ls.create_camera(spawnable=${params.spawnable !== false ? 'True' : 'False'})\n        print('RESULT:' + json.dumps({'success': True, 'cameraBindingId': str(cam.get_binding_id()) if hasattr(cam, 'get_binding_id') else ''}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'addCamera'
    );
    
    return this.parsePythonResult(resp, 'addCamera');
  }

  async addActor(params: { actorName: string }) {
    const py = `
import unreal, json
try:
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ls = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)
    if not ls or not actor_sub:
        print('RESULT:' + json.dumps({'success': False, 'error': 'Subsystem unavailable'}))
    else:
        target = None
        actors = actor_sub.get_all_level_actors()
        for a in actors:
            if not a: continue
            label = a.get_actor_label()
            name = a.get_name()
            # Check label, name, and partial matches
            if label == r"${params.actorName}" or name == r"${params.actorName}" or label.startswith(r"${params.actorName}"):
                target = a
                break
        
        if not target:
            # Try to find any actors to debug
            actor_info = []
            for a in actors[:5]:
                if a:
                    actor_info.append({'label': a.get_actor_label(), 'name': a.get_name()})
            print('RESULT:' + json.dumps({'success': False, 'error': f'Actor "${params.actorName}" not found. Sample actors: {actor_info}'}))
        else:
            # Make sure we have a focused sequence
            seq = unreal.LevelSequenceEditorBlueprintLibrary.get_focused_level_sequence()
            if seq:
                bindings = ls.add_actors([target])
                print('RESULT:' + json.dumps({'success': True, 'count': len(bindings), 'actorAdded': target.get_actor_label()}))
            else:
                print('RESULT:' + json.dumps({'success': False, 'error': 'No sequence is currently focused'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'addActor'
    );
    
    return this.parsePythonResult(resp, 'addActor');
  }

  /**
   * Play the current level sequence
   */
  async play(params?: { startTime?: number; loopMode?: 'once' | 'loop' | 'pingpong' }) {
    const py = `
import unreal, json
try:
    unreal.LevelSequenceEditorBlueprintLibrary.play()
    ${params?.loopMode ? `unreal.LevelSequenceEditorBlueprintLibrary.set_loop_mode('${params.loopMode}')` : ''}
    print('RESULT:' + json.dumps({'success': True, 'playing': True}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'playSequence'
    );
    
    return this.parsePythonResult(resp, 'playSequence');
  }

  /**
   * Pause the current level sequence
   */
  async pause() {
    const py = `
import unreal, json
try:
    unreal.LevelSequenceEditorBlueprintLibrary.pause()
    print('RESULT:' + json.dumps({'success': True, 'paused': True}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'pauseSequence'
    );
    
    return this.parsePythonResult(resp, 'pauseSequence');
  }

  /**
   * Stop/close the current level sequence
   */
  async stop() {
    const py = `
import unreal, json
try:
    unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()
    print('RESULT:' + json.dumps({'success': True, 'stopped': True}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'stopSequence'
    );
    
    return this.parsePythonResult(resp, 'stopSequence');
  }

  /**
   * Set sequence properties including frame rate and length
   */
  async setSequenceProperties(params: { 
    path?: string;
    frameRate?: number;
    lengthInFrames?: number;
    playbackStart?: number;
    playbackEnd?: number;
  }) {
    const py = `
import unreal, json
try:
    # Load the sequence
    seq_path = r"${params.path || ''}"
    if seq_path:
        seq = unreal.load_asset(seq_path)
    else:
        # Try to get the currently open sequence
        seq = unreal.LevelSequenceEditorBlueprintLibrary.get_focused_level_sequence()
    
    if not seq:
        print('RESULT:' + json.dumps({'success': False, 'error': 'No sequence found or loaded'}))
    else:
        result = {'success': True, 'changes': []}
        
        # Set frame rate if provided
        ${params.frameRate ? `
        frame_rate = unreal.FrameRate(numerator=${params.frameRate}, denominator=1)
        unreal.MovieSceneSequenceExtensions.set_display_rate(seq, frame_rate)
        result['changes'].append({'property': 'frameRate', 'value': ${params.frameRate}})
        ` : ''}
        
        # Set playback range if provided
        ${(params.playbackStart !== undefined || params.playbackEnd !== undefined) ? `
        current_range = unreal.MovieSceneSequenceExtensions.get_playback_range(seq)
        start = ${params.playbackStart !== undefined ? params.playbackStart : 'current_range.get_start_frame()'}
        end = ${params.playbackEnd !== undefined ? params.playbackEnd : 'current_range.get_end_frame()'}
        # Use set_playback_start and set_playback_end instead
        if ${params.playbackStart !== undefined}:
            unreal.MovieSceneSequenceExtensions.set_playback_start(seq, ${params.playbackStart})
        if ${params.playbackEnd !== undefined}:
            unreal.MovieSceneSequenceExtensions.set_playback_end(seq, ${params.playbackEnd})
        result['changes'].append({'property': 'playbackRange', 'start': start, 'end': end})
        ` : ''}
        
        # Set total length in frames if provided
        ${params.lengthInFrames ? `
        # This sets the playback end to match the desired length
        start = unreal.MovieSceneSequenceExtensions.get_playback_start(seq)
        end = start + ${params.lengthInFrames}
        unreal.MovieSceneSequenceExtensions.set_playback_end(seq, end)
        result['changes'].append({'property': 'lengthInFrames', 'value': ${params.lengthInFrames}})
        ` : ''}
        
        # Get final properties for confirmation
        final_rate = unreal.MovieSceneSequenceExtensions.get_display_rate(seq)
        final_range = unreal.MovieSceneSequenceExtensions.get_playback_range(seq)
        result['finalProperties'] = {
            'frameRate': {'numerator': final_rate.numerator, 'denominator': final_rate.denominator},
            'playbackStart': final_range.get_start_frame(),
            'playbackEnd': final_range.get_end_frame(),
            'duration': final_range.get_end_frame() - final_range.get_start_frame()
        }
        
        print('RESULT:' + json.dumps(result))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'setSequenceProperties'
    );
    
    return this.parsePythonResult(resp, 'setSequenceProperties');
  }

  /**
   * Get sequence properties
   */
  async getSequenceProperties(params: { path?: string }) {
    const py = `
import unreal, json
try:
    # Load the sequence
    seq_path = r"${params.path || ''}"
    if seq_path:
        seq = unreal.load_asset(seq_path)
    else:
        # Try to get the currently open sequence
        seq = unreal.LevelSequenceEditorBlueprintLibrary.get_focused_level_sequence()
    
    if not seq:
        print('RESULT:' + json.dumps({'success': False, 'error': 'No sequence found or loaded'}))
    else:
        # Get all properties
        display_rate = unreal.MovieSceneSequenceExtensions.get_display_rate(seq)
        playback_range = unreal.MovieSceneSequenceExtensions.get_playback_range(seq)
        
        # Get marked frames if any
        marked_frames = []
        try:
            frames = unreal.MovieSceneSequenceExtensions.get_marked_frames(seq)
            marked_frames = [{'frame': f.frame_number.value, 'label': f.label} for f in frames]
        except:
            pass
        
        result = {
            'success': True,
            'path': seq.get_path_name(),
            'name': seq.get_name(),
            'frameRate': {
                'numerator': display_rate.numerator,
                'denominator': display_rate.denominator,
                'fps': float(display_rate.numerator) / float(display_rate.denominator) if display_rate.denominator > 0 else 0
            },
            'playbackStart': playback_range.get_start_frame(),
            'playbackEnd': playback_range.get_end_frame(),
            'duration': playback_range.get_end_frame() - playback_range.get_start_frame(),
            'markedFrames': marked_frames
        }
        
        print('RESULT:' + json.dumps(result))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'getSequenceProperties'
    );
    
    return this.parsePythonResult(resp, 'getSequenceProperties');
  }

  /**
   * Set playback speed/rate
   */
  async setPlaybackSpeed(params: { speed: number }) {
    const py = `
import unreal, json
try:
    unreal.LevelSequenceEditorBlueprintLibrary.set_playback_speed(${params.speed})
    print('RESULT:' + json.dumps({'success': True, 'playbackSpeed': ${params.speed}}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'setPlaybackSpeed'
    );
    
    return this.parsePythonResult(resp, 'setPlaybackSpeed');
  }
}
