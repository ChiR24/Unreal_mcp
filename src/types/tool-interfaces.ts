import { AutomationBridge } from '../automation/index.js';

export interface IBaseTool {
    getAutomationBridge(): AutomationBridge;
}

export interface StandardActionResponse<T = any> {
    success: boolean;
    data?: T;
    warnings?: string[];
    error?: string | {
        code?: string;
        message: string;
        [key: string]: unknown;
    } | null;
    [key: string]: unknown; // Allow compatibility fields
}

export interface IActorTools {
    spawn(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number }; actorName?: string; meshPath?: string; timeoutMs?: number }): Promise<StandardActionResponse>;
    delete(params: { actorName?: string; actorNames?: string[] }): Promise<StandardActionResponse>;
    applyForce(params: { actorName: string; force: { x: number; y: number; z: number } }): Promise<StandardActionResponse>;
    spawnBlueprint(params: { blueprintPath: string; actorName?: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }): Promise<StandardActionResponse>;
    setTransform(params: { actorName: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number }; scale?: { x: number; y: number; z: number } }): Promise<StandardActionResponse>;
    getTransform(actorName: string): Promise<StandardActionResponse>;
    setVisibility(params: { actorName: string; visible: boolean }): Promise<StandardActionResponse>;
    addComponent(params: { actorName: string; componentType: string; componentName?: string; properties?: Record<string, unknown> }): Promise<StandardActionResponse>;
    setComponentProperties(params: { actorName: string; componentName: string; properties: Record<string, unknown> }): Promise<StandardActionResponse>;
    getComponents(actorName: string): Promise<StandardActionResponse>;
    duplicate(params: { actorName: string; newName?: string; offset?: { x: number; y: number; z: number } }): Promise<StandardActionResponse>;
    addTag(params: { actorName: string; tag: string }): Promise<StandardActionResponse>;
    removeTag(params: { actorName: string; tag: string }): Promise<StandardActionResponse>;
    findByTag(params: { tag: string; matchType?: string }): Promise<StandardActionResponse>;
    findByName(name: string): Promise<StandardActionResponse>;
    detach(actorName: string): Promise<StandardActionResponse>;
    attach(params: { childActor: string; parentActor: string }): Promise<StandardActionResponse>;
    deleteByTag(tag: string): Promise<StandardActionResponse>;
    setBlueprintVariables(params: { actorName: string; variables: Record<string, unknown> }): Promise<StandardActionResponse>;
    createSnapshot(params: { actorName: string; snapshotName: string }): Promise<StandardActionResponse>;
    restoreSnapshot(params: { actorName: string; snapshotName: string }): Promise<StandardActionResponse>;
    listActors(params?: { filter?: string }): Promise<StandardActionResponse>;
    getMetadata(actorName: string): Promise<StandardActionResponse>;
    exportActor(params: { actorName: string; destinationPath?: string }): Promise<StandardActionResponse>;
    getBoundingBox(actorName: string): Promise<StandardActionResponse>;
}

export interface SourceControlState {
    isCheckedOut: boolean;
    isAdded: boolean;
    isDeleted: boolean;
    isModified: boolean;
    whoCheckedOut?: string;
}

export interface IAssetTools {
    importAsset(params: { sourcePath: string; destinationPath: string; overwrite?: boolean; save?: boolean }): Promise<StandardActionResponse>;
    createFolder(path: string): Promise<StandardActionResponse>;
    duplicateAsset(params: { sourcePath: string; destinationPath: string; overwrite?: boolean }): Promise<StandardActionResponse>;
    renameAsset(params: { sourcePath: string; destinationPath: string }): Promise<StandardActionResponse>;
    moveAsset(params: { sourcePath: string; destinationPath: string }): Promise<StandardActionResponse>;
    deleteAssets(params: { paths: string[]; fixupRedirectors?: boolean; timeoutMs?: number }): Promise<StandardActionResponse>;
    searchAssets(params: { classNames?: string[]; packagePaths?: string[]; recursivePaths?: boolean; recursiveClasses?: boolean; limit?: number }): Promise<StandardActionResponse>;
    saveAsset(assetPath: string): Promise<StandardActionResponse>;
    findByTag(params: { tag: string; value?: string }): Promise<StandardActionResponse>;
    getDependencies(params: { assetPath: string; recursive?: boolean }): Promise<StandardActionResponse>;
    getMetadata(params: { assetPath: string }): Promise<StandardActionResponse>;
    getSourceControlState(params: { assetPath: string }): Promise<SourceControlState | StandardActionResponse>;
    analyzeGraph(params: { assetPath: string; maxDepth?: number }): Promise<StandardActionResponse>;
    createThumbnail(params: { assetPath: string; width?: number; height?: number }): Promise<StandardActionResponse>;
    setTags(params: { assetPath: string; tags: string[] }): Promise<StandardActionResponse>;
    generateReport(params: { directory: string; reportType?: string; outputPath?: string }): Promise<StandardActionResponse>;
    validate(params: { assetPath: string }): Promise<StandardActionResponse>;
    generateLODs(params: { assetPath: string; lodCount: number; reductionSettings?: Record<string, unknown> }): Promise<StandardActionResponse>;
}

export interface ISequenceTools {
    create(params: { name: string; path?: string; timeoutMs?: number }): Promise<StandardActionResponse>;
    open(params: { path: string }): Promise<StandardActionResponse>;
    addCamera(params: { spawnable?: boolean; path?: string }): Promise<StandardActionResponse>;
    addActor(params: { actorName: string; createBinding?: boolean; path?: string }): Promise<StandardActionResponse>;
    addActors(params: { actorNames: string[]; path?: string }): Promise<StandardActionResponse>;
    removeActors(params: { actorNames: string[]; path?: string }): Promise<StandardActionResponse>;
    getBindings(params: { path?: string }): Promise<StandardActionResponse>;
    addSpawnableFromClass(params: { className: string; path?: string }): Promise<StandardActionResponse>;
    play(params: { path?: string; startTime?: number; loopMode?: 'once' | 'loop' | 'pingpong' }): Promise<StandardActionResponse>;
    pause(params?: { path?: string }): Promise<StandardActionResponse>;
    stop(params?: { path?: string }): Promise<StandardActionResponse>;
    setSequenceProperties(params: { path?: string; frameRate?: number; lengthInFrames?: number; playbackStart?: number; playbackEnd?: number }): Promise<StandardActionResponse>;
    getSequenceProperties(params: { path?: string }): Promise<StandardActionResponse>;
    setPlaybackSpeed(params: { speed: number; path?: string }): Promise<StandardActionResponse>;
    list(params: { path?: string }): Promise<StandardActionResponse>;
    duplicate(params: { path: string; destinationPath: string }): Promise<StandardActionResponse>;
    rename(params: { path: string; newName: string }): Promise<StandardActionResponse>;
    deleteSequence(params: { path: string }): Promise<StandardActionResponse>;
    getMetadata(params: { path: string }): Promise<StandardActionResponse>;
    listTracks(params: { path: string }): Promise<StandardActionResponse>;
    setWorkRange(params: { path?: string; start: number; end: number }): Promise<StandardActionResponse>;
}

export interface IAssetResources {
    list(directory?: string, recursive?: boolean, limit?: number): Promise<any>;
}

export interface IBlueprintTools {
    createBlueprint(params: { name: string; blueprintType?: string; savePath?: string; parentClass?: string; properties?: Record<string, unknown>; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<StandardActionResponse>;
    modifyConstructionScript(params: { blueprintPath: string; operations: any[]; compile?: boolean; save?: boolean; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<StandardActionResponse>;
    addComponent(params: { blueprintName: string; componentType: string; componentName: string; attachTo?: string; transform?: Record<string, unknown>; properties?: Record<string, unknown>; compile?: boolean; save?: boolean; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<StandardActionResponse>;
    waitForBlueprint(blueprintRef: string | string[], timeoutMs?: number): Promise<StandardActionResponse>;
    getBlueprint(params: { blueprintName: string; timeoutMs?: number }): Promise<StandardActionResponse>;
    getBlueprintInfo(params: { blueprintPath: string; timeoutMs?: number }): Promise<StandardActionResponse>;
    probeSubobjectDataHandle(opts?: { componentClass?: string }): Promise<StandardActionResponse>;
    setBlueprintDefault(params: { blueprintName: string; propertyName: string; value: unknown }): Promise<StandardActionResponse>;
    addVariable(params: { blueprintName: string; variableName: string; variableType: string; defaultValue?: any; category?: string; isReplicated?: boolean; isPublic?: boolean; variablePinType?: Record<string, unknown>; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<StandardActionResponse>;
    removeVariable(params: { blueprintName: string; variableName: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<StandardActionResponse>;
    renameVariable(params: { blueprintName: string; oldName: string; newName: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<StandardActionResponse>;
    addEvent(params: { blueprintName: string; eventType: string; customEventName?: string; parameters?: Array<{ name: string; type: string }>; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<StandardActionResponse>;
    removeEvent(params: { blueprintName: string; eventName: string; customEventName?: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<StandardActionResponse>;
    addFunction(params: { blueprintName: string; functionName: string; inputs?: Array<{ name: string; type: string }>; outputs?: Array<{ name: string; type: string }>; isPublic?: boolean; category?: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<StandardActionResponse>;
    setVariableMetadata(params: { blueprintName: string; variableName: string; metadata: Record<string, unknown>; timeoutMs?: number }): Promise<StandardActionResponse>;
    renameVariable(params: { blueprintName: string; oldName: string; newName: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<StandardActionResponse>;
    addConstructionScript(params: { blueprintName: string; scriptName: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<StandardActionResponse>;
    compileBlueprint(params: { blueprintName: string; saveAfterCompile?: boolean }): Promise<StandardActionResponse>;
    getBlueprintSCS(params: { blueprintPath: string; timeoutMs?: number }): Promise<StandardActionResponse>;
    addSCSComponent(params: { blueprintPath: string; componentClass: string; componentName: string; parentComponent?: string; meshPath?: string; materialPath?: string; timeoutMs?: number }): Promise<StandardActionResponse>;
    removeSCSComponent(params: { blueprintPath: string; componentName: string; timeoutMs?: number }): Promise<StandardActionResponse>;
    reparentSCSComponent(params: { blueprintPath: string; componentName: string; newParent: string; timeoutMs?: number }): Promise<StandardActionResponse>;
    setSCSComponentTransform(params: { blueprintPath: string; componentName: string; location?: [number, number, number]; rotation?: [number, number, number]; scale?: [number, number, number]; timeoutMs?: number }): Promise<StandardActionResponse>;
    setSCSComponentProperty(params: { blueprintPath: string; componentName: string; propertyName: string; propertyValue: any; timeoutMs?: number }): Promise<StandardActionResponse>;
    addNode(params: { blueprintName: string; nodeType: string; graphName?: string; functionName?: string; variableName?: string; nodeName?: string; eventName?: string; memberClass?: string; posX?: number; posY?: number; timeoutMs?: number }): Promise<StandardActionResponse>;
    connectPins(params: { blueprintName: string; sourceNodeGuid: string; targetNodeGuid: string; sourcePinName?: string; targetPinName?: string; timeoutMs?: number }): Promise<StandardActionResponse>;
}

export interface ILevelTools {
    listLevels(): Promise<StandardActionResponse>;
    getLevelSummary(levelPath?: string): Promise<StandardActionResponse>;
    registerLight(levelPath: string | undefined, info: { name: string; type: string; details?: Record<string, unknown> }): void;
    exportLevel(params: { levelPath?: string; exportPath: string; note?: string; timeoutMs?: number }): Promise<StandardActionResponse>;
    importLevel(params: { packagePath: string; destinationPath?: string; streaming?: boolean; timeoutMs?: number }): Promise<StandardActionResponse>;
    saveLevelAs(params: { sourcePath?: string; targetPath: string }): Promise<StandardActionResponse>;
    deleteLevels(params: { levelPaths: string[] }): Promise<StandardActionResponse>;
    loadLevel(params: { levelPath: string; streaming?: boolean; position?: [number, number, number] }): Promise<StandardActionResponse>;
    saveLevel(params: { levelName?: string; savePath?: string }): Promise<StandardActionResponse>;
    createLevel(params: { levelName: string; template?: 'Empty' | 'Default' | 'VR' | 'TimeOfDay'; savePath?: string }): Promise<StandardActionResponse>;
    addSubLevel(params: { parentLevel?: string; subLevelPath: string; streamingMethod?: 'Blueprint' | 'AlwaysLoaded' }): Promise<StandardActionResponse>;
    streamLevel(params: { levelPath?: string; levelName?: string; shouldBeLoaded: boolean; shouldBeVisible?: boolean; position?: [number, number, number] }): Promise<StandardActionResponse>;
    setupWorldComposition(params: { enableComposition: boolean; tileSize?: number; distanceStreaming?: boolean; streamingDistance?: number }): Promise<StandardActionResponse>;
    editLevelBlueprint(params: { eventType: 'BeginPlay' | 'EndPlay' | 'Tick' | 'Custom'; customEventName?: string; nodes?: Array<{ nodeType: string; position: [number, number]; connections?: string[] }> }): Promise<StandardActionResponse>;
    createSubLevel(params: { name: string; type: 'Persistent' | 'Streaming' | 'Lighting' | 'Gameplay'; parent?: string }): Promise<StandardActionResponse>;
    setWorldSettings(params: { gravity?: number; worldScale?: number; gameMode?: string; defaultPawn?: string; killZ?: number }): Promise<StandardActionResponse>;
    setLevelBounds(params: { min: [number, number, number]; max: [number, number, number] }): Promise<StandardActionResponse>;
    buildNavMesh(params: { rebuildAll?: boolean; selectedOnly?: boolean }): Promise<StandardActionResponse>;
    setLevelVisibility(params: { levelName: string; visible: boolean }): Promise<StandardActionResponse>;
    setWorldOrigin(params: { location: [number, number, number] }): Promise<StandardActionResponse>;
    createStreamingVolume(params: { levelName: string; position: [number, number, number]; size: [number, number, number]; streamingDistance?: number }): Promise<StandardActionResponse>;
    setLevelLOD(params: { levelName: string; lodLevel: number; distance: number }): Promise<StandardActionResponse>;
}

export interface IEditorTools {
    isInPIE(): Promise<boolean>;
    ensureNotInPIE(): Promise<void>;
    playInEditor(timeoutMs?: number): Promise<StandardActionResponse>;
    stopPlayInEditor(): Promise<StandardActionResponse>;
    pausePlayInEditor(): Promise<StandardActionResponse>;
    pauseInEditor(): Promise<StandardActionResponse>;
    buildLighting(): Promise<StandardActionResponse>;
    setViewportCamera(location?: { x: number; y: number; z: number } | [number, number, number] | null | undefined, rotation?: { pitch: number; yaw: number; roll: number } | [number, number, number] | null | undefined): Promise<StandardActionResponse>;
    setCameraSpeed(speed: number): Promise<StandardActionResponse>;
    setFOV(fov: number): Promise<StandardActionResponse>;
    takeScreenshot(filename?: string, resolution?: string): Promise<StandardActionResponse>;
    resumePlayInEditor(): Promise<StandardActionResponse>;
    stepPIEFrame(steps?: number): Promise<StandardActionResponse>;
    startRecording(options?: { filename?: string; frameRate?: number; durationSeconds?: number; metadata?: Record<string, unknown> }): Promise<StandardActionResponse>;
    stopRecording(): Promise<StandardActionResponse>;
    createCameraBookmark(name: string): Promise<StandardActionResponse>;
    jumpToCameraBookmark(name: string): Promise<StandardActionResponse>;
    setEditorPreferences(category: string | undefined, preferences: Record<string, unknown>): Promise<StandardActionResponse>;
    setViewportResolution(width: number, height: number): Promise<StandardActionResponse>;
    executeConsoleCommand(command: string): Promise<StandardActionResponse>;
}

export interface IEnvironmentTools {
    setTimeOfDay(hour: unknown): Promise<StandardActionResponse>;
    setSunIntensity(intensity: unknown): Promise<StandardActionResponse>;
    setSkylightIntensity(intensity: unknown): Promise<StandardActionResponse>;
    exportSnapshot(params: { path?: unknown; filename?: unknown }): Promise<StandardActionResponse>;
    importSnapshot(params: { path?: unknown; filename?: unknown }): Promise<StandardActionResponse>;
    cleanup(params?: { names?: unknown }): Promise<StandardActionResponse>;
}

export interface ILandscapeTools {
    createLandscape(params: { name: string; location?: [number, number, number]; sizeX?: number; sizeY?: number; quadsPerSection?: number; sectionsPerComponent?: number; componentCount?: number; materialPath?: string; enableWorldPartition?: boolean; runtimeGrid?: string; isSpatiallyLoaded?: boolean; dataLayers?: string[] }): Promise<StandardActionResponse>;
    sculptLandscape(params: { landscapeName: string; tool: string; brushSize?: number; brushFalloff?: number; strength?: number; location?: [number, number, number]; radius?: number }): Promise<StandardActionResponse>;
    paintLandscape(params: { landscapeName: string; layerName: string; position: [number, number, number]; brushSize?: number; strength?: number; targetValue?: number; radius?: number; density?: number }): Promise<StandardActionResponse>;
    createProceduralTerrain(params: { name: string; location?: [number, number, number]; subdivisions?: number; heightFunction?: string; material?: string; settings?: Record<string, unknown> }): Promise<StandardActionResponse>;
    createLandscapeGrassType(params: { name: string; meshPath: string; density?: number; minScale?: number; maxScale?: number; path?: string; staticMesh?: string }): Promise<StandardActionResponse>;
    setLandscapeMaterial(params: { landscapeName: string; materialPath: string }): Promise<StandardActionResponse>;
    modifyHeightmap(params: { landscapeName: string; heightData: number[]; minX: number; minY: number; maxX: number; maxY: number; updateNormals?: boolean }): Promise<StandardActionResponse>;
}

export interface IFoliageTools {
    addFoliageType(params: { name: string; meshPath: string; density?: number; radius?: number; minScale?: number; maxScale?: number; alignToNormal?: boolean; randomYaw?: boolean; groundSlope?: number }): Promise<StandardActionResponse>;
    addFoliage(params: { foliageType: string; locations: Array<{ x: number; y: number; z: number }> }): Promise<StandardActionResponse>;
    paintFoliage(params: { foliageType: string; position: [number, number, number]; brushSize?: number; paintDensity?: number; eraseMode?: boolean }): Promise<StandardActionResponse>;
    createProceduralFoliage(params: { name: string; bounds?: { location: { x: number; y: number; z: number }; size: { x: number; y: number; z: number } }; foliageTypes?: Array<{ meshPath: string; density: number; minScale?: number; maxScale?: number; alignToNormal?: boolean; randomYaw?: boolean }>; volumeName?: string; position?: [number, number, number]; size?: [number, number, number]; seed?: number; tileSize?: number }): Promise<StandardActionResponse>;
    addFoliageInstances(params: { foliageType: string; transforms: Array<{ location: [number, number, number]; rotation?: [number, number, number]; scale?: [number, number, number] }> }): Promise<StandardActionResponse>;
    getFoliageInstances(params: { foliageType?: string }): Promise<StandardActionResponse>;
    removeFoliage(params: { foliageType?: string; removeAll?: boolean }): Promise<StandardActionResponse>;
    createInstancedMesh(params: { name: string; meshPath: string; instances: Array<{ position: [number, number, number]; rotation?: [number, number, number]; scale?: [number, number, number] }>; enableCulling?: boolean; cullDistance?: number }): Promise<StandardActionResponse>;
    setFoliageLOD(params: { foliageType: string; lodDistances?: number[]; screenSize?: number[] }): Promise<StandardActionResponse>;
    setFoliageCollision(params: { foliageType: string; collisionEnabled?: boolean; collisionProfile?: string; generateOverlapEvents?: boolean }): Promise<StandardActionResponse>;
    createGrassSystem(params: { name: string; grassTypes: Array<{ meshPath: string; density: number; minScale?: number; maxScale?: number }>; windStrength?: number; windSpeed?: number }): Promise<StandardActionResponse>;
    removeFoliageInstances(params: { foliageType: string; position: [number, number, number]; radius: number }): Promise<StandardActionResponse>;
    selectFoliageInstances(params: { foliageType: string; position?: [number, number, number]; radius?: number; selectAll?: boolean }): Promise<StandardActionResponse>;
    updateFoliageInstances(params: { foliageType: string; updateTransforms?: boolean; updateMesh?: boolean; newMeshPath?: string }): Promise<StandardActionResponse>;
    createFoliageSpawner(params: { name: string; spawnArea: 'Landscape' | 'StaticMesh' | 'BSP' | 'Foliage' | 'All'; excludeAreas?: Array<[number, number, number, number]> }): Promise<StandardActionResponse>;
    optimizeFoliage(params: { mergeInstances?: boolean; generateClusters?: boolean; clusterSize?: number; reduceDrawCalls?: boolean }): Promise<StandardActionResponse>;
}

export interface ITools {
    actorTools: IActorTools;
    assetTools: IAssetTools;
    blueprintTools: IBlueprintTools;
    editorTools: IEditorTools;
    levelTools: ILevelTools;
    sequenceTools: ISequenceTools;
    assetResources: IAssetResources;
    landscapeTools: ILandscapeTools;
    foliageTools: IFoliageTools;
    environmentTools: IEnvironmentTools;

    // Added newly required tools to remove 'any' casting
    materialTools: any;
    niagaraTools: any;
    animationTools: any;
    physicsTools: any;
    lightingTools: any;
    debugTools: any;
    performanceTools: any;
    audioTools: any;
    uiTools: any;
    introspectionTools: any;
    visualTools?: any; // Optional as it's being removed
    engineTools: any;
    systemTools: any;
    behaviorTreeTools: any;
    logTools: any;

    automationBridge?: AutomationBridge;
    [key: string]: any;
}
