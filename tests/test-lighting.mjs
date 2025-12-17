#!/usr/bin/env node
/**
 * Comprehensive Lighting Management Test Suite
 * Tool: manage_lighting
 * Coverage: All 14 actions with success, error, and edge cases
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
    // === PRE-CLEANUP ===
    {
        scenario: 'Pre-cleanup test actors',
        toolName: 'control_actor',
        arguments: { action: 'delete_by_tag', tag: 'TC_Lighting' },
        expected: 'success|not_found'
    },

    // === SPAWN LIGHTS (spawn_light, create_light) ===
    {
        scenario: 'Spawn Point Light',
        toolName: 'manage_lighting',
        arguments: {
            action: 'spawn_light',
            lightType: 'Point',
            name: 'TC_PointLight_1',
            location: { x: 0, y: 0, z: 200 },
            intensity: 5000,
            color: [1, 1, 1]
        },
        expected: 'success'
    },
    {
        scenario: 'Create Point Light (alias)',
        toolName: 'manage_lighting',
        arguments: {
            action: 'create_light',
            lightType: 'Point',
            name: 'TC_PointLight_2',
            location: { x: 200, y: 0, z: 200 },
            intensity: 8000,
            radius: 1000
        },
        expected: 'success'
    },
    {
        scenario: 'Spawn Directional Light',
        toolName: 'manage_lighting',
        arguments: {
            action: 'spawn_light',
            lightType: 'Directional',
            name: 'TC_DirLight',
            rotation: { pitch: -45, yaw: 0, roll: 0 },
            intensity: 3.0,
            useAsAtmosphereSunLight: true
        },
        expected: 'success'
    },
    {
        scenario: 'Spawn Spot Light',
        toolName: 'manage_lighting',
        arguments: {
            action: 'spawn_light',
            lightType: 'Spot',
            name: 'TC_SpotLight',
            location: { x: 0, y: 200, z: 300 },
            rotation: { pitch: -90, yaw: 0, roll: 0 },
            intensity: 10000,
            innerCone: 22,
            outerCone: 45
        },
        expected: 'success'
    },
    {
        scenario: 'Spawn Rect Light',
        toolName: 'manage_lighting',
        arguments: {
            action: 'spawn_light',
            lightType: 'Rect',
            name: 'TC_RectLight',
            location: { x: 100, y: 100, z: 200 },
            intensity: 15000,
            width: 100,
            height: 50
        },
        expected: 'success'
    },

    // === SKY LIGHTS (spawn_sky_light, create_sky_light, ensure_single_sky_light) ===
    {
        scenario: 'Spawn Sky Light',
        toolName: 'manage_lighting',
        arguments: {
            action: 'spawn_sky_light',
            name: 'TC_SkyLight_1',
            location: { x: 0, y: 0, z: 500 },
            intensity: 1.0,
            sourceType: 'CapturedScene'
        },
        expected: 'success'
    },
    {
        scenario: 'Create Sky Light (alias)',
        toolName: 'manage_lighting',
        arguments: {
            action: 'create_sky_light',
            name: 'TC_SkyLight_2',
            intensity: 0.8,
            recapture: true
        },
        expected: 'success'
    },
    {
        scenario: 'Ensure Single Sky Light',
        toolName: 'manage_lighting',
        arguments: {
            action: 'ensure_single_sky_light',
            intensity: 1.2
        },
        expected: 'success'
    },

    // === VOLUMES (create_lightmass_volume) ===
    {
        scenario: 'Create Lightmass Volume',
        toolName: 'manage_lighting',
        arguments: {
            action: 'create_lightmass_volume',
            name: 'TC_LightmassVolume',
            location: { x: 0, y: 0, z: 0 },
            size: { x: 2000, y: 2000, z: 1000 }
        },
        expected: 'success'
    },

    // === LEVEL SETUP (create_lighting_enabled_level, create_dynamic_light) ===
    {
        scenario: 'Create Lighting Enabled Level',
        toolName: 'manage_lighting',
        arguments: {
            action: 'create_lighting_enabled_level',
            levelName: 'TC_LightingLevel',
            useTemplate: true
        },
        expected: 'success'
    },
    {
        scenario: 'Create Dynamic Light',
        toolName: 'manage_lighting',
        arguments: {
            action: 'create_dynamic_light',
            name: 'TC_DynamicLight',
            lightType: 'Point',
            location: { x: 300, y: 300, z: 150 },
            intensity: 7000,
            castShadows: true
        },
        expected: 'success'
    },

    // === GLOBAL ILLUMINATION (setup_global_illumination) ===
    {
        scenario: 'Setup GI - Lumen',
        toolName: 'manage_lighting',
        arguments: {
            action: 'setup_global_illumination',
            method: 'LumenGI',
            bounces: 3,
            indirectLightingIntensity: 1.2
        },
        expected: 'success'
    },
    {
        scenario: 'Setup GI - Lightmass',
        toolName: 'manage_lighting',
        arguments: {
            action: 'setup_global_illumination',
            method: 'Lightmass',
            bounces: 2
        },
        expected: 'success'
    },
    {
        scenario: 'Setup GI - Screen Space',
        toolName: 'manage_lighting',
        arguments: {
            action: 'setup_global_illumination',
            method: 'ScreenSpace'
        },
        expected: 'success'
    },

    // === SHADOWS (configure_shadows) ===
    {
        scenario: 'Configure Shadows - High Quality',
        toolName: 'manage_lighting',
        arguments: {
            action: 'configure_shadows',
            shadowQuality: 'High',
            cascadedShadows: true,
            contactShadows: true,
            rayTracedShadows: false
        },
        expected: 'success'
    },
    {
        scenario: 'Configure Shadows - Ray Traced',
        toolName: 'manage_lighting',
        arguments: {
            action: 'configure_shadows',
            rayTracedShadows: true,
            shadowDistance: 5000
        },
        expected: 'success|not_supported'
    },

    // === EXPOSURE (set_exposure) ===
    {
        scenario: 'Set Exposure Auto',
        toolName: 'manage_lighting',
        arguments: {
            action: 'set_exposure',
            minBrightness: 0.5,
            maxBrightness: 2.0,
            compensationValue: 0
        },
        expected: 'success'
    },
    {
        scenario: 'Set Exposure Manual',
        toolName: 'manage_lighting',
        arguments: {
            action: 'set_exposure',
            minBrightness: 1.0,
            maxBrightness: 1.0,
            compensationValue: 1.5
        },
        expected: 'success'
    },

    // === AMBIENT OCCLUSION (set_ambient_occlusion) ===
    {
        scenario: 'Enable Ambient Occlusion',
        toolName: 'manage_lighting',
        arguments: {
            action: 'set_ambient_occlusion',
            enabled: true
        },
        expected: 'success'
    },
    {
        scenario: 'Disable Ambient Occlusion',
        toolName: 'manage_lighting',
        arguments: {
            action: 'set_ambient_occlusion',
            enabled: false
        },
        expected: 'success'
    },

    // === VOLUMETRIC FOG (setup_volumetric_fog) ===
    {
        scenario: 'Setup Volumetric Fog',
        toolName: 'manage_lighting',
        arguments: {
            action: 'setup_volumetric_fog',
            enabled: true,
            density: 0.02,
            scatteringIntensity: 0.5,
            fogHeight: 500
        },
        expected: 'success'
    },
    {
        scenario: 'Disable Volumetric Fog',
        toolName: 'manage_lighting',
        arguments: {
            action: 'setup_volumetric_fog',
            enabled: false
        },
        expected: 'success'
    },

    // === BUILD LIGHTING ===
    {
        scenario: 'Build Lighting - Preview',
        toolName: 'manage_lighting',
        arguments: {
            action: 'build_lighting',
            quality: 'Preview'
        },
        expected: 'success'
    },
    {
        scenario: 'Build Lighting - Medium',
        toolName: 'manage_lighting',
        arguments: {
            action: 'build_lighting',
            quality: 'Medium',
            buildOnlySelected: false
        },
        expected: 'success'
    },

    // === REAL-WORLD SCENARIO: Day/Night Setup ===
    {
        scenario: 'Scene Setup - Create Sun',
        toolName: 'manage_lighting',
        arguments: {
            action: 'spawn_light',
            lightType: 'Directional',
            name: 'TC_Sun',
            rotation: { pitch: -60, yaw: 45, roll: 0 },
            intensity: 5.0,
            temperature: 6500,
            useAsAtmosphereSunLight: true
        },
        expected: 'success'
    },
    {
        scenario: 'Scene Setup - Create Sky',
        toolName: 'manage_lighting',
        arguments: {
            action: 'ensure_single_sky_light',
            intensity: 1.0
        },
        expected: 'success'
    },
    {
        scenario: 'Scene Setup - Configure GI',
        toolName: 'manage_lighting',
        arguments: {
            action: 'setup_global_illumination',
            method: 'LumenGI',
            bounces: 2
        },
        expected: 'success'
    },

    // === ERROR CASES ===
    {
        scenario: 'Error: Invalid Light Type',
        toolName: 'manage_lighting',
        arguments: {
            action: 'spawn_light',
            lightType: 'InvalidLightType',
            name: 'TC_InvalidLight'
        },
        expected: 'error|unknown_type'
    },
    {
        scenario: 'Error: Invalid GI Method',
        toolName: 'manage_lighting',
        arguments: {
            action: 'setup_global_illumination',
            method: 'InvalidMethod'
        },
        expected: 'error|unknown_method'
    },
    {
        scenario: 'Error: Invalid Build Quality',
        toolName: 'manage_lighting',
        arguments: {
            action: 'build_lighting',
            quality: 'InvalidQuality'
        },
        expected: 'error|unknown_quality'
    },

    // === EDGE CASES ===
    {
        scenario: 'Edge: Zero Intensity Light',
        toolName: 'manage_lighting',
        arguments: {
            action: 'spawn_light',
            lightType: 'Point',
            name: 'TC_ZeroIntensity',
            location: { x: 0, y: 0, z: 0 },
            intensity: 0
        },
        expected: 'success'
    },
    {
        scenario: 'Edge: Negative Intensity Light',
        toolName: 'manage_lighting',
        arguments: {
            action: 'spawn_light',
            lightType: 'Point',
            name: 'TC_NegIntensity',
            intensity: -1000
        },
        expected: 'success|handled'
    },
    {
        scenario: 'Edge: Very High Intensity',
        toolName: 'manage_lighting',
        arguments: {
            action: 'spawn_light',
            lightType: 'Point',
            name: 'TC_HighIntensity',
            intensity: 1000000
        },
        expected: 'success'
    },

    // === CLEANUP ===
    {
        scenario: 'Cleanup - Delete test lights',
        toolName: 'control_actor',
        arguments: {
            action: 'delete',
            actorNames: [
                'TC_PointLight_1', 'TC_PointLight_2', 'TC_DirLight', 'TC_SpotLight',
                'TC_RectLight', 'TC_SkyLight_1', 'TC_SkyLight_2', 'TC_LightmassVolume',
                'TC_DynamicLight', 'TC_Sun', 'TC_ZeroIntensity', 'TC_NegIntensity', 'TC_HighIntensity'
            ]
        },
        expected: 'success|not_found'
    },
    {
        scenario: 'Cleanup - Delete lighting level',
        toolName: 'manage_level',
        arguments: { action: 'delete', levelPath: '/Game/Maps/TC_LightingLevel' },
        expected: 'success|not_found'
    }
];

await runToolTests('Lighting Management', testCases);
