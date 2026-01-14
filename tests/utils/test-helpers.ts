/**
 * Test Utilities Library
 * Centralized mock factories and assertions for unit tests.
 */
import { vi } from 'vitest';
import type { ITools, IActorTools, IAssetTools, IBlueprintTools, IEditorTools, ILevelTools } from '../../src/types/tool-interfaces.js';

/**
 * Mock AutomationBridge with common methods.
 */
export interface MockAutomationBridge {
    isConnected: () => boolean;
    sendAutomationRequest: ReturnType<typeof vi.fn>;
}

/**
 * Creates a mock AutomationBridge for testing.
 * @param overrides - Optional overrides for bridge methods
 * @returns MockAutomationBridge with `isConnected` and `sendAutomationRequest`
 */
export function createMockAutomationBridge(
    overrides: Partial<MockAutomationBridge> = {}
): MockAutomationBridge {
    return {
        isConnected: overrides.isConnected ?? (() => true),
        sendAutomationRequest: overrides.sendAutomationRequest ?? vi.fn().mockResolvedValue({ success: true }),
    };
}

/**
 * Creates a mock actorTools object with all methods mocked.
 */
function createMockActorTools(): IActorTools {
    return {
        spawn: vi.fn().mockResolvedValue({ success: true, actorName: 'SpawnedActor_1' }),
        delete: vi.fn().mockResolvedValue({ success: true }),
        applyForce: vi.fn().mockResolvedValue({ success: true }),
        setTransform: vi.fn().mockResolvedValue({ success: true }),
        getTransform: vi.fn().mockResolvedValue({ success: true, location: { x: 0, y: 0, z: 0 } }),
        duplicate: vi.fn().mockResolvedValue({ success: true }),
        attach: vi.fn().mockResolvedValue({ success: true }),
        detach: vi.fn().mockResolvedValue({ success: true }),
        addTag: vi.fn().mockResolvedValue({ success: true }),
        removeTag: vi.fn().mockResolvedValue({ success: true }),
        findByTag: vi.fn().mockResolvedValue({ success: true, actors: [] }),
        deleteByTag: vi.fn().mockResolvedValue({ success: true }),
        spawnBlueprint: vi.fn().mockResolvedValue({ success: true, actorName: 'BP_Actor_1' }),
        listActors: vi.fn().mockResolvedValue({ success: true, actors: [] }),
        findByName: vi.fn().mockResolvedValue({ success: true, actors: [] }),
        getComponents: vi.fn().mockResolvedValue({ success: true, components: [] }),
        setComponentProperties: vi.fn().mockResolvedValue({ success: true }),
        setVisibility: vi.fn().mockResolvedValue({ success: true }),
        addComponent: vi.fn().mockResolvedValue({ success: true }),
        setBlueprintVariables: vi.fn().mockResolvedValue({ success: true }),
        createSnapshot: vi.fn().mockResolvedValue({ success: true }),
        restoreSnapshot: vi.fn().mockResolvedValue({ success: true }),
        getMetadata: vi.fn().mockResolvedValue({ success: true }),
        exportActor: vi.fn().mockResolvedValue({ success: true }),
        getBoundingBox: vi.fn().mockResolvedValue({ success: true }),
    };
}

/**
 * Creates a mock assetTools object with all methods mocked.
 */
function createMockAssetTools(): IAssetTools {
    return {
        importAsset: vi.fn().mockResolvedValue({ success: true }),
        createFolder: vi.fn().mockResolvedValue({ success: true }),
        duplicateAsset: vi.fn().mockResolvedValue({ success: true }),
        renameAsset: vi.fn().mockResolvedValue({ success: true }),
        moveAsset: vi.fn().mockResolvedValue({ success: true }),
        deleteAssets: vi.fn().mockResolvedValue({ success: true }),
        searchAssets: vi.fn().mockResolvedValue({ success: true, assets: [] }),
        saveAsset: vi.fn().mockResolvedValue({ success: true }),
        findByTag: vi.fn().mockResolvedValue({ success: true, assets: [] }),
        getDependencies: vi.fn().mockResolvedValue({ success: true, dependencies: [] }),
        getMetadata: vi.fn().mockResolvedValue({ success: true }),
        getSourceControlState: vi.fn().mockResolvedValue({ isCheckedOut: false, isAdded: false, isDeleted: false, isModified: false }),
        analyzeGraph: vi.fn().mockResolvedValue({ success: true }),
        createThumbnail: vi.fn().mockResolvedValue({ success: true }),
        setTags: vi.fn().mockResolvedValue({ success: true }),
        generateReport: vi.fn().mockResolvedValue({ success: true }),
        validate: vi.fn().mockResolvedValue({ success: true }),
        generateLODs: vi.fn().mockResolvedValue({ success: true }),
    };
}

/**
 * Creates a mock blueprintTools object with all methods mocked.
 */
function createMockBlueprintTools(): IBlueprintTools {
    return {
        createBlueprint: vi.fn().mockResolvedValue({ success: true }),
        modifyConstructionScript: vi.fn().mockResolvedValue({ success: true }),
        addComponent: vi.fn().mockResolvedValue({ success: true }),
        waitForBlueprint: vi.fn().mockResolvedValue({ success: true }),
        getBlueprint: vi.fn().mockResolvedValue({ success: true }),
        getBlueprintInfo: vi.fn().mockResolvedValue({ success: true }),
        probeSubobjectDataHandle: vi.fn().mockResolvedValue({ success: true }),
        setBlueprintDefault: vi.fn().mockResolvedValue({ success: true }),
        addVariable: vi.fn().mockResolvedValue({ success: true }),
        removeVariable: vi.fn().mockResolvedValue({ success: true }),
        renameVariable: vi.fn().mockResolvedValue({ success: true }),
        addEvent: vi.fn().mockResolvedValue({ success: true }),
        removeEvent: vi.fn().mockResolvedValue({ success: true }),
        addFunction: vi.fn().mockResolvedValue({ success: true }),
        setVariableMetadata: vi.fn().mockResolvedValue({ success: true }),
        addConstructionScript: vi.fn().mockResolvedValue({ success: true }),
        compileBlueprint: vi.fn().mockResolvedValue({ success: true }),
        getBlueprintSCS: vi.fn().mockResolvedValue({ success: true }),
        addSCSComponent: vi.fn().mockResolvedValue({ success: true }),
        removeSCSComponent: vi.fn().mockResolvedValue({ success: true }),
        reparentSCSComponent: vi.fn().mockResolvedValue({ success: true }),
        setSCSComponentTransform: vi.fn().mockResolvedValue({ success: true }),
        setSCSComponentProperty: vi.fn().mockResolvedValue({ success: true }),
        addNode: vi.fn().mockResolvedValue({ success: true }),
        connectPins: vi.fn().mockResolvedValue({ success: true }),
    };
}

/**
 * Creates a mock editorTools object with all methods mocked.
 */
function createMockEditorTools(): IEditorTools {
    return {
        isInPIE: vi.fn().mockResolvedValue(false),
        ensureNotInPIE: vi.fn().mockResolvedValue(undefined),
        playInEditor: vi.fn().mockResolvedValue({ success: true }),
        stopPlayInEditor: vi.fn().mockResolvedValue({ success: true }),
        pausePlayInEditor: vi.fn().mockResolvedValue({ success: true }),
        pauseInEditor: vi.fn().mockResolvedValue({ success: true }),
        buildLighting: vi.fn().mockResolvedValue({ success: true }),
        setViewportCamera: vi.fn().mockResolvedValue({ success: true }),
        setCameraSpeed: vi.fn().mockResolvedValue({ success: true }),
        setFOV: vi.fn().mockResolvedValue({ success: true }),
        takeScreenshot: vi.fn().mockResolvedValue({ success: true }),
        resumePlayInEditor: vi.fn().mockResolvedValue({ success: true }),
        stepPIEFrame: vi.fn().mockResolvedValue({ success: true }),
        startRecording: vi.fn().mockResolvedValue({ success: true }),
        stopRecording: vi.fn().mockResolvedValue({ success: true }),
        createCameraBookmark: vi.fn().mockResolvedValue({ success: true }),
        jumpToCameraBookmark: vi.fn().mockResolvedValue({ success: true }),
        setEditorPreferences: vi.fn().mockResolvedValue({ success: true }),
        setViewportResolution: vi.fn().mockResolvedValue({ success: true }),
        setViewportRealtime: vi.fn().mockResolvedValue({ success: true }),
        executeConsoleCommand: vi.fn().mockResolvedValue({ success: true }),
    };
}

/**
 * Creates a mock levelTools object with all methods mocked.
 */
function createMockLevelTools(): ILevelTools {
    return {
        listLevels: vi.fn().mockResolvedValue({ success: true, levels: [] }),
        getLevelSummary: vi.fn().mockResolvedValue({ success: true }),
        registerLight: vi.fn(),
        exportLevel: vi.fn().mockResolvedValue({ success: true }),
        importLevel: vi.fn().mockResolvedValue({ success: true }),
        saveLevelAs: vi.fn().mockResolvedValue({ success: true }),
        deleteLevels: vi.fn().mockResolvedValue({ success: true }),
        loadLevel: vi.fn().mockResolvedValue({ success: true }),
        saveLevel: vi.fn().mockResolvedValue({ success: true }),
        createLevel: vi.fn().mockResolvedValue({ success: true }),
        addSubLevel: vi.fn().mockResolvedValue({ success: true }),
        streamLevel: vi.fn().mockResolvedValue({ success: true }),
        setupWorldComposition: vi.fn().mockResolvedValue({ success: true }),
        editLevelBlueprint: vi.fn().mockResolvedValue({ success: true }),
        createSubLevel: vi.fn().mockResolvedValue({ success: true }),
        setWorldSettings: vi.fn().mockResolvedValue({ success: true }),
        setLevelBounds: vi.fn().mockResolvedValue({ success: true }),
        buildNavMesh: vi.fn().mockResolvedValue({ success: true }),
        setLevelVisibility: vi.fn().mockResolvedValue({ success: true }),
        setWorldOrigin: vi.fn().mockResolvedValue({ success: true }),
        createStreamingVolume: vi.fn().mockResolvedValue({ success: true }),
        setLevelLOD: vi.fn().mockResolvedValue({ success: true }),
    };
}

/**
 * Creates a full mock ITools object for testing.
 * @param overrides - Optional partial overrides for any tool set
 * @returns Typed ITools mock object
 */
export function createMockTools(overrides: Partial<ITools> = {}): ITools {
    const mockBridge = createMockAutomationBridge();
    
    return {
        automationBridge: mockBridge as unknown as ITools['automationBridge'],
        actorTools: createMockActorTools(),
        assetTools: createMockAssetTools(),
        blueprintTools: createMockBlueprintTools(),
        editorTools: createMockEditorTools(),
        levelTools: createMockLevelTools(),
        sequenceTools: {} as ITools['sequenceTools'],
        assetResources: { list: vi.fn().mockResolvedValue({}) } as ITools['assetResources'],
        landscapeTools: {} as ITools['landscapeTools'],
        foliageTools: {} as ITools['foliageTools'],
        environmentTools: {} as ITools['environmentTools'],
        materialTools: {} as ITools['materialTools'],
        niagaraTools: {} as ITools['niagaraTools'],
        animationTools: {} as ITools['animationTools'],
        physicsTools: {} as ITools['physicsTools'],
        lightingTools: {} as ITools['lightingTools'],
        debugTools: {} as ITools['debugTools'],
        performanceTools: {} as ITools['performanceTools'],
        audioTools: {} as ITools['audioTools'],
        uiTools: {} as ITools['uiTools'],
        introspectionTools: {} as ITools['introspectionTools'],
        engineTools: {} as ITools['engineTools'],
        systemTools: {
            executeConsoleCommand: vi.fn().mockResolvedValue({ success: true }),
            getProjectSettings: vi.fn().mockResolvedValue({}),
        },
        behaviorTreeTools: {} as ITools['behaviorTreeTools'],
        logTools: {} as ITools['logTools'],
        ...overrides,
    } as ITools;
}

/**
 * Recursively checks that an object has no undefined values.
 * @param obj - The object to check
 * @param path - Current path for error messages (internal use)
 * @throws Error if any undefined value is found
 */
export function expectCleanObject(obj: unknown, path = ''): void {
    if (obj === undefined) {
        throw new Error(`Found undefined value at path: ${path || 'root'}`);
    }
    
    if (obj === null || typeof obj !== 'object') {
        return; // Primitives and null are allowed
    }
    
    if (Array.isArray(obj)) {
        obj.forEach((item, index) => {
            expectCleanObject(item, `${path}[${index}]`);
        });
        return;
    }
    
    const record = obj as Record<string, unknown>;
    for (const key of Object.keys(record)) {
        const value = record[key];
        const currentPath = path ? `${path}.${key}` : key;
        
        if (value === undefined) {
            throw new Error(`Found undefined value at path: ${currentPath}`);
        }
        
        if (typeof value === 'object' && value !== null) {
            expectCleanObject(value, currentPath);
        }
    }
}

/**
 * Helper to check if a mock function was called with specific arguments.
 * @param mockFn - The vitest mock function
 * @param expectedArgs - Expected arguments
 * @returns boolean indicating if the call was made
 */
export function wasCalledWith(
    mockFn: ReturnType<typeof vi.fn>,
    ...expectedArgs: unknown[]
): boolean {
    return mockFn.mock.calls.some((callArgs: unknown[]) => {
        if (callArgs.length !== expectedArgs.length) return false;
        return callArgs.every((arg, i) => 
            JSON.stringify(arg) === JSON.stringify(expectedArgs[i])
        );
    });
}

/**
 * Creates a mock response factory for simulating bridge responses.
 */
export const mockResponses = {
    success: <T extends Record<string, unknown>>(data: T = {} as T) => ({
        success: true,
        ...data,
    }),
    
    error: (message: string, code?: string) => ({
        success: false,
        message,
        ...(code ? { error: { code, message } } : {}),
    }),
    
    bridgeUnavailable: () => ({
        success: false,
        error: 'BRIDGE_UNAVAILABLE',
        message: 'Automation bridge not connected',
    }),
};
