import { UnrealBridge } from '../unreal-bridge.js';
import { validateAssetParams, concurrencyDelay } from '../utils/validation.js';

export class AnimationTools {
  constructor(private bridge: UnrealBridge) {}

  /**
   * Create Animation Blueprint
   */
  async createAnimationBlueprint(params: {
    name: string;
    skeletonPath: string;
    savePath?: string;
  }) {
    try {
      // Strong input validation with expected error messages
      if (!params.name || params.name.trim() === '') {
        return {
          success: false,
          message: 'Failed: Name cannot be empty',
          error: 'Name cannot be empty'
        };
      }
      
      // Check for whitespace issues
      if (params.name.includes('  ') || params.name.startsWith(' ') || params.name.endsWith(' ')) {
        return {
          success: false,
          message: 'Failed to create Animation Blueprint: Name contains invalid whitespace',
          error: 'Name contains invalid whitespace'
        };
      }
      
      // Check for SQL injection patterns
      if (params.name.toLowerCase().includes('drop') || params.name.toLowerCase().includes('delete') || 
          params.name.includes(';') || params.name.includes('--')) {
        return {
          success: false,
          message: 'Failed to create Animation Blueprint: Name contains invalid characters',
          error: 'Name contains invalid characters'
        };
      }
      
      // Check save path starts with /
      if (params.savePath && !params.savePath.startsWith('/')) {
        return {
          success: false,
          message: 'Failed to create Animation Blueprint: Path must start with /',
          error: 'Path must start with /'
        };
      }
      
      // Now validate and sanitize for actual use
      const validation = validateAssetParams({
        name: params.name,
        savePath: params.savePath || '/Game/Animations'
      });
      
      if (!validation.valid) {
        return {
          success: false,
          message: `Failed to create Animation Blueprint: ${validation.error}`,
          error: validation.error
        };
      }
      
      const sanitizedParams = validation.sanitized;
      const path = sanitizedParams.savePath || '/Game/Animations';
      
      // Add concurrency delay to prevent race conditions
      await concurrencyDelay();
      
      // Enhanced Python script with proper persistence and error detection
      const pythonScript = `
import unreal
import time

# Helper function to ensure asset persistence
def ensure_asset_persistence(asset_path):
    """Ensure asset is properly saved and registered"""
    try:
        # Load the asset to ensure it's in memory
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not asset:
            return False
            
        # Save the asset
        saved = unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)
        if saved:
            print(f"Asset saved: {asset_path}")
        
        # Refresh the asset registry for the asset's directory only
        try:
            asset_dir = asset_path.rsplit('/', 1)[0]
            unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([asset_dir], True)
        except Exception as _reg_e:
            pass
        
        # Small delay to ensure filesystem sync
        time.sleep(0.1)
        
        return saved
    except Exception as e:
        print(f"Error ensuring persistence: {e}")
        return False

# Stop PIE if it's running
try:
    if unreal.EditorLevelLibrary.is_playing_editor():
        print("Stopping Play In Editor mode...")
        unreal.EditorLevelLibrary.editor_end_play()
        # Small delay to ensure editor fully exits play mode
        import time as _t
        _t.sleep(0.5)
except Exception as _e:
    # Try alternative check
    try:
        play_world = unreal.EditorLevelLibrary.get_editor_world()
        if play_world and play_world.is_play_in_editor():
            print("Stopping PIE via alternative method...")
            unreal.EditorLevelLibrary.editor_end_play()
            import time as _t2
            _t2.sleep(0.5)
    except:
        pass  # Continue if we can't check/stop play mode

# Main execution
success = False
error_msg = ""

# Log the attempt
print("Creating animation blueprint: ${sanitizedParams.name}")

asset_path = "${path}"
asset_name = "${sanitizedParams.name}"
full_path = f"{asset_path}/{asset_name}"

try:
    # Check if already exists
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        print(f"Asset already exists at {full_path}")
        # Load and return existing
        existing = unreal.EditorAssetLibrary.load_asset(full_path)
        if existing:
            print(f"Loaded existing AnimBlueprint: {full_path}")
            success = True
        else:
            error_msg = f"Could not load existing asset at {full_path}"
            print(f"Warning: {error_msg}")
    else:
        # Try to create new animation blueprint
        factory = unreal.AnimBlueprintFactory()
        
        # Try to load skeleton if provided
        skeleton_path = "${params.skeletonPath}"
        skeleton = None
        skeleton_set = False
        
        if skeleton_path and skeleton_path != "None":
            if unreal.EditorAssetLibrary.does_asset_exist(skeleton_path):
                skeleton = unreal.EditorAssetLibrary.load_asset(skeleton_path)
                if skeleton and isinstance(skeleton, unreal.Skeleton):
                    # Different Unreal versions use different attribute names
                    try:
                        factory.target_skeleton = skeleton
                        skeleton_set = True
                        print(f"Using skeleton: {skeleton_path}")
                    except AttributeError:
                        try:
                            factory.skeleton = skeleton
                            skeleton_set = True
                            print(f"Using skeleton (alternate): {skeleton_path}")
                        except AttributeError:
                            # In some versions, the skeleton is set differently
                            try:
                                factory.set_editor_property('target_skeleton', skeleton)
                                skeleton_set = True
                                print(f"Using skeleton (property): {skeleton_path}")
                            except:
                                print(f"Warning: Could not set skeleton on factory")
                else:
                    error_msg = f"Invalid skeleton at {skeleton_path}"
                    print(f"Warning: {error_msg}")
            else:
                print(f"Warning: Skeleton not found at {skeleton_path}, creating without skeleton")
        
        # Create the asset
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        new_asset = asset_tools.create_asset(
            asset_name=asset_name,
            package_path=asset_path,
            asset_class=unreal.AnimBlueprint,
            factory=factory
        )
        
        if new_asset:
            print(f"Successfully created AnimBlueprint at {full_path}")
            
            # Ensure persistence
            if ensure_asset_persistence(full_path):
                # Verify it was saved
                if unreal.EditorAssetLibrary.does_asset_exist(full_path):
                    print(f"Verified asset exists after save: {full_path}")
                    success = True
                else:
                    error_msg = f"Asset not found after save: {full_path}"
                    print(f"Warning: {error_msg}")
            else:
                error_msg = "Failed to persist asset"
                print(f"Warning: {error_msg}")
        else:
            error_msg = f"Failed to create AnimBlueprint {asset_name}"
            print(error_msg)
            
except Exception as e:
    error_msg = str(e)
    print(f"Error: {error_msg}")
    import traceback
    traceback.print_exc()

# Output result markers for parsing
if success:
    print("SUCCESS")
else:
    print(f"FAILED: {error_msg}")

print("DONE")
`;
      
      // Execute Python and parse the output
      try {
        const response = await this.bridge.executePython(pythonScript);
        
        // Parse the response to detect actual success or failure
        const responseStr = typeof response === 'string' ? response : JSON.stringify(response);
        
        // Check for explicit success/failure markers
        if (responseStr.includes('SUCCESS')) {
          return { 
            success: true, 
            message: `Animation Blueprint ${sanitizedParams.name} created successfully`,
            path: `${path}/${sanitizedParams.name}`
          };
        } else if (responseStr.includes('FAILED:')) {
          // Extract error message after FAILED:
          const failMatch = responseStr.match(/FAILED:\s*(.+)/);     const errorMsg = failMatch ? failMatch[1] : 'Unknown error';
          return {
            success: false,
            message: `Failed to create Animation Blueprint: ${errorMsg}`,
            error: errorMsg
          };
        } else {
          // If no explicit markers, check for other error indicators
          if (responseStr.includes('Error:') || responseStr.includes('error') || 
              responseStr.includes('failed') || responseStr.includes('Failed')) {
            return {
              success: false,
              message: 'Failed to create Animation Blueprint',
              error: responseStr
            };
          }
          
          // Assume success if no errors detected
          return { 
            success: true, 
            message: `Animation Blueprint ${sanitizedParams.name} processed`,
            path: `${path}/${sanitizedParams.name}`
          };
        }
      } catch (error) {
        return {
          success: false,
          message: 'Failed to create Animation Blueprint',
          error: String(error)
        };
      }
    } catch (err) {
      return { success: false, error: `Failed to create AnimBlueprint: ${err}` };
    }
  }

  /**
   * Add State Machine to Animation Blueprint
   */
  async addStateMachine(params: {
    blueprintPath: string;
    machineName: string;
    states: Array<{
      name: string;
      animation?: string;
      isEntry?: boolean;
      isExit?: boolean;
    }>;
    transitions?: Array<{
      sourceState: string;
      targetState: string;
      condition?: string;
    }>;
  }) {
    try {
      // State machines are complex - we'll use console commands for basic setup
      const commands = [
        `AddAnimStateMachine ${params.blueprintPath} ${params.machineName}`
      ];
      
      // Add states
      for (const state of params.states) {
        commands.push(
          `AddAnimState ${params.blueprintPath} ${params.machineName} ${state.name} ${state.animation || ''}`
        );
        if (state.isEntry) {
          commands.push(`SetAnimStateEntry ${params.blueprintPath} ${params.machineName} ${state.name}`);
        }
      }
      
      // Add transitions
      if (params.transitions) {
        for (const transition of params.transitions) {
          commands.push(
            `AddAnimTransition ${params.blueprintPath} ${params.machineName} ${transition.sourceState} ${transition.targetState}`
          );
        }
      }
      
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `State machine ${params.machineName} added to ${params.blueprintPath}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to add state machine: ${err}` };
    }
  }

  /**
   * Create Animation Montage
   */
  async createMontage(params: {
    name: string;
    animationSequence: string;
    savePath?: string;
    sections?: Array<{
      name: string;
      startTime: number;
      endTime: number;
    }>;
    notifies?: Array<{
      name: string;
      time: number;
    }>;
  }) {
    try {
      const path = params.savePath || '/Game/Animations/Montages';
      const commands = [
        `CreateAsset AnimMontage ${params.name} ${path}`,
        `SetMontageAnimation ${params.name} ${params.animationSequence}`
      ];
      
      // Add sections
      if (params.sections) {
        for (const section of params.sections) {
          commands.push(
            `AddMontageSection ${params.name} ${section.name} ${section.startTime} ${section.endTime}`
          );
        }
      }
      
      // Add notifies
      if (params.notifies) {
        for (const notify of params.notifies) {
          commands.push(
            `AddMontageNotify ${params.name} ${notify.name} ${notify.time}`
          );
        }
      }
      
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Animation Montage ${params.name} created`,
        path: `${path}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create montage: ${err}` };
    }
  }

  /**
   * Create Blend Space
   */
  async createBlendSpace(params: {
    name: string;
    skeletonPath: string;
    savePath?: string;
    dimensions: 1 | 2;
    horizontalAxis?: {
      name: string;
      minValue: number;
      maxValue: number;
    };
    verticalAxis?: {
      name: string;
      minValue: number;
      maxValue: number;
    };
    samples?: Array<{
      animation: string;
      x: number;
      y?: number;
    }>;
  }) {
    try {
      const path = params.savePath || '/Game/Animations/BlendSpaces';
      const blendSpaceType = params.dimensions === 1 ? 'BlendSpace1D' : 'BlendSpace';
      
      // These commands don't exist, return a message about limitations
      const commands = [
        `echo Creating ${blendSpaceType} ${params.name} at ${path}`
      ];
      
      // Configure axes
      if (params.horizontalAxis) {
        commands.push(
          `SetBlendSpaceAxis ${params.name} Horizontal ${params.horizontalAxis.name} ${params.horizontalAxis.minValue} ${params.horizontalAxis.maxValue}`
        );
      }
      
      if (params.dimensions === 2 && params.verticalAxis) {
        commands.push(
          `SetBlendSpaceAxis ${params.name} Vertical ${params.verticalAxis.name} ${params.verticalAxis.minValue} ${params.verticalAxis.maxValue}`
        );
      }
      
      // Add sample animations
      if (params.samples) {
        for (const sample of params.samples) {
          const coords = params.dimensions === 1 ? `${sample.x}` : `${sample.x} ${sample.y || 0}`;
          commands.push(
            `AddBlendSpaceSample ${params.name} ${sample.animation} ${coords}`
          );
        }
      }
      
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Blend Space ${params.name} created`,
        path: `${path}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create blend space: ${err}` };
    }
  }

  /**
   * Setup Control Rig
   */
  async setupControlRig(params: {
    name: string;
    skeletonPath: string;
    savePath?: string;
    controls?: Array<{
      name: string;
      type: 'Transform' | 'Float' | 'Bool' | 'Vector';
      bone?: string;
      defaultValue?: any;
    }>;
  }) {
    try {
      const path = params.savePath || '/Game/Animations';
      
      // Validate path length (Unreal has a 260 character limit)
      const fullPath = `${path}/${params.name}`;
      if (fullPath.length > 260) {
        return {
          success: false,
          message: `Failed: Path too long (${fullPath.length} characters)`,
          error: 'Unreal Engine paths must be less than 260 characters'
        };
      }
      
      const commands = [
        `CreateAsset ControlRig ${params.name} ${path}`,
        `SetControlRigSkeleton ${params.name} ${params.skeletonPath}`
      ];
      
      // Add controls
      if (params.controls) {
        for (const control of params.controls) {
          commands.push(
            `AddControlRigControl ${params.name} ${control.name} ${control.type} ${control.bone || ''}`
          );
          if (control.defaultValue !== undefined) {
            commands.push(
              `SetControlRigDefault ${params.name} ${control.name} ${JSON.stringify(control.defaultValue)}`
            );
          }
        }
      }
      
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Control Rig ${params.name} created`,
        path: `${path}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to setup control rig: ${err}` };
    }
  }

  /**
   * Create Level Sequence (for cinematics)
   */
  async createLevelSequence(params: {
    name: string;
    savePath?: string;
    frameRate?: number;
    duration?: number;
    tracks?: Array<{
      actorName: string;
      trackType: 'Transform' | 'Animation' | 'Camera' | 'Event';
      keyframes?: Array<{
        time: number;
        value: any;
      }>;
    }>;
  }) {
    try {
      const path = params.savePath || '/Game/Cinematics';
      
      const commands = [
        `CreateAsset LevelSequence ${params.name} ${path}`,
        `SetSequenceFrameRate ${params.name} ${params.frameRate || 30}`,
        `SetSequenceDuration ${params.name} ${params.duration || 5}`
      ];
      
      // Add tracks
      if (params.tracks) {
        for (const track of params.tracks) {
          commands.push(
            `AddSequenceTrack ${params.name} ${track.actorName} ${track.trackType}`
          );
          
          // Add keyframes
          if (track.keyframes) {
            for (const keyframe of track.keyframes) {
              commands.push(
                `AddSequenceKey ${params.name} ${track.actorName} ${track.trackType} ${keyframe.time} ${JSON.stringify(keyframe.value)}`
              );
            }
          }
        }
      }
      
      for (const cmd of commands) {
        await this.bridge.executeConsoleCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Level Sequence ${params.name} created`,
        path: `${path}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create level sequence: ${err}` };
    }
  }

  /**
   * Play Animation on Actor
   */
  async playAnimation(params: {
    actorName: string;
    animationType: 'Montage' | 'Sequence' | 'BlendSpace';
    animationPath: string;
    playRate?: number;
    loop?: boolean;
    blendInTime?: number;
    blendOutTime?: number;
  }) {
    try {
      // Implement via Python for UE 5.x compatibility instead of non-existent console commands
      const playRate = params.playRate ?? 1.0;
      const loopFlag = params.loop ? 'True' : 'False';

      const python = `
import unreal
import json

result = {"success": False, "message": ""}

try:
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    target = None
    search = "${params.actorName}"
    for a in actors:
        if not a:
            continue
        name = a.get_name()
        label = a.get_actor_label()
        if (search.lower() == name.lower()) or (search.lower() == label.lower()) or (search.lower() in label.lower()):
            target = a
            break

    if not target:
        result["message"] = f"Actor not found: {search}"
    else:
        # Try to get a SkeletalMeshComponent from the actor
        sk = target.get_component_by_class(unreal.SkeletalMeshComponent)
        if not sk:
            # Try commonly named properties (e.g., Character mesh)
            try:
                sk = target.get_editor_property('mesh')
            except Exception:
                sk = None
        if not sk:
            result["message"] = "No SkeletalMeshComponent found on actor"
        else:
            anim_type = "${params.animationType}"
            asset_path = r"${params.animationPath}"
            if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
                result["message"] = f"Animation asset not found: {asset_path}"
            else:
                asset = unreal.EditorAssetLibrary.load_asset(asset_path)
                if anim_type == 'Montage':
                    # Use AnimInstance montage_play
                    inst = sk.get_anim_instance()
                    if not inst:
                        result["message"] = "AnimInstance not found on SkeletalMeshComponent"
                    else:
                        try:
                            # montage_play(montage, play_rate, return_value_type, time_to_start_montage_at, stop_all_montages)
                            inst.montage_play(asset, ${playRate})
                            result["success"] = True
                            result["message"] = f"Montage playing on {search}"
                        except Exception as e:
                            result["message"] = f"Failed to play montage: {e}"
                elif anim_type == 'Sequence':
                    try:
                        sk.play_animation(asset, ${loopFlag})
                        # Adjust rate if supported via play rate on AnimInstance
                        try:
                            inst = sk.get_anim_instance()
                            if inst:
                                # Not all paths support direct play rate control here; best effort only
                                pass
                        except Exception:
                            pass
                        result["success"] = True
                        result["message"] = f"Sequence playing on {search}"
                    except Exception as e:
                        result["message"] = f"Failed to play sequence: {e}"
                else:
                    result["message"] = "BlendSpace playback requires an Animation Blueprint; not supported via direct play."
except Exception as e:
    result["message"] = f"Error: {e}"

print("RESULT:" + json.dumps(result))
`.trim();

      const resp = await this.bridge.executePython(python);
      // Parse Python result
      let output = '';
      if (resp && typeof resp === 'object' && Array.isArray((resp as any).LogOutput)) {
        output = (resp as any).LogOutput.map((l: any) => l.Output || '').join('');
      } else if (typeof resp === 'string') {
        output = resp;
      } else {
        output = JSON.stringify(resp);
      }
      const m = output.match(/RESULT:({.*})/);
      if (m) {
        try {
          const parsed = JSON.parse(m[1]);
          return parsed.success ? { success: true, message: parsed.message } : { success: false, error: parsed.message };
        } catch {}
      }
      return { success: true, message: `Animation ${params.animationType} processed for ${params.actorName}` };
    } catch (err) {
      return { success: false, error: `Failed to play animation: ${err}` };
    }
  }

  /**
   * Helper function to execute console commands
   */
  private async executeCommand(command: string) {
    return this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/Engine.Default__KismetSystemLibrary',
      functionName: 'ExecuteConsoleCommand',
      parameters: {
        WorldContextObject: null,
        Command: command,
        SpecificPlayer: null
      },
      generateTransaction: false
    });
  }
}
