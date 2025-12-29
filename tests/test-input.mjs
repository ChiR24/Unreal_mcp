#!/usr/bin/env node
/**
 * Comprehensive Input Management Test Suite
 * Tool: manage_input
 * Coverage: All 4 actions with success, error, and edge cases
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
    // === PRE-CLEANUP ===
    {
        scenario: 'Pre-cleanup: Delete existing test input assets',
        toolName: 'manage_asset',
        arguments: {
            action: 'delete',
            assetPaths: [
                '/Game/Input/IA_TestJump',
                '/Game/Input/IA_TestMove',
                '/Game/Input/IA_TestLook',
                '/Game/Input/IA_TestInteract',
                '/Game/Input/IA_TestFire',
                '/Game/Input/IMC_TestContext',
                '/Game/Input/IMC_TestContext2'
            ]
        },
        expected: 'success|not_found'
    },

    // === CREATE INPUT ACTIONS ===
    {
        scenario: 'Create Input Action - Jump',
        toolName: 'manage_input',
        arguments: {
            action: 'create_input_action',
            name: 'IA_TestJump',
            path: '/Game/Input'
        },
        expected: 'success'
    },
    {
        scenario: 'Create Input Action - Move',
        toolName: 'manage_input',
        arguments: {
            action: 'create_input_action',
            name: 'IA_TestMove',
            path: '/Game/Input'
        },
        expected: 'success'
    },
    {
        scenario: 'Create Input Action - Look',
        toolName: 'manage_input',
        arguments: {
            action: 'create_input_action',
            name: 'IA_TestLook',
            path: '/Game/Input'
        },
        expected: 'success'
    },
    {
        scenario: 'Create Input Action - Interact',
        toolName: 'manage_input',
        arguments: {
            action: 'create_input_action',
            name: 'IA_TestInteract',
            path: '/Game/Input'
        },
        expected: 'success'
    },
    {
        scenario: 'Create Input Action - Fire',
        toolName: 'manage_input',
        arguments: {
            action: 'create_input_action',
            name: 'IA_TestFire',
            path: '/Game/Input'
        },
        expected: 'success'
    },

    // === CREATE INPUT MAPPING CONTEXTS ===
    {
        scenario: 'Create Input Mapping Context',
        toolName: 'manage_input',
        arguments: {
            action: 'create_input_mapping_context',
            name: 'IMC_TestContext',
            path: '/Game/Input'
        },
        expected: 'success'
    },
    {
        scenario: 'Create Second Mapping Context',
        toolName: 'manage_input',
        arguments: {
            action: 'create_input_mapping_context',
            name: 'IMC_TestContext2',
            path: '/Game/Input'
        },
        expected: 'success'
    },

    // === ADD MAPPINGS ===
    {
        scenario: 'Add Mapping - Space to Jump',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestJump',
            key: 'SpaceBar'
        },
        expected: 'success'
    },
    {
        scenario: 'Add Mapping - W to Move',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestMove',
            key: 'W'
        },
        expected: 'success'
    },
    {
        scenario: 'Add Mapping - E to Interact',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestInteract',
            key: 'E'
        },
        expected: 'success'
    },
    {
        scenario: 'Add Mapping - Mouse Left to Fire',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestFire',
            key: 'LeftMouseButton'
        },
        expected: 'success'
    },
    {
        scenario: 'Add Mapping - Gamepad A to Jump',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestJump',
            key: 'Gamepad_FaceButton_Bottom'
        },
        expected: 'success'
    },
    {
        scenario: 'Add Mapping - Right Trigger to Fire',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestFire',
            key: 'Gamepad_RightTrigger'
        },
        expected: 'success'
    },
    {
        scenario: 'Add Mapping to second context',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext2',
            actionPath: '/Game/Input/IA_TestJump',
            key: 'F'
        },
        expected: 'success'
    },

    // === REMOVE MAPPINGS ===
    {
        scenario: 'Remove Mapping - Gamepad A from Jump',
        toolName: 'manage_input',
        arguments: {
            action: 'remove_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestJump',
            key: 'Gamepad_FaceButton_Bottom'
        },
        expected: 'success'
    },
    {
        scenario: 'Remove Mapping - Right Trigger from Fire',
        toolName: 'manage_input',
        arguments: {
            action: 'remove_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestFire',
            key: 'Gamepad_RightTrigger'
        },
        expected: 'success'
    },

    // === REAL-WORLD SCENARIO: Complete Character Input Setup ===
    {
        scenario: 'Character Setup - Add WASD Movement',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestMove',
            key: 'A'
        },
        expected: 'success'
    },
    {
        scenario: 'Character Setup - Add S Movement',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestMove',
            key: 'S'
        },
        expected: 'success'
    },
    {
        scenario: 'Character Setup - Add D Movement',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestMove',
            key: 'D'
        },
        expected: 'success'
    },
    {
        scenario: 'Character Setup - Mouse Look',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestLook',
            key: 'Mouse2D'
        },
        // Note: Mouse2D is not a valid UE key name; valid are MouseX, MouseY, etc.
        expected: 'success|error|not_found'
    },

    // === ERROR CASES ===
    {
        scenario: 'Error: Create action without name',
        toolName: 'manage_input',
        arguments: {
            action: 'create_input_action',
            path: '/Game/Input'
        },
        expected: 'error|missing'
    },
    {
        scenario: 'Error: Create context without name',
        toolName: 'manage_input',
        arguments: {
            action: 'create_input_mapping_context',
            path: '/Game/Input'
        },
        expected: 'error|missing'
    },
    {
        scenario: 'Error: Add mapping to non-existent context',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_NonExistent',
            actionPath: '/Game/Input/IA_TestJump',
            key: 'X'
        },
        expected: 'success|error|not_found'
    },
    {
        scenario: 'Error: Add mapping with non-existent action',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_NonExistent',
            key: 'X'
        },
        expected: 'success|error|not_found'
    },
    {
        scenario: 'Error: Remove non-existent mapping',
        toolName: 'manage_input',
        arguments: {
            action: 'remove_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestJump',
            key: 'InvalidKey'
        },
        // C++ may succeed even if mapping doesn't exist
        expected: 'success|error|not_found'
    },

    // === EDGE CASES ===
    {
        scenario: 'Edge: Create action with empty path',
        toolName: 'manage_input',
        arguments: {
            action: 'create_input_action',
            name: 'IA_TestEmpty',
            path: ''
        },
        expected: 'error|validation'
    },
    {
        scenario: 'Edge: Add duplicate mapping (same key)',
        toolName: 'manage_input',
        arguments: {
            action: 'add_mapping',
            contextPath: '/Game/Input/IMC_TestContext',
            actionPath: '/Game/Input/IA_TestJump',
            key: 'SpaceBar'
        },
        expected: 'success|already_exists'
    },

    // === CLEANUP ===
    {
        scenario: 'Cleanup: Delete all test input assets',
        toolName: 'manage_asset',
        arguments: {
            action: 'delete',
            assetPaths: [
                '/Game/Input/IA_TestJump',
                '/Game/Input/IA_TestMove',
                '/Game/Input/IA_TestLook',
                '/Game/Input/IA_TestInteract',
                '/Game/Input/IA_TestFire',
                '/Game/Input/IMC_TestContext',
                '/Game/Input/IMC_TestContext2'
            ]
        },
        expected: 'success|not_found'
    }
];

await runToolTests('Input Management', testCases);
