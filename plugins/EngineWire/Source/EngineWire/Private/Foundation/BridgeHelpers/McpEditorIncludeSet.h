#pragma once

// The editor and engine headers the Environment and Inspect domains build on, as one include set (it was the head of
// Domains/Environment/McpEnvShared.h, which Inspect included for it). The same headers, in the same order, on every engine version.
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Safety/McpSafeOperations.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "HAL/PlatformMemory.h"
#include "Misc/App.h"
#include "UObject/UnrealType.h"

#include <initializer_list>

// =============================================================================
// Editor-Only Includes
// =============================================================================
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Selection.h"

// Editor subsystems
#include "Subsystems/EditorActorSubsystem.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "LevelEditorSubsystem.h"

// =============================================================================
// Engine Component Includes
// =============================================================================
#include "Camera/CameraComponent.h"
#include "Components/ActorComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/ExponentialHeightFog.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/WindDirectionalSource.h"
#include "Materials/MaterialInterface.h"
#include "Particles/Emitter.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

// =============================================================================
// Editor & Asset Includes
// =============================================================================
#include "DynamicRHI.h"
#include "Engine/Blueprint.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "GeneralProjectSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/EngineVersion.h"
#include "Settings/LevelEditorViewportSettings.h"

// =============================================================================
// Procedural & Mesh Includes
// =============================================================================
#include "KismetProceduralMeshLibrary.h"
#include "Misc/FileHelper.h"
#include "ProceduralMeshComponent.h"

// =============================================================================
// Landscape Includes (for foliage dispatch)
// =============================================================================
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeGrassType.h"
#include "FoliageType.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"

class UTexture;

