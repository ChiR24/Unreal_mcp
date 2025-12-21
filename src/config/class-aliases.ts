/**
 * Shared configuration for Unreal Engine class names and aliases.
 * Centralizes class mappings to avoid duplication across handlers.
 */

/**
 * User-friendly class name aliases to full Unreal class paths.
 * These allow users to specify simpler names like 'PointLight' instead of '/Script/Engine.PointLight'.
 */
export const ACTOR_CLASS_ALIASES: Record<string, string> = {
    // Special cases that need component addition
    'SplineActor': '/Script/Engine.Actor',
    'Spline': '/Script/Engine.Actor',

    // Light actors
    'PointLight': '/Script/Engine.PointLight',
    'SpotLight': '/Script/Engine.SpotLight',
    'DirectionalLight': '/Script/Engine.DirectionalLight',
    'RectLight': '/Script/Engine.RectLight',

    // Camera actors
    'Camera': '/Script/Engine.CameraActor',
    'CameraActor': '/Script/Engine.CameraActor',

    // Mesh actors
    'StaticMeshActor': '/Script/Engine.StaticMeshActor',
    'SkeletalMeshActor': '/Script/Engine.SkeletalMeshActor',

    // Gameplay actors
    'PlayerStart': '/Script/Engine.PlayerStart',
    'Pawn': '/Script/Engine.Pawn',
    'Character': '/Script/Engine.Character',
    'Actor': '/Script/Engine.Actor',

    // Trigger volumes
    'TriggerBox': '/Script/Engine.TriggerBox',
    'TriggerSphere': '/Script/Engine.TriggerSphere',
    'BlockingVolume': '/Script/Engine.BlockingVolume',
};

/**
 * Class aliases that require a component to be auto-added after spawning.
 */
export const CLASSES_REQUIRING_COMPONENT: Record<string, string> = {
    'SplineActor': 'SplineComponent',
    'Spline': 'SplineComponent',
};

/**
 * Resolve a class name alias to its full Unreal class path.
 * @param classNameOrPath - The class name or alias to resolve
 * @returns The full class path, or the original if not an alias
 */
export function resolveClassAlias(classNameOrPath: string): string {
    return ACTOR_CLASS_ALIASES[classNameOrPath] || classNameOrPath;
}

/**
 * Check if a class alias requires an auto-added component.
 * @param className - The original class name or alias
 * @returns The component name to add, or undefined
 */
export function getRequiredComponent(className: string): string | undefined {
    return CLASSES_REQUIRING_COMPONENT[className];
}
