// src/tools/catalog/capabilities/records/shared/light-text.ts
// What the light-creating records say about their parameters, once.
//
// build_environment.create_light and manage_level.create_light run the same handler
// (McpLightingHandlers::HandleSpawnLight), so a caller must read the same facts on either. The `properties` keys
// below are exactly the ones ApplyLightProperties reads; tests/unit/plugin/light_create_contracts.test.ts fails when
// the C++ and this list disagree.

/** Every key of a light's `properties` object that the C++ reads. */
export const LIGHT_PROPERTY_KEYS = [
  'intensity',
  'color',
  'castShadows',
  'useAsAtmosphereSunLight',
  'attenuationRadius',
  'innerConeAngle',
  'outerConeAngle',
  'sourceWidth',
  'sourceHeight',
] as const;

/** What build_environment.create_light says it does, in search and in describe. */
export const LIGHT_SUMMARY =
  'Create a light actor: point, spot, directional, rect or sky (or any light class), with intensity, color and, in properties, shadow, radius and cone settings.';

export const LIGHT_WHEN_TO_USE: readonly string[] = [
  'A point, spot, directional, rect or sky light must be added to the level being edited, with its intensity, color or shadow settings.',
];

// Three records create lights: this one and manage_level.create_light run one handler, and
// manage_effect.create_dynamic_light is a separate, smaller one that can spawn into a running game.
export const LIGHT_WHEN_NOT_TO_USE: readonly string[] = [
  'An existing light should be reconfigured: control_actor.edit_component sets its component values.',
  'The light must exist in the running PIE world: manage_effect.create_dynamic_light spawns into the active world, this one into the level being edited.',
  'manage_level.create_light runs this same handler; use this one, which documents every property.',
  'The light is always created Movable; a static or stationary light needs its mobility changed afterwards with control_actor.edit_component.',
];

export const LIGHT_TYPE_TEXT =
  'Light kind: point, spot, directional, rect or sky (any case), or the class name PointLight, SpotLight, DirectionalLight, RectLight or SkyLight. A light class of your own goes in lightClass.';

export const LIGHT_CLASS_TEXT =
  'Light class name or path, for a class lightType does not name, including a Blueprint light (/Game/Lights/BP_Lamp.BP_Lamp_C); build_environment list_light_types lists the loaded ones. Wins over lightType.';

export const LIGHT_LOCATION_TEXT = 'World location {x, y, z}; 0, 0, 300 when omitted.';

/** A spot or directional light shines along its forward axis. */
export const LIGHT_AIM_TEXT = 'A spot or directional light shines along its forward axis, so pitch -90 points it straight down.';

export const LIGHT_INTENSITY_TEXT =
  "Brightness: the light component's Intensity, set as given, in the component's own units (a directional light is in lux). Left out, the engine default stays. properties.intensity wins over it.";

/** How a color value is read, whatever shape the record takes it in. */
export const LIGHT_COLOR_TEXT =
  'Linear 0-1 channels, stored by the engine as the 8-bit sRGB color: {r: 1, g: 0.7, b: 0.42} reads back as (255, 218, 173). A missing channel reads 0 (alpha 1).';

/** The same, for a handler whose `properties` object can carry a color of its own. */
export const LIGHT_COLOR_WITH_PROPERTIES_TEXT = `${LIGHT_COLOR_TEXT} properties.color wins over it.`;

export const LIGHT_PROPERTIES_TEXT =
  'Light component settings, applied to the lights they fit; the reply lists what went in as details.applied, what did not as details.refused ("name: why") and any value replaced for being out of range as details.adjusted. '
  + 'Keys: intensity (0 or more; a negative reads 0), '
  + 'color (as above), castShadows (true or false), '
  + 'useAsAtmosphereSunLight (true or false; a directional light given properties is the atmosphere sun unless this says false), '
  + 'attenuationRadius (above 0, point and spot lights; an invalid value reads 1000), '
  + 'innerConeAngle and outerConeAngle (0 to 180 degrees, spot lights; clamped), '
  + 'sourceWidth and sourceHeight (above 0, rect lights; an invalid value reads 100). '
  + 'A sky light takes only the top-level intensity and color. Any other key, or one that does not fit the light, is refused by name.';
