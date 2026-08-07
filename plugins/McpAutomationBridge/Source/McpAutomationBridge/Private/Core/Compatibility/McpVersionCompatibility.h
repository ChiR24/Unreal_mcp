// =============================================================================
// McpVersionCompatibility.h
// =============================================================================
// UE 5.0 - 5.8+ API Compatibility Macros
//
// These macros abstract API differences between UE versions to allow the same
// code to compile across UE 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, and 5.8.
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

#ifndef MCP_HAS_CINEMATIC_CAMERA
#define MCP_HAS_CINEMATIC_CAMERA 0
#endif

#ifndef MCP_HAS_MEDIA_ASSETS
#define MCP_HAS_MEDIA_ASSETS 0
#endif

#ifndef MCP_HAS_MOVIE_RENDER_PIPELINE
#define MCP_HAS_MOVIE_RENDER_PIPELINE 0
#endif

#ifndef MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS
#define MCP_HAS_MOVIE_PIPELINE_OBJECT_ID_PASS 0
#endif

#ifndef MCP_HAS_MOVIE_PIPELINE_PASS_METADATA
#define MCP_HAS_MOVIE_PIPELINE_PASS_METADATA 0
#endif

#ifndef MCP_HAS_SMAA
#define MCP_HAS_SMAA 0
#endif

#ifndef MCP_HAS_TAKE_RECORDER
#define MCP_HAS_TAKE_RECORDER 0
#endif

#ifndef MCP_HAS_TAKE_RECORDER_OPEN_SEQUENCER
#define MCP_HAS_TAKE_RECORDER_OPEN_SEQUENCER 0
#endif

#ifndef MCP_HAS_REPLAY_API
#define MCP_HAS_REPLAY_API 0
#endif

#ifndef MCP_HAS_REPLAY_SUBSYSTEM_TOTAL_TIME
#define MCP_HAS_REPLAY_SUBSYSTEM_TOTAL_TIME 0
#endif

// MCP_DISALLOW_SHRINKING is passed as the bAllowShrinking argument to
// TArray::RemoveAt. On UE 5.6+ it is the EAllowShrinking enum
// (EAllowShrinking::No on the modern path). On older UE it falls back to
// `false`, which maps to the legacy `bool bAllowShrinking` parameter that
// existed before the enum was introduced. Both paths mean "do not shrink";
// the fallback value is intentionally the legacy bool, not the modern enum.
#if __has_include("Containers/AllowShrinking.h")
#include "Containers/AllowShrinking.h"
#define MCP_DISALLOW_SHRINKING EAllowShrinking::No
#else
#define MCP_DISALLOW_SHRINKING false
#endif

// =============================================================================
// StructUtils header relocation (UE 5.0-5.4 vs 5.5+)
// =============================================================================
// StructUtils merged into CoreUObject in 5.5 and the headers moved under a
// StructUtils/ prefix. The engine kept forwarding shims at the old paths behind
// UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_5, then DELETED them in 5.8:
//
//   UE 5.0-5.4  UUserDefinedStruct -> "Engine/UserDefinedStruct.h"
//               FInstancedStruct   -> "InstancedStruct.h"  (StructUtils plugin)
//   UE 5.5-5.7  both the new StructUtils/ paths and the old shims resolve
//   UE 5.8+     ONLY the StructUtils/ paths exist
//
// Five files included the StructUtils/ paths unconditionally, which cannot
// resolve on 5.0-5.4 — so the plugin could not compile there at all, despite
// claiming 5.0-5.8 support. Probing with __has_include rather than
// ENGINE_MINOR_VERSION keeps this correct if a path moves again, and mirrors
// the MCP_MOVIE_PIPELINE_CONFIG_HEADER pattern below.
#if __has_include("StructUtils/UserDefinedStruct.h")
#define MCP_USER_DEFINED_STRUCT_HEADER "StructUtils/UserDefinedStruct.h"
#else
#define MCP_USER_DEFINED_STRUCT_HEADER "Engine/UserDefinedStruct.h"
#endif

#if __has_include("StructUtils/InstancedStruct.h")
#define MCP_INSTANCED_STRUCT_HEADER "StructUtils/InstancedStruct.h"
#else
#define MCP_INSTANCED_STRUCT_HEADER "InstancedStruct.h"
#endif

// These macros are only defined when Movie Render Pipeline is actually enabled
// (MCP_HAS_MOVIE_RENDER_PIPELINE=1). When MRP is disabled, callers gate the
// related #include with the same flag, so the macros being undefined is safe.
// When MRP is enabled, the standard MRP config header must be present so the
// macros resolve to a real include path; otherwise we #error with a clear
// message instead of letting the broken #include produce a confusing error.
#if MCP_HAS_MOVIE_RENDER_PIPELINE
#if __has_include("MoviePipelinePrimaryConfig.h")
#define MCP_MOVIE_PIPELINE_CONFIG_HEADER "MoviePipelinePrimaryConfig.h"
#define MCP_MOVIE_PIPELINE_CONFIG_CLASS UMoviePipelinePrimaryConfig
#elif __has_include("MoviePipelineMasterConfig.h")
#define MCP_MOVIE_PIPELINE_CONFIG_HEADER "MoviePipelineMasterConfig.h"
#define MCP_MOVIE_PIPELINE_CONFIG_CLASS UMoviePipelineMasterConfig
#else
#error "MCP_HAS_MOVIE_RENDER_PIPELINE=1 but neither MoviePipelinePrimaryConfig.h nor MoviePipelineMasterConfig.h is present on the include path. Check that the Movie Render Pipeline module/plugin is installed."
#endif
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
#define MCP_GET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue) (Queue)->IsDirty()
#define MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, bDirty) \
  (Queue)->SetIsDirty(bDirty)
#else
#define MCP_GET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue) false
#define MCP_SET_MOVIE_PIPELINE_QUEUE_DIRTY(Queue, bDirty) \
  do {                                                   \
    (void)(Queue);                                       \
    (void)(bDirty);                                      \
  } while (false)
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
#define MCP_HAS_MOVIE_SCENE_SHOT_METADATA 1
#else
#define MCP_HAS_MOVIE_SCENE_SHOT_METADATA 0
#endif

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
    (Property)->ExportText_Direct(OutText, ValuePtr, DefaultValuePtr, Container, Flags, nullptr)
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
// K2Node Header Location Compatibility (UE 5.0 - 5.8)
// =============================================================================
// K2Node headers moved between engine versions:
// UE 5.0-5.3: K2Node_*.h at root level
// UE 5.4+: May be under BlueprintGraph/ or BlueprintGraph/Classes/

// This is handled in the source files with __has_include chains
// The MCP_HAS_K2NODE_HEADERS macro is set during include probing

// =============================================================================
// EdGraphSchema_K2 Compatibility (UE 5.0 - 5.8)
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
// Grepped out of Engine/Source/Runtime/Engine/Public/AssetCompilingManager.h at
// each release tag in EpicGames/UnrealEngine:
//   5.0.3 5.1.1 -> absent
//   5.2.1 5.3.2 5.4.4 5.5.4 5.6.1 -> PRESENT  (5.7 declares it at line 76)
// The boundary is 5.2, not 5.1. The old >= 1 guard made UE 5.1 call
// FinishCompilationForObjects, which does not exist there — a compile error on
// that version. 5.1 now takes the FinishAllCompilation() fallback with 5.0.
//
// UE 5.0-5.1: Only FinishAllCompilation() exists (no object-specific compilation)
// UE 5.2+: FinishCompilationForObjects(TArrayView<UObject* const>) for selective compilation

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
#define MCP_HAS_FINISH_COMPILATION_FOR_OBJECTS 1
#define MCP_FINISH_COMPILATION_FOR_OBJECTS(Manager, Objects) (Manager).FinishCompilationForObjects(Objects)
#else
#define MCP_HAS_FINISH_COMPILATION_FOR_OBJECTS 0
// UE 5.0-5.1: Fall back to global compilation finish
#define MCP_FINISH_COMPILATION_FOR_OBJECTS(Manager, Objects) (Manager).FinishAllCompilation()
#endif

// =============================================================================
// UPackageTools API Compatibility (UE 5.0 vs 5.1+)
// =============================================================================
// We always use the simple overload UnloadPackages(TArray<UPackage*>, FText&, bool)
// because it works reliably across all UE 5.x versions (5.0 through 5.8+).
// The FUnloadPackageParams struct is version‑unstable and removed in 5.7+.
#define MCP_HAS_UNLOAD_PACKAGE_PARAMS 0
// MCP_UNLOAD_PACKAGE_PARAMS_TYPE is not defined because it is never used.

// =============================================================================
// UWidgetBlueprint API Compatibility (UE 5.0 vs 5.1 vs 5.2-5.6 vs 5.7+)
// =============================================================================
// Grepped out of Engine/Source/Editor/UMGEditor/Public/WidgetBlueprint.h at each
// release tag in EpicGames/UnrealEngine:
//   5.0.3 5.1.0 5.1.1 5.2.1 5.3.2 5.4.4 5.5.4 -> absent
//   5.6.0 5.6.1 -> PRESENT   (5.7 / 5.8 confirmed in the installed engines)
// The member appears in 5.6.0 and never goes away, so the boundary is a plain
// >= 6. The previous guard claimed it existed in 5.1 and NOT in 5.6: the 5.1
// half was a COMPILE ERROR on that version (the member is not there), and the
// 5.6 half silently dropped the GUID registration on an engine that supports it.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
    #define MCP_HAS_WIDGET_VARIABLE_GUID_MAP 1
    #define MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP) (WidgetBP)->WidgetVariableNameToGuidMap
#else
    // UE 5.0-5.5: WidgetVariableNameToGuidMap does not exist.
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

// CreateNewIKRigAsset availability. Grepped out of
// Engine/Plugins/Animation/IKRig/Source/IKRigEditor/Public/RigEditor/
// IKRigDefinitionFactory.h at each release tag in EpicGames/UnrealEngine:
//   5.0.3 5.1.0 5.1.1 5.2.1 5.3.2 5.4.4 5.5.4 -> absent
//   5.6.0 5.6.1 -> PRESENT   (5.7 / 5.8 confirmed in the installed engines)
//     static UE_API UIKRigDefinition* CreateNewIKRigAsset(
//         const FString& InPackagePath, const FString& InAssetName);
// This read "only UE 5.1 has it", which was wrong in BOTH directions. 5.1 does
// not have it, so that branch was a compile error on the one version it claimed
// to serve. And every engine from 5.6 on does have it, so the guard fell back to
// NewObject there — a path that creates the package and object but never calls
// FAssetRegistryModule::AssetCreated, leaving a rig made through create_ik_rig
// unregistered and missing from the Content Browser until a rescan.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
    #define MCP_HAS_IKRIG_CREATE_NEW_ASSET 1
#else
    #define MCP_HAS_IKRIG_CREATE_NEW_ASSET 0
#endif

// SetIKRig with enum availability. Grepped out of
// Engine/Plugins/Animation/IKRig/Source/IKRigEditor/Public/RetargetEditor/
// IKRetargeterController.h at each release tag in EpicGames/UnrealEngine:
//   5.0.3 5.1.1 -> SetSourceIKRig/SetTargetIKRig present, SetIKRig absent
//   5.2.1 5.3.2 5.4.4 5.5.4 5.6.1 -> SetIKRig present, the pair removed
// The swap happens in 5.2, not 5.1. The old >= 1 guard made UE 5.1 call
// SetIKRig(ERetargetSourceOrTarget, ...) — which is not there on that version —
// while the SetSourceIKRig/SetTargetIKRig pair it should have used was.
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
    #define MCP_HAS_IKRETARGETER_SET_IKRIG_ENUM 1
    #define MCP_IKRETARGETER_SET_SOURCE_IKRIG(Controller, Rig) (Controller)->SetIKRig(ERetargetSourceOrTarget::Source, Rig)
    #define MCP_IKRETARGETER_SET_TARGET_IKRIG(Controller, Rig) (Controller)->SetIKRig(ERetargetSourceOrTarget::Target, Rig)
#else
    #define MCP_HAS_IKRETARGETER_SET_IKRIG_ENUM 0
    // UE 5.0-5.1: use separate methods
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

// =============================================================================
// UUserDefinedEnum::SetEnums Compatibility (UE 5.8 signature change)
// =============================================================================
// UE 5.8: UUserDefinedEnum overrides SetEnums with a 5-parameter signature
//         (Names, CppForm, UnderlyingType, Flags, AddMaxKeyIfMissing). The
//         2/4-argument UEnum overload is deprecated AND final, and the override
//         hides it, so a 2-argument call no longer compiles.
// UE 5.0-5.7: the 2-argument UEnum::SetEnums(Names, CppForm) is the way.
//
// On 5.8 the underlying type is read back with GetUnderlyingType() so the
// enum keeps its own type. Note this is deliberately NOT what the deprecated
// 2-argument overload does -- that one forwards EUnderlyingType::int64
// unconditionally. Reading the type back matches Engine's own
// FEnumEditorUtils, which is the behaviour you want when editing an existing
// user-defined enum (creating one uses uint8, so int64 would rewrite it).

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8
  #define MCP_SET_ENUMS(EnumPtr, Names, CppForm)                                \
    (EnumPtr)->SetEnums((Names), (CppForm), (EnumPtr)->GetUnderlyingType(),     \
                        EEnumFlags::None, UEnum::EAddMaxKeyIfMissing::Yes)
#else
  #define MCP_SET_ENUMS(EnumPtr, Names, CppForm)                                \
    (EnumPtr)->SetEnums((Names), (CppForm))
#endif
