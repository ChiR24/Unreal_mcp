/**
 * Python Utilities for Modern Unreal Engine API
 * 
 * This module provides Python code snippets that use the modern
 * Unreal Engine Python API instead of deprecated functions.
 */

export class PythonUtils {
  /**
   * Get all actors in the level using modern API
   * @returns Python code to get all level actors
   */
  static getAllLevelActors(): string {
    return `
# Use modern EditorActorSubsystem instead of deprecated EditorLevelLibrary
editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = editor_actor_subsystem.get_all_level_actors()
`.trim();
  }

  /**
   * Get selected actors using modern API
   * @returns Python code to get selected actors
   */
  static getSelectedActors(): string {
    return `
editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
selected_actors = editor_actor_subsystem.get_selected_level_actors()
`.trim();
  }

  /**
   * Spawn actor from class using modern API
   * @returns Python code to spawn actor
   */
  static spawnActorFromClass(): string {
    return `
editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
# Use spawn_actor_from_class for spawning
`.trim();
  }

  /**
   * Get a safe way to access actors with fallback
   * @returns Python code with both modern and legacy API fallback
   */
  static getSafeActorAccess(): string {
    return `
# Try modern API first, fallback to legacy if needed
try:
    editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = editor_actor_subsystem.get_all_level_actors()
except:
    # Fallback to deprecated API if modern one fails
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
`.trim();
  }
}