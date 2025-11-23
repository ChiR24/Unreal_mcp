import { AutomationBridge } from '../automation-bridge.js';

export interface IBaseTool {
    getAutomationBridge(): AutomationBridge;
}

export interface IActorTools {
    spawn(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number }; actorName?: string; timeoutMs?: number }): Promise<any>;
    delete(params: { actorName?: string; actorNames?: string[] }): Promise<any>;
    applyForce(params: { actorName: string; force: { x: number; y: number; z: number } }): Promise<any>;
    spawnBlueprint(params: { blueprintPath: string; actorName?: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }): Promise<any>;
    setTransform(params: { actorName: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number }; scale?: { x: number; y: number; z: number } }): Promise<any>;
    getTransform(actorName: string): Promise<any>;
    setVisibility(params: { actorName: string; visible: boolean }): Promise<any>;
    addComponent(params: { actorName: string; componentType: string; componentName?: string; properties?: Record<string, unknown> }): Promise<any>;
    setComponentProperties(params: { actorName: string; componentName: string; properties: Record<string, unknown> }): Promise<any>;
    getComponents(actorName: string): Promise<any>;
    duplicate(params: { actorName: string; newName?: string; offset?: { x: number; y: number; z: number } }): Promise<any>;
    addTag(params: { actorName: string; tag: string }): Promise<any>;
    findByTag(params: { tag: string; matchType?: string }): Promise<any>;
    findByName(name: string): Promise<any>;
    detach(actorName: string): Promise<any>;
    attach(params: { childActor: string; parentActor: string }): Promise<any>;
    deleteByTag(tag: string): Promise<any>;
    setBlueprintVariables(params: { actorName: string; variables: Record<string, unknown> }): Promise<any>;
    createSnapshot(params: { actorName: string; snapshotName: string }): Promise<any>;
    restoreSnapshot(params: { actorName: string; snapshotName: string }): Promise<any>;
    listActors(): Promise<any>;
    getMetadata(actorName: string): Promise<any>;
    exportActor(params: { actorName: string; destinationPath?: string }): Promise<any>;
    getBoundingBox(actorName: string): Promise<any>;
}

export interface SourceControlState {
    isCheckedOut: boolean;
    isAdded: boolean;
    isDeleted: boolean;
    isModified: boolean;
    whoCheckedOut?: string;
}

export interface IAssetTools {
    importAsset(params: { sourcePath: string; destinationPath: string; overwrite?: boolean; save?: boolean }): Promise<any>;
    createFolder(path: string): Promise<any>;
    duplicateAsset(params: { sourcePath: string; destinationPath: string }): Promise<any>;
    renameAsset(params: { sourcePath: string; destinationPath: string }): Promise<any>;
    moveAsset(params: { sourcePath: string; destinationPath: string }): Promise<any>;
    deleteAssets(params: { paths: string[]; fixupRedirectors?: boolean; timeoutMs?: number }): Promise<any>;
    saveAsset(assetPath: string): Promise<any>;
    getDependencies(params: { assetPath: string; recursive?: boolean }): Promise<any>;
    getSourceControlState(params: { assetPath: string }): Promise<SourceControlState | any>;
    analyzeGraph(params: { assetPath: string; maxDepth?: number }): Promise<any>;
    createThumbnail(params: { assetPath: string; width?: number; height?: number }): Promise<any>;
    setTags(params: { assetPath: string; tags: string[] }): Promise<any>;
    generateReport(params: { directory: string; reportType?: string; outputPath?: string }): Promise<any>;
    validate(params: { assetPath: string }): Promise<any>;
    generateLODs(params: { assetPath: string; lodCount: number; reductionSettings?: Record<string, unknown> }): Promise<any>;
}

export interface IEditorTools {
    playInEditor(): Promise<any>;
    stopPlayInEditor(): Promise<any>;
    pausePlayInEditor(): Promise<any>;
    takeScreenshot(filename?: string): Promise<any>;
}

export interface ILevelTools {
    loadLevel(params: { levelPath: string; streaming?: boolean }): Promise<any>;
}

export interface ISequenceTools {
    create(params: { name: string; path?: string; timeoutMs?: number }): Promise<any>;
    open(params: { path: string }): Promise<any>;
    addCamera(params: { spawnable?: boolean }): Promise<any>;
    addActor(params: { actorName: string; createBinding?: boolean }): Promise<any>;
    addActors(params: { actorNames: string[] }): Promise<any>;
    removeActors(params: { actorNames: string[] }): Promise<any>;
    getBindings(params: { path?: string }): Promise<any>;
    addSpawnableFromClass(params: { className: string; path?: string }): Promise<any>;
    play(params: { startTime?: number; loopMode?: 'once' | 'loop' | 'pingpong' }): Promise<any>;
    pause(): Promise<any>;
    stop(): Promise<any>;
    setSequenceProperties(params: { path?: string; frameRate?: number; lengthInFrames?: number; playbackStart?: number; playbackEnd?: number }): Promise<any>;
    getSequenceProperties(params: { path?: string }): Promise<any>;
    setPlaybackSpeed(params: { speed: number }): Promise<any>;
    list(params: { path?: string }): Promise<any>;
    duplicate(params: { path: string; destinationPath: string }): Promise<any>;
    rename(params: { path: string; newName: string }): Promise<any>;
    deleteSequence(params: { path: string }): Promise<any>;
    getMetadata(params: { path: string }): Promise<any>;
}

export interface IAssetResources {
    list(directory?: string, recursive?: boolean, limit?: number): Promise<any>;
}

export interface IBlueprintTools {
    createBlueprint(params: { name: string; blueprintType?: string; savePath?: string; parentClass?: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<any>;
    modifyConstructionScript(params: { blueprintPath: string; operations: any[]; compile?: boolean; save?: boolean; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<any>;
    addComponent(params: { blueprintName: string; componentType: string; componentName: string; attachTo?: string; transform?: Record<string, unknown>; properties?: Record<string, unknown>; compile?: boolean; save?: boolean; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<any>;
    waitForBlueprint(blueprintRef: string | string[], timeoutMs?: number): Promise<any>;
    getBlueprint(params: { blueprintName: string; timeoutMs?: number }): Promise<any>;
    getBlueprintInfo(params: { blueprintPath: string; timeoutMs?: number }): Promise<any>;
    probeSubobjectDataHandle(opts?: { componentClass?: string }): Promise<any>;
    setBlueprintDefault(params: { blueprintName: string; propertyName: string; value: unknown }): Promise<any>;
    addVariable(params: { blueprintName: string; variableName: string; variableType: string; defaultValue?: any; category?: string; isReplicated?: boolean; isPublic?: boolean; variablePinType?: Record<string, unknown>; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<any>;
    addEvent(params: { blueprintName: string; eventType: string; customEventName?: string; parameters?: Array<{ name: string; type: string }>; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<any>;
    removeEvent(params: { blueprintName: string; eventName: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<any>;
    addFunction(params: { blueprintName: string; functionName: string; inputs?: Array<{ name: string; type: string }>; outputs?: Array<{ name: string; type: string }>; isPublic?: boolean; category?: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<any>;
    setVariableMetadata(params: { blueprintName: string; variableName: string; metadata: Record<string, unknown>; timeoutMs?: number }): Promise<any>;
    addConstructionScript(params: { blueprintName: string; scriptName: string; timeoutMs?: number; waitForCompletion?: boolean; waitForCompletionTimeoutMs?: number }): Promise<any>;
    compileBlueprint(params: { blueprintName: string; saveAfterCompile?: boolean }): Promise<any>;
    getBlueprintSCS(params: { blueprintPath: string; timeoutMs?: number }): Promise<any>;
    addSCSComponent(params: { blueprintPath: string; componentClass: string; componentName: string; parentComponent?: string; timeoutMs?: number }): Promise<any>;
    removeSCSComponent(params: { blueprintPath: string; componentName: string; timeoutMs?: number }): Promise<any>;
    reparentSCSComponent(params: { blueprintPath: string; componentName: string; newParent: string; timeoutMs?: number }): Promise<any>;
    setSCSComponentTransform(params: { blueprintPath: string; componentName: string; location?: [number, number, number]; rotation?: [number, number, number]; scale?: [number, number, number]; timeoutMs?: number }): Promise<any>;
    setSCSComponentProperty(params: { blueprintPath: string; componentName: string; propertyName: string; propertyValue: any; timeoutMs?: number }): Promise<any>;
    addNode(params: { blueprintName: string; nodeType: string; graphName?: string; functionName?: string; variableName?: string; nodeName?: string; posX?: number; posY?: number; timeoutMs?: number }): Promise<any>;
    connectPins(params: { blueprintName: string; sourceNodeGuid: string; targetNodeGuid: string; sourcePinName?: string; targetPinName?: string; timeoutMs?: number }): Promise<any>;
}

export interface ILevelTools {
    listLevels(): Promise<any>;
    getLevelSummary(levelPath?: string): Promise<any>;
    registerLight(levelPath: string | undefined, info: { name: string; type: string; details?: Record<string, unknown> }): void;
    exportLevel(params: { levelPath?: string; exportPath: string; note?: string }): Promise<any>;
    importLevel(params: { packagePath: string; destinationPath?: string; streaming?: boolean }): Promise<any>;
    saveLevelAs(params: { sourcePath?: string; targetPath: string }): Promise<any>;
    deleteLevels(params: { levelPaths: string[] }): Promise<any>;
    loadLevel(params: { levelPath: string; streaming?: boolean; position?: [number, number, number] }): Promise<any>;
    saveLevel(params: { levelName?: string; savePath?: string }): Promise<any>;
    createLevel(params: { levelName: string; template?: 'Empty' | 'Default' | 'VR' | 'TimeOfDay'; savePath?: string }): Promise<any>;
    streamLevel(params: { levelPath?: string; levelName?: string; shouldBeLoaded: boolean; shouldBeVisible?: boolean; position?: [number, number, number] }): Promise<any>;
    setupWorldComposition(params: { enableComposition: boolean; tileSize?: number; distanceStreaming?: boolean; streamingDistance?: number }): Promise<any>;
    editLevelBlueprint(params: { eventType: 'BeginPlay' | 'EndPlay' | 'Tick' | 'Custom'; customEventName?: string; nodes?: Array<{ nodeType: string; position: [number, number]; connections?: string[] }> }): Promise<any>;
    createSubLevel(params: { name: string; type: 'Persistent' | 'Streaming' | 'Lighting' | 'Gameplay'; parent?: string }): Promise<any>;
    setWorldSettings(params: { gravity?: number; worldScale?: number; gameMode?: string; defaultPawn?: string; killZ?: number }): Promise<any>;
    setLevelBounds(params: { min: [number, number, number]; max: [number, number, number] }): Promise<any>;
    buildNavMesh(params: { rebuildAll?: boolean; selectedOnly?: boolean }): Promise<any>;
    setLevelVisibility(params: { levelName: string; visible: boolean }): Promise<any>;
    setWorldOrigin(params: { location: [number, number, number] }): Promise<any>;
    createStreamingVolume(params: { levelName: string; position: [number, number, number]; size: [number, number, number]; streamingDistance?: number }): Promise<any>;
    setLevelLOD(params: { levelName: string; lodLevel: number; distance: number }): Promise<any>;
}

export interface IEditorTools {
    isInPIE(): Promise<boolean>;
    ensureNotInPIE(): Promise<void>;
    playInEditor(timeoutMs?: number): Promise<any>;
    stopPlayInEditor(): Promise<any>;
    pausePlayInEditor(): Promise<any>;
    pauseInEditor(): Promise<any>;
    buildLighting(): Promise<any>;
    setViewportCamera(location?: { x: number; y: number; z: number } | [number, number, number] | null | undefined, rotation?: { pitch: number; yaw: number; roll: number } | [number, number, number] | null | undefined): Promise<any>;
    setCameraSpeed(speed: number): Promise<any>;
    setFOV(fov: number): Promise<any>;
    takeScreenshot(filename?: string, resolution?: string): Promise<any>;
    resumePlayInEditor(): Promise<any>;
    stepPIEFrame(steps?: number): Promise<any>;
    startRecording(options?: { filename?: string; frameRate?: number; durationSeconds?: number; metadata?: Record<string, unknown> }): Promise<any>;
    stopRecording(): Promise<any>;
    createCameraBookmark(name: string): Promise<any>;
    jumpToCameraBookmark(name: string): Promise<any>;
    setEditorPreferences(category: string | undefined, preferences: Record<string, unknown>): Promise<any>;
    setViewportResolution(width: number, height: number): Promise<any>;
    executeConsoleCommand(command: string): Promise<any>;
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
    systemTools: any;
    uiTools: any;
    introspectionTools: any;
    audioTools: any;
    physicsTools: any;
    animationTools: any;
    automationBridge?: AutomationBridge;
    [key: string]: any;
}

export interface IEnvironmentTools {
    setTimeOfDay(hour: unknown): Promise<any>;
    setSunIntensity(intensity: unknown): Promise<any>;
    setSkylightIntensity(intensity: unknown): Promise<any>;
    exportSnapshot(params: { path?: unknown; filename?: unknown }): Promise<any>;
    importSnapshot(params: { path?: unknown; filename?: unknown }): Promise<any>;
    cleanup(params?: { names?: unknown }): Promise<any>;
}

export interface ILandscapeTools {
    createLandscape(params: { name: string; location?: [number, number, number]; sizeX?: number; sizeY?: number; quadsPerSection?: number; sectionsPerComponent?: number; componentCount?: number; materialPath?: string; enableWorldPartition?: boolean; runtimeGrid?: string; isSpatiallyLoaded?: boolean; dataLayers?: string[] }): Promise<any>;
    sculptLandscape(params: { landscapeName: string; tool: string; brushSize?: number; brushFalloff?: number; strength?: number; location?: [number, number, number]; radius?: number }): Promise<any>;
    paintLandscape(params: { landscapeName: string; layerName: string; position: [number, number, number]; brushSize?: number; strength?: number; targetValue?: number; radius?: number; density?: number }): Promise<any>;
    createProceduralTerrain(params: { name: string; location?: [number, number, number]; subdivisions?: number; heightFunction?: string; material?: string; settings?: Record<string, unknown> }): Promise<any>;
    createLandscapeGrassType(params: { name: string; meshPath: string; density?: number; minScale?: number; maxScale?: number; path?: string; staticMesh?: string }): Promise<any>;
    setLandscapeMaterial(params: { landscapeName: string; materialPath: string }): Promise<any>;
}

export interface IFoliageTools {
    addFoliageType(params: { name: string; meshPath: string; density?: number; radius?: number; minScale?: number; maxScale?: number; alignToNormal?: boolean; randomYaw?: boolean; groundSlope?: number }): Promise<any>;
    addFoliage(params: { foliageType: string; locations: Array<{ x: number; y: number; z: number }> }): Promise<any>;
    paintFoliage(params: { foliageType: string; position: [number, number, number]; brushSize?: number; paintDensity?: number; eraseMode?: boolean }): Promise<any>;
    createProceduralFoliage(params: { name: string; bounds?: { location: { x: number; y: number; z: number }; size: { x: number; y: number; z: number } }; foliageTypes?: Array<{ meshPath: string; density: number; minScale?: number; maxScale?: number; alignToNormal?: boolean; randomYaw?: boolean }>; volumeName?: string; position?: [number, number, number]; size?: [number, number, number]; seed?: number; tileSize?: number }): Promise<any>;
    addFoliageInstances(params: { foliageType: string; transforms: Array<{ location: [number, number, number]; rotation?: [number, number, number]; scale?: [number, number, number] }> }): Promise<any>;
    getFoliageInstances(params: { foliageType?: string }): Promise<any>;
    removeFoliage(params: { foliageType?: string; removeAll?: boolean }): Promise<any>;
    createInstancedMesh(params: { name: string; meshPath: string; instances: Array<{ position: [number, number, number]; rotation?: [number, number, number]; scale?: [number, number, number] }>; enableCulling?: boolean; cullDistance?: number }): Promise<any>;
    setFoliageLOD(params: { foliageType: string; lodDistances?: number[]; screenSize?: number[] }): Promise<any>;
    setFoliageCollision(params: { foliageType: string; collisionEnabled?: boolean; collisionProfile?: string; generateOverlapEvents?: boolean }): Promise<any>;
    createGrassSystem(params: { name: string; grassTypes: Array<{ meshPath: string; density: number; minScale?: number; maxScale?: number }>; windStrength?: number; windSpeed?: number }): Promise<any>;
    removeFoliageInstances(params: { foliageType: string; position: [number, number, number]; radius: number }): Promise<any>;
    selectFoliageInstances(params: { foliageType: string; position?: [number, number, number]; radius?: number; selectAll?: boolean }): Promise<any>;
    updateFoliageInstances(params: { foliageType: string; updateTransforms?: boolean; updateMesh?: boolean; newMeshPath?: string }): Promise<any>;
    createFoliageSpawner(params: { name: string; spawnArea: 'Landscape' | 'StaticMesh' | 'BSP' | 'Foliage' | 'All'; excludeAreas?: Array<[number, number, number, number]> }): Promise<any>;
    optimizeFoliage(params: { mergeInstances?: boolean; generateClusters?: boolean; clusterSize?: number; reduceDrawCalls?: boolean }): Promise<any>;
}
