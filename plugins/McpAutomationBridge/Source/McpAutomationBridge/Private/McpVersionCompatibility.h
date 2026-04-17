// =============================================================================
// McpVersionCompatibility.h
// =============================================================================
// UE 5.0 - 5.7+ API Compatibility Macros
//
// These macros abstract API differences between UE versions to allow the same
// code to compile across UE 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, and 5.7.
//
// REFACTORING NOTES:
// - Extracted from McpAutomationBridgeHelpers.h for better organization
// - Include this file FIRST before other engine includes
// - All version-specific APIs should use these macros
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "Misc/Crc.h"

// =============================================================================
// Default Feature Detection
// =============================================================================

// ControlRigBlueprintFactory availability
// Available in all UE 5.x versions, but header location varies
#ifndef MCP_HAS_CONTROLRIG_FACTORY
  #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    #define MCP_HAS_CONTROLRIG_FACTORY 1
  #else
    #define MCP_HAS_CONTROLRIG_FACTORY 0
  #endif
#endif

// =============================================================================
// Material API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: Material->Expressions (direct TArray access)
// UE 5.1+: Material->GetEditorOnlyData()->ExpressionCollection.Expressions

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  #define MCP_GET_MATERIAL_EXPRESSIONS(Material) \
    (Material)->GetEditorOnlyData()->ExpressionCollection.Expressions
  #define MCP_GET_FUNCTION_EXPRESSIONS(Function) \
    (Function)->GetEditorOnlyData()->ExpressionCollection.Expressions
  #define MCP_GET_MATERIAL_INPUT(Material, InputName) \
    (Material)->GetEditorOnlyData()->InputName
  #define MCP_HAS_MATERIAL_EDITOR_ONLY_DATA 1
#else
  #define MCP_GET_MATERIAL_EXPRESSIONS(Material) \
    (Material)->Expressions
  #define MCP_GET_FUNCTION_EXPRESSIONS(Function) \
    (Function)->FunctionExpressions
  #define MCP_GET_MATERIAL_INPUT(Material, InputName) \
    (Material)->InputName
  #define MCP_HAS_MATERIAL_EDITOR_ONLY_DATA 0
#endif

// =============================================================================
// DataLayer API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: UDataLayer (direct)
// UE 5.1+: UDataLayerInstance, UDataLayerAsset with FDataLayerCreationParameters

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  #define MCP_HAS_DATALAYER_INSTANCE 1
  #define MCP_HAS_DATALAYER_ASSET 1
  #define MCP_DATALAYER_TYPE UDataLayerInstance
  #define MCP_DATALAYER_ASSET_TYPE UDataLayerAsset
#else
  #define MCP_HAS_DATALAYER_INSTANCE 0
  #define MCP_HAS_DATALAYER_ASSET 0
  #define MCP_DATALAYER_TYPE UDataLayer
  #define MCP_DATALAYER_ASSET_TYPE UDataLayer
#endif

// =============================================================================
// FReferenceSkeletonModifier API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: Only Add(), UpdateRefPoseTransform(), FindBoneIndex()
// UE 5.1+: Also Remove(), SetParent()

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  #define MCP_HAS_REF_SKELETON_MODIFIER_REMOVE 1
  #define MCP_HAS_REF_SKELETON_MODIFIER_SETPARENT 1
#else
  #define MCP_HAS_REF_SKELETON_MODIFIER_REMOVE 0
  #define MCP_HAS_REF_SKELETON_MODIFIER_SETPARENT 0
#endif

// =============================================================================
// Niagara API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: FNiagaraEmitterHandle::GetInstance() returns UNiagaraEmitter*
// UE 5.1+: Returns FVersionedNiagaraEmitter

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  #define MCP_NIAGARA_EMITTER_DATA_TYPE FVersionedNiagaraEmitterData
  #define MCP_GET_NIAGARA_EMITTER_DATA(Handle) (Handle)->GetEmitterData()
  #define MCP_HAS_NIAGARA_VERSIONING 1
#else
  #define MCP_NIAGARA_EMITTER_DATA_TYPE UNiagaraEmitter
  #define MCP_GET_NIAGARA_EMITTER_DATA(Handle) (Handle)->GetInstance()
  #define MCP_HAS_NIAGARA_VERSIONING 0
#endif

// =============================================================================
// AssetRegistry API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: FARFilter uses ClassNames (TArray<FName>)
// UE 5.1+: FARFilter uses ClassPaths (TArray<FTopLevelAssetPath>)

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  #define MCP_ASSET_FILTER_CLASS_PATHS Filter.ClassPaths
  #define MCP_HAS_ASSET_CLASS_PATHS 1
  #define MCP_FTOP_LEVEL_ASSET_PATH FTopLevelAssetPath
#else
  #define MCP_ASSET_FILTER_CLASS_PATHS Filter.ClassNames
  #define MCP_HAS_ASSET_CLASS_PATHS 0
  #define MCP_FTOP_LEVEL_ASSET_PATH FName
#endif

// =============================================================================
// FAssetData API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: AssetClass (FName), no GetSoftObjectPath()
// UE 5.1+: AssetClassPath (FTopLevelAssetPath), GetSoftObjectPath()

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  #define MCP_ASSET_DATA_GET_CLASS_PATH(AssetData) (AssetData).AssetClassPath.ToString()
  #define MCP_ASSET_DATA_GET_SOFT_PATH(AssetData) (AssetData).GetSoftObjectPath().ToString()
  #define MCP_HAS_ASSET_SOFT_PATH 1
#else
  #define MCP_ASSET_DATA_GET_CLASS_PATH(AssetData) (AssetData).AssetClass.ToString()
  #define MCP_ASSET_DATA_GET_SOFT_PATH(AssetData) (AssetData).PackageName.ToString()
  #define MCP_HAS_ASSET_SOFT_PATH 0
#endif

// =============================================================================
// FProperty ExportText API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: ExportText_Direct() with different parameters
// UE 5.1+: ExportTextItem_Direct()

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  #define MCP_PROPERTY_EXPORT_TEXT(Property, OutText, ValuePtr, DefaultValuePtr, Container, Flags) \
    (Property)->ExportTextItem_Direct(OutText, ValuePtr, DefaultValuePtr, Container, Flags)
#else
  #define MCP_PROPERTY_EXPORT_TEXT(Property, OutText, ValuePtr, DefaultValuePtr, Container, Flags) \
    (Property)->ExportText_Direct(OutText, ValuePtr, DefaultValuePtr, Flags, Container)
#endif

// =============================================================================
// SmartObject API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: Different slot definition structure
// UE 5.1+: FSmartObjectSlotDefinition with bEnabled, ID, etc.

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  #define MCP_HAS_SMARTOBJECT_SLOT_ENABLED 1
  #define MCP_HAS_SMARTOBJECT_SLOT_ID 1
#else
  #define MCP_HAS_SMARTOBJECT_SLOT_ENABLED 0
  #define MCP_HAS_SMARTOBJECT_SLOT_ID 0
#endif

// =============================================================================
// Animation Data Controller API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: Different API for animation data controller
// UE 5.1+: SetNumberOfFrames(), IsValidBoneTrackName(), etc.

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  #define MCP_HAS_ANIM_DATA_CONTROLLER_SET_NUM_FRAMES 1
  #define MCP_HAS_ANIM_DATA_MODEL_VALID_BONE_TRACK 1
#else
  #define MCP_HAS_ANIM_DATA_CONTROLLER_SET_NUM_FRAMES 0
  #define MCP_HAS_ANIM_DATA_MODEL_VALID_BONE_TRACK 0
#endif

// =============================================================================
// HLOD Layer API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: UHLODLayer without SetIsSpatiallyLoaded(), SetLayerType()
// UE 5.1+: These methods exist

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  #define MCP_HAS_HLOD_SET_IS_SPATIALLY_LOADED 1
  #define MCP_HAS_HLOD_SET_LAYER_TYPE 1
#else
  #define MCP_HAS_HLOD_SET_IS_SPATIALLY_LOADED 0
  #define MCP_HAS_HLOD_SET_LAYER_TYPE 0
#endif

// =============================================================================
// Spatial Hash Runtime Grid API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: FSpatialHashRuntimeGrid without Origin
// UE 5.1+: Has Origin member

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  #define MCP_HAS_SPATIAL_HASH_RUNTIME_GRID_ORIGIN 1
#else
  #define MCP_HAS_SPATIAL_HASH_RUNTIME_GRID_ORIGIN 0
#endif

// =============================================================================
// K2Node Header Location Compatibility (UE 5.0 - 5.7)
// =============================================================================
// K2Node headers moved between engine versions:
// UE 5.0-5.3: K2Node_*.h at root level
// UE 5.4+: May be under BlueprintGraph/ or BlueprintGraph/Classes/

// This is handled in the source files with __has_include chains
// The MCP_HAS_K2NODE_HEADERS macro is set during include probing

// =============================================================================
// EdGraphSchema_K2 Compatibility (UE 5.0 - 5.7)
// =============================================================================

// Pin category constants - resolved at include time in source files
// MCP_PC_* macros are defined after including EdGraphSchema_K2.h

// =============================================================================
// SubobjectDataSubsystem API Compatibility (UE 5.1+)
// =============================================================================

// MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM is defined via build system or include probing
// Used for SCS (Simple Construction Script) operations

// =============================================================================
// FAssetCompilingManager API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: Only FinishAllCompilation() exists (no object-specific compilation)
// UE 5.1+: FinishCompilationForObjects(TArray<UObject*>) for selective compilation

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_HAS_FINISH_COMPILATION_FOR_OBJECTS 1
#define MCP_FINISH_COMPILATION_FOR_OBJECTS(Manager, Objects) (Manager).FinishCompilationForObjects(Objects)
#else
#define MCP_HAS_FINISH_COMPILATION_FOR_OBJECTS 0
// UE 5.0: Fall back to global compilation finish
#define MCP_FINISH_COMPILATION_FOR_OBJECTS(Manager, Objects) (Manager).FinishAllCompilation()
#endif

// =============================================================================
// UPackageTools API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// We always use the simple overload UnloadPackages(TArray<UPackage*>, FText&, bool)
// because it works reliably across all UE 5.x versions (5.0 through 5.7+).
// The FUnloadPackageParams struct is version‑unstable and removed in 5.7+.
#define MCP_HAS_UNLOAD_PACKAGE_PARAMS 0
// MCP_UNLOAD_PACKAGE_PARAMS_TYPE is not defined because it is never used.

// =============================================================================
// UWidgetBlueprint API Compatibility (UE 5.0 vs 5.1 vs 5.2 vs 5.3+)
// =============================================================================
// UE 5.0: No WidgetVariableNameToGuidMap - use UBlueprint::NewVariables
// UE 5.1: WidgetVariableNameToGuidMap exists
// UE 5.2: WidgetVariableNameToGuidMap does NOT exist (or is private)
// UE 5.3: WidgetVariableNameToGuidMap does NOT exist (not present in engine)
// UE 5.4+: Unknown; assume fallback to NewVariables for safety
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
    #define MCP_HAS_WIDGET_VARIABLE_GUID_MAP 1
    #define MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP) (WidgetBP)->WidgetVariableNameToGuidMap
#else
    // UE 5.0, 5.2, 5.3+: WidgetVariableNameToGuidMap does not exist.
    // The macro is never used because MCP_HAS_WIDGET_VARIABLE_GUID_MAP is 0,
    // but we define it to a no-op to avoid "macro redefined" warnings.
    #define MCP_HAS_WIDGET_VARIABLE_GUID_MAP 0
    #define MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP) (void)(WidgetBP)
#endif

// =============================================================================
// IKRig Editor API Compatibility (UE 5.0 vs 5.1 vs 5.2 vs 5.3+)
// =============================================================================
// UE 5.0: No CreateNewIKRigAsset, use NewObject; separate SetSourceIKRig/SetTargetIKRig
// UE 5.1: CreateNewIKRigAsset exists; SetIKRig with enum
// UE 5.2: CreateNewIKRigAsset DOES NOT exist (use NewObject); SetIKRig with enum exists
// UE 5.3+: CreateNewIKRigAsset DOES NOT exist (factory has no static method); SetIKRig with enum exists

// CreateNewIKRigAsset availability – only UE 5.1 has it
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 1
    #define MCP_HAS_IKRIG_CREATE_NEW_ASSET 1
#else
    #define MCP_HAS_IKRIG_CREATE_NEW_ASSET 0
#endif

// SetIKRig with enum availability (all UE 5.1+)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    #define MCP_HAS_IKRETARGETER_SET_IKRIG_ENUM 1
    #define MCP_IKRETARGETER_SET_SOURCE_IKRIG(Controller, Rig) (Controller)->SetIKRig(ERetargetSourceOrTarget::Source, Rig)
    #define MCP_IKRETARGETER_SET_TARGET_IKRIG(Controller, Rig) (Controller)->SetIKRig(ERetargetSourceOrTarget::Target, Rig)
#else
    #define MCP_HAS_IKRETARGETER_SET_IKRIG_ENUM 0
    // UE 5.0: use separate methods
    #define MCP_IKRETARGETER_SET_SOURCE_IKRIG(Controller, Rig) (Controller)->SetSourceIKRig(Rig)
    #define MCP_IKRETARGETER_SET_TARGET_IKRIG(Controller, Rig) (Controller)->SetTargetIKRig(Rig)
#endif

// CreateNewIKRigAsset macro (only when available)
#if MCP_HAS_IKRIG_CREATE_NEW_ASSET
    #define MCP_IKRIG_CREATE_NEW_ASSET(Path, Name) UIKRigDefinitionFactory::CreateNewIKRigAsset(Path, Name)
#else
    // No macro for creation – code will fall back to NewObject
#endif

// =============================================================================
// Deterministic GUID generator (for widget/anim variable GUIDs)
// =============================================================================
#ifndef MCP_NEW_DETERMINISTIC_GUID
    #define MCP_NEW_DETERMINISTIC_GUID(PathString) \
        [Path = FString(PathString)]() -> FGuid \
        { \
            uint32 Hash = FCrc::StrCrc32(*Path); \
            uint32 Hash2 = FCrc::StrCrc32(*FString::Printf(TEXT("%s_salt"), *Path)); \
            return FGuid(Hash, Hash2, Hash, Hash2); \
        }()
#endif

// =============================================================================
// FAssetData Object Path Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// UE 5.0: ObjectPath member (FName), no GetObjectPathString()
// UE 5.1+: GetObjectPathString() method

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#define MCP_ASSET_DATA_GET_OBJECT_PATH(AssetData) (AssetData).GetObjectPathString()
#else
#define MCP_ASSET_DATA_GET_OBJECT_PATH(AssetData) (AssetData).ObjectPath.ToString()
#endif