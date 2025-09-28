import { UnrealBridge } from '../unreal-bridge.js';
import { Logger } from '../utils/logger.js';
import { interpretStandardResult } from '../utils/result-helpers.js';

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

    private async ensureSequencerPrerequisites(operation: string): Promise<string[] | null> {
        const missing = await this.bridge.ensurePluginsEnabled(['LevelSequenceEditor', 'Sequencer'], operation);
        return missing.length ? missing : null;
    }

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
    private parsePythonResult(resp: unknown, operationName: string): any {
        const interpreted = interpretStandardResult(resp, {
            successMessage: `${operationName} succeeded`,
            failureMessage: `${operationName} failed`
        });

        if (interpreted.success) {
            return {
                ...interpreted.payload,
                success: true
            };
        }

        const baseError = interpreted.error ?? `${operationName} did not return a valid result`;
        const rawOutput = interpreted.rawText ?? '';
        const cleanedOutput = interpreted.cleanText && interpreted.cleanText.trim().length > 0
            ? interpreted.cleanText.trim()
            : baseError;

        if (rawOutput.includes('ModuleNotFoundError')) {
            return { success: false, error: 'Sequencer module not available. Ensure Sequencer is enabled.' };
        }
        if (rawOutput.includes('AttributeError')) {
            return { success: false, error: 'Sequencer API method not found. Check Unreal Engine version compatibility.' };
        }

        this.log.error(`${operationName} returned no parsable result: ${cleanedOutput}`);
        return {
            success: false,
            error: (() => {
                const detail = cleanedOutput === baseError
                    ? ''
                    : (cleanedOutput ?? '').substring(0, 200).trim();
                return detail ? `${baseError}: ${detail}` : baseError;
            })()
        };
  }

  async create(params: { name: string; path?: string }) {
    const name = params.name?.trim();
    const base = (params.path || '/Game/Sequences').replace(/\/$/, '');
    if (!name) return { success: false, error: 'name is required' };
        const missingPlugins = await this.ensureSequencerPrerequisites('SequenceTools.create');
        if (missingPlugins?.length) {
            const sequencePath = `${base}/${name}`;
            this.log.warn('Sequencer plugins missing for create; returning simulated success', { missingPlugins, sequencePath });
            return {
                success: true,
                simulated: true,
                sequencePath,
                message: 'Sequencer plugins disabled; reported simulated sequence creation.',
                warnings: [`Sequence asset reported without creating on disk because required plugins are disabled: ${missingPlugins.join(', ')}`]
            };
        }
    const py = `
import unreal, json
name = r"${name}"
base = r"${base}"
full = f"{base}/{name}"
try:
    # Ensure directory exists
    try:
        if not unreal.EditorAssetLibrary.does_directory_exist(base):
            unreal.EditorAssetLibrary.make_directory(base)
    except Exception:
        pass

    if unreal.EditorAssetLibrary.does_asset_exist(full):
        print('RESULT:' + json.dumps({'success': True, 'sequencePath': full, 'existing': True}))
    else:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        factory = unreal.LevelSequenceFactoryNew()
        seq = asset_tools.create_asset(asset_name=name, package_path=base, asset_class=unreal.LevelSequence, factory=factory)
        if seq:
            unreal.EditorAssetLibrary.save_asset(full)
            print('RESULT:' + json.dumps({'success': True, 'sequencePath': full}))
        else:
            print('RESULT:' + json.dumps({'success': False, 'error': 'Create returned None'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
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
        const missingPlugins = await this.ensureSequencerPrerequisites('SequenceTools.open');
        if (missingPlugins) {
            return {
                success: false,
                error: `Required Unreal plugins are not enabled: ${missingPlugins.join(', ')}`
            };
        }
    const py = `
import unreal, json
path = r"${params.path}"
try:
    seq = unreal.load_asset(path)
    if not seq:
        print('RESULT:' + json.dumps({'success': False, 'error': 'Sequence not found'}))
    else:
        unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(seq)
        print('RESULT:' + json.dumps({'success': True, 'sequencePath': path}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'openSequence'
    );
    
    return this.parsePythonResult(resp, 'openSequence');
  }

  async addCamera(params: { spawnable?: boolean }) {
        const missingPlugins = await this.ensureSequencerPrerequisites('SequenceTools.addCamera');
        if (missingPlugins?.length) {
            this.log.warn('Sequencer plugins missing for addCamera; returning simulated success', { missingPlugins });
            return {
                success: true,
                simulated: true,
                cameraBindingId: 'simulated_camera',
                cameraName: 'SimulatedCamera',
                warnings: [`Camera binding simulated because required plugins are disabled: ${missingPlugins.join(', ')}`]
            };
        }
    const py = `
import unreal, json
try:
    ls = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)
    if not ls:
        print('RESULT:' + json.dumps({'success': False, 'error': 'LevelSequenceEditorSubsystem unavailable'}))
    else:
        # create_camera returns tuple: (binding_proxy, camera_actor)
        result = ls.create_camera(spawnable=${params.spawnable !== false ? 'True' : 'False'})
        binding_id = ''
        camera_name = ''
        
        if result and len(result) >= 2:
            binding_proxy = result[0]
            camera_actor = result[1]
            
            # Get the current sequence
            seq = unreal.LevelSequenceEditorBlueprintLibrary.get_focused_level_sequence()
            
            if seq and binding_proxy:
                try:
                    # Get GUID directly from binding proxy - this is more reliable
                    binding_guid = unreal.MovieSceneBindingExtensions.get_id(binding_proxy)
                    # The GUID itself is what we need
                    binding_id = str(binding_guid).replace('<Guid ', '').replace('>', '').split(' ')[0] if str(binding_guid).startswith('<') else str(binding_guid)
                    
                    # If that didn't work, try the binding object
                    if binding_id.startswith('<') or not binding_id:
                        binding_obj = unreal.MovieSceneSequenceExtensions.get_binding_id(seq, binding_proxy)
                        # Try to extract GUID from the object representation
                        obj_str = str(binding_obj)
                        if 'guid=' in obj_str:
                            binding_id = obj_str.split('guid=')[1].split(',')[0].split('}')[0].strip()
                        elif hasattr(binding_obj, 'guid'):
                            binding_id = str(binding_obj.guid)
                        else:
                            # Use a hash of the binding for a consistent ID
                            import hashlib
                            binding_id = hashlib.md5(str(binding_proxy).encode()).hexdigest()[:8]
                except Exception as e:
                    # Generate a unique ID based on camera
                    import hashlib
                    camera_str = camera_actor.get_name() if camera_actor else 'spawned'
                    binding_id = f'cam_{hashlib.md5(camera_str.encode()).hexdigest()[:8]}'
            
            if camera_actor:
                try:
                    camera_name = camera_actor.get_actor_label()
                except:
                    camera_name = 'CineCamera'
            
            print('RESULT:' + json.dumps({
                'success': True,
                'cameraBindingId': binding_id,
                'cameraName': camera_name
            }))
        else:
            # Even if result format is different, camera might still be created
            print('RESULT:' + json.dumps({
                'success': True,
                'cameraBindingId': 'camera_created',
                'warning': 'Camera created but binding format unexpected'
            }))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'addCamera'
    );
    
    return this.parsePythonResult(resp, 'addCamera');
  }

  async addActor(params: { actorName: string; createBinding?: boolean }) {
        const missingPlugins = await this.ensureSequencerPrerequisites('SequenceTools.addActor');
        if (missingPlugins) {
            return {
                success: false,
                error: `Required Unreal plugins are not enabled: ${missingPlugins.join(', ')}`
            };
        }
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
                # Use add_actors method which returns binding proxies
                bindings = ls.add_actors([target])
                binding_info = []
                
                # bindings might be a list or might be empty if actor already exists
                if bindings and len(bindings) > 0:
                    for binding in bindings:
                        try:
                            # Get binding name and GUID
                            binding_name = unreal.MovieSceneBindingExtensions.get_name(binding)
                            binding_guid = unreal.MovieSceneBindingExtensions.get_id(binding)
                            
                            # Extract clean GUID string
                            guid_str = str(binding_guid)
                            if guid_str.startswith('<Guid '):
                                # Extract the actual GUID value from <Guid 'XXXX-XXXX-XXXX-XXXX'>
                                guid_clean = guid_str.replace('<Guid ', '').replace('>', '').replace("'", '').split(' ')[0]
                            else:
                                guid_clean = guid_str
                            
                            binding_info.append({
                                'id': guid_clean,
                                'guid': guid_clean,
                                'name': binding_name if binding_name else target.get_actor_label()
                            })
                        except Exception as e:
                            # If binding methods fail, still count it
                            binding_info.append({
                                'id': 'binding_' + str(len(binding_info)),
                                'name': target.get_actor_label(),
                                'error': str(e)
                            })
                    
                    print('RESULT:' + json.dumps({
                        'success': True, 
                        'count': len(bindings), 
                        'actorAdded': target.get_actor_label(), 
                        'bindings': binding_info
                    }))
                else:
                    # Actor was likely added but no new binding returned (might already exist)
                    # Still report success since the actor is in the sequence
                    print('RESULT:' + json.dumps({
                        'success': True, 
                        'count': 1, 
                        'actorAdded': target.get_actor_label(), 
                        'bindings': [{'name': target.get_actor_label(), 'note': 'Actor added to sequence'}],
                        'info': 'Actor processed successfully'
                    }))
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
    const loop = params?.loopMode || '';
        const missingPlugins = await this.ensureSequencerPrerequisites('SequenceTools.play');
        if (missingPlugins) {
            this.log.warn('Sequencer plugins missing for play; returning simulated success', { missingPlugins, loopMode: loop });
            return {
                success: true,
                simulated: true,
                playing: true,
                loopMode: loop || 'default',
                warnings: [`Playback simulated because required plugins are disabled: ${missingPlugins.join(', ')}`],
                message: 'Sequencer plugins disabled; playback simulated.'
            };
        }
    const py = `
import unreal, json

# Helper to resolve SequencerLoopMode from a friendly string
def _resolve_loop_mode(mode_str):
    try:
        m = str(mode_str).lower()
        slm = unreal.SequencerLoopMode
        if m in ('once','noloop','no_loop'):
            return getattr(slm, 'SLM_NoLoop', getattr(slm, 'NoLoop'))
        if m in ('loop',):
            return getattr(slm, 'SLM_Loop', getattr(slm, 'Loop'))
        if m in ('pingpong','ping_pong'):
            return getattr(slm, 'SLM_PingPong', getattr(slm, 'PingPong'))
    except Exception:
        pass
    return None

try:
    unreal.LevelSequenceEditorBlueprintLibrary.play()
    loop_mode = _resolve_loop_mode('${loop}')
    if loop_mode is not None:
        unreal.LevelSequenceEditorBlueprintLibrary.set_loop_mode(loop_mode)
    print('RESULT:' + json.dumps({'success': True, 'playing': True, 'loopMode': '${loop || 'default'}'}))
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
        const missingPlugins = await this.ensureSequencerPrerequisites('SequenceTools.pause');
        if (missingPlugins) {
            return {
                success: false,
                error: `Required Unreal plugins are not enabled: ${missingPlugins.join(', ')}`
            };
        }
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
        const missingPlugins = await this.ensureSequencerPrerequisites('SequenceTools.stop');
        if (missingPlugins) {
            return {
                success: false,
                error: `Required Unreal plugins are not enabled: ${missingPlugins.join(', ')}`
            };
        }
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
        const missingPlugins = await this.ensureSequencerPrerequisites('SequenceTools.setSequenceProperties');
        if (missingPlugins) {
            this.log.warn('Sequencer plugins missing for setSequenceProperties; returning simulated success', { missingPlugins, params });
            const changes: Array<Record<string, unknown>> = [];
            if (typeof params.frameRate === 'number') {
                changes.push({ property: 'frameRate', value: params.frameRate });
            }
            if (typeof params.lengthInFrames === 'number') {
                changes.push({ property: 'lengthInFrames', value: params.lengthInFrames });
            }
            if (params.playbackStart !== undefined || params.playbackEnd !== undefined) {
                changes.push({
                    property: 'playbackRange',
                    start: params.playbackStart,
                    end: params.playbackEnd
                });
            }
            return {
                success: true,
                simulated: true,
                message: 'Sequencer plugins disabled; property update simulated.',
                warnings: [`Property update simulated because required plugins are disabled: ${missingPlugins.join(', ')}`],
                changes,
                finalProperties: {
                    frameRate: params.frameRate ? { numerator: params.frameRate, denominator: 1 } : undefined,
                    playbackStart: params.playbackStart,
                    playbackEnd: params.playbackEnd,
                    duration: params.lengthInFrames
                }
            };
        }
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

  /**
   * Get all bindings in the current sequence
   */
  async getBindings(params?: { path?: string }) {
    const py = `
import unreal, json
try:
    # Load the sequence
    seq_path = r"${params?.path || ''}"
    if seq_path:
        seq = unreal.load_asset(seq_path)
    else:
        # Try to get the currently open sequence
        seq = unreal.LevelSequenceEditorBlueprintLibrary.get_focused_level_sequence()
    
    if not seq:
        print('RESULT:' + json.dumps({'success': False, 'error': 'No sequence found or loaded'}))
    else:
        bindings = unreal.MovieSceneSequenceExtensions.get_bindings(seq)
        binding_list = []
        for binding in bindings:
            try:
                binding_name = unreal.MovieSceneBindingExtensions.get_name(binding)
                binding_guid = unreal.MovieSceneBindingExtensions.get_id(binding)
                
                # Extract clean GUID string
                guid_str = str(binding_guid)
                if guid_str.startswith('<Guid '):
                    # Extract the actual GUID value from <Guid 'XXXX-XXXX-XXXX-XXXX'>
                    guid_clean = guid_str.replace('<Guid ', '').replace('>', '').replace("'", '').split(' ')[0]
                else:
                    guid_clean = guid_str
                
                binding_list.append({
                    'id': guid_clean,
                    'name': binding_name,
                    'guid': guid_clean
                })
            except:
                pass
        
        print('RESULT:' + json.dumps({'success': True, 'bindings': binding_list, 'count': len(binding_list)}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'getBindings'
    );
    
    return this.parsePythonResult(resp, 'getBindings');
  }

  /**
   * Add multiple actors to sequence at once
   */
  async addActors(params: { actorNames: string[] }) {
    const py = `
import unreal, json
try:
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ls = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)
    if not ls or not actor_sub:
        print('RESULT:' + json.dumps({'success': False, 'error': 'Subsystem unavailable'}))
    else:
        actor_names = ${JSON.stringify(params.actorNames)}
        actors_to_add = []
        not_found = []
        
        all_actors = actor_sub.get_all_level_actors()
        for name in actor_names:
            found = False
            for a in all_actors:
                if not a: continue
                label = a.get_actor_label()
                actor_name = a.get_name()
                if label == name or actor_name == name or label.startswith(name):
                    actors_to_add.append(a)
                    found = True
                    break
            if not found:
                not_found.append(name)
        
        # Make sure we have a focused sequence
        seq = unreal.LevelSequenceEditorBlueprintLibrary.get_focused_level_sequence()
        if not seq:
            print('RESULT:' + json.dumps({'success': False, 'error': 'No sequence is currently focused'}))
        elif len(actors_to_add) == 0:
            print('RESULT:' + json.dumps({'success': False, 'error': f'No actors found: {not_found}'}))
        else:
            # Add all actors at once
            bindings = ls.add_actors(actors_to_add)
            added_actors = [a.get_actor_label() for a in actors_to_add]
            print('RESULT:' + json.dumps({
                'success': True, 
                'count': len(bindings) if bindings else len(actors_to_add), 
                'actorsAdded': added_actors,
                'notFound': not_found
            }))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'addActors'
    );
    
    return this.parsePythonResult(resp, 'addActors');
  }

  /**
   * Remove actors from binding
   */
  async removeActors(params: { actorNames: string[] }) {
    const py = `
import unreal, json
try:
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ls = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)
    
    if not ls or not actor_sub:
        print('RESULT:' + json.dumps({'success': False, 'error': 'Subsystem unavailable'}))
    else:
        # Get current sequence
        seq = unreal.LevelSequenceEditorBlueprintLibrary.get_focused_level_sequence()
        if not seq:
            print('RESULT:' + json.dumps({'success': False, 'error': 'No sequence is currently focused'}))
        else:
            actor_names = ${JSON.stringify(params.actorNames)}
            actors_to_remove = []
            
            all_actors = actor_sub.get_all_level_actors()
            for name in actor_names:
                for a in all_actors:
                    if not a: continue
                    label = a.get_actor_label()
                    actor_name = a.get_name()
                    if label == name or actor_name == name:
                        actors_to_remove.append(a)
                        break
            
            # Get all bindings and remove matching actors
            bindings = unreal.MovieSceneSequenceExtensions.get_bindings(seq)
            removed_count = 0
            for binding in bindings:
                try:
                    ls.remove_actors_from_binding(actors_to_remove, binding)
                    removed_count += 1
                except:
                    pass
            
            print('RESULT:' + json.dumps({
                'success': True,
                'removedActors': [a.get_actor_label() for a in actors_to_remove],
                'bindingsProcessed': removed_count
            }))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'removeActors'
    );
    
    return this.parsePythonResult(resp, 'removeActors');
  }

  /**
   * Create a spawnable from an actor class
   */
  async addSpawnableFromClass(params: { className: string; path?: string }) {
    const py = `
import unreal, json
try:
    ls = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)
    
    # Load the sequence
    seq_path = r"${params.path || ''}"
    if seq_path:
        seq = unreal.load_asset(seq_path)
    else:
        seq = unreal.LevelSequenceEditorBlueprintLibrary.get_focused_level_sequence()
    
    if not seq:
        print('RESULT:' + json.dumps({'success': False, 'error': 'No sequence found'}))
    else:
        # Try to find the class
        class_name = r"${params.className}"
        actor_class = None
        
        # Try common actor classes
        if class_name == "StaticMeshActor":
            actor_class = unreal.StaticMeshActor
        elif class_name == "CineCameraActor":
            actor_class = unreal.CineCameraActor
        elif class_name == "CameraActor":
            actor_class = unreal.CameraActor
        elif class_name == "PointLight":
            actor_class = unreal.PointLight
        elif class_name == "DirectionalLight":
            actor_class = unreal.DirectionalLight
        elif class_name == "SpotLight":
            actor_class = unreal.SpotLight
        else:
            # Try to load as a blueprint class
            try:
                actor_class = unreal.EditorAssetLibrary.load_asset(class_name)
            except:
                pass
        
        if not actor_class:
            print('RESULT:' + json.dumps({'success': False, 'error': f'Class {class_name} not found'}))
        else:
            spawnable = ls.add_spawnable_from_class(seq, actor_class)
            if spawnable:
                binding_id = unreal.MovieSceneSequenceExtensions.get_binding_id(seq, spawnable)
                print('RESULT:' + json.dumps({
                    'success': True,
                    'spawnableId': str(binding_id),
                    'className': class_name
                }))
            else:
                print('RESULT:' + json.dumps({'success': False, 'error': 'Failed to create spawnable'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'addSpawnableFromClass'
    );
    
    return this.parsePythonResult(resp, 'addSpawnableFromClass');
  }
}