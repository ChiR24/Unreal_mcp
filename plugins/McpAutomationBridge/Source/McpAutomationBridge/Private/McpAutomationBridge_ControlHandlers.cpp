// =============================================================================
// McpAutomationBridge_ControlHandlers.cpp
// =============================================================================
// Editor control, viewport, PIE, camera, and actor manipulation handlers.
//
// HANDLERS (64 actions):
//   Editor Control:
//     - start_pie, stop_pie, pause_pie, resume_pie, is_pie_active
//     - get_pie_info, set_pie_mode
//
//   Camera & Viewport:
//     - set_camera_view, get_camera_view, focus_viewport_on_actor
//     - set_viewport_view_mode, get_viewport_screenshot, capture_screenshot
//     - set_viewport_layout, get_active_viewport_info
//
//   Actor Control:
//     - spawn_actor, delete_actor, duplicate_actor, get_actors
//     - set_actor_transform, get_actor_transform, set_actor_location
//     - set_actor_rotation, set_actor_scale, apply_physics
//     - set_actor_tags, add_actor_tag, remove_actor_tag
//     - attach_actor, detach_actor, teleport_actor
//
//   Component Operations:
//     - add_component, remove_component, get_components
//     - set_component_property, get_component_property
//
//   Selection:
//     - select_actor, deselect_actor, clear_selection, get_selected_actors
//     - select_components, get_selected_components
//
//   Debug:
//     - draw_debug_line, draw_debug_sphere, draw_debug_box
//     - draw_debug_arrow, clear_debug_drawings
//
// REFACTORING NOTES:
//   - Uses McpVersionCompatibility.h for UE 5.0-5.7 API abstraction
//   - Uses McpHandlerUtils for standardized JSON parsing/responses
//   - Editor subsystems paths vary by UE version
//   - LevelEditor module is optional (may not be available in some contexts)
//
// VERSION COMPATIBILITY:
//   - EditorActorSubsystem: Path varies (Subsystems/ vs root)
//   - LevelEditorSubsystem: UE 5.0+ (optional, conditional include)
//   - UnrealEditorSubsystem: UE 5.0+ (optional, conditional include)
//   - LevelEditorPlaySettings: UE 5.0+ (optional, conditional include)
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpVersionCompatibility.h"
#include "Dom/JsonObject.h"
#include "Async/Async.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpLandscapeMetadataTags.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR

// -----------------------------------------------------------------------------
// Editor-only Includes: Asset & Engine Utilities
// -----------------------------------------------------------------------------
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Editor Subsystems (paths vary by UE version)
// -----------------------------------------------------------------------------
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#if __has_include("Subsystems/UnrealEditorSubsystem.h")
#include "Subsystems/UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#elif __has_include("UnrealEditorSubsystem.h")
#include "UnrealEditorSubsystem.h"
#define MCP_HAS_UNREALEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/LevelEditorSubsystem.h")
#include "Subsystems/LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#elif __has_include("LevelEditorSubsystem.h")
#include "LevelEditorSubsystem.h"
#define MCP_HAS_LEVELEDITOR_SUBSYSTEM 1
#endif
#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#elif __has_include("AssetEditorSubsystem.h")
#include "AssetEditorSubsystem.h"
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Viewport Control
// -----------------------------------------------------------------------------
#include "Components/LightComponent.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "IAssetViewport.h"  // For IAssetViewport::GetAssetViewportClient()

// -----------------------------------------------------------------------------
// Editor-only Includes: Level Editor (optional, may not be available)
// -----------------------------------------------------------------------------
#if __has_include("LevelEditor.h")
#include "LevelEditor.h"
#define MCP_HAS_LEVEL_EDITOR_MODULE 1
#else
#define MCP_HAS_LEVEL_EDITOR_MODULE 0
#endif
#if __has_include("Settings/LevelEditorPlaySettings.h")
#include "Settings/LevelEditorPlaySettings.h"
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 1
#else
#define MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS 0
#endif

// -----------------------------------------------------------------------------
// Editor-only Includes: Components & Actors
// -----------------------------------------------------------------------------
#include "Components/PrimitiveComponent.h"
#include "EditorViewportClient.h"
#include "Engine/Blueprint.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"

#if __has_include("FileHelpers.h")
#include "FileHelpers.h"
#endif
#include "Animation/SkeletalMeshActor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

// -----------------------------------------------------------------------------
// Editor-only Includes: Export & Output
// -----------------------------------------------------------------------------
#include "Exporters/Exporter.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "UnrealClient.h" // For FScreenshotRequest
#include "Widgets/SWindow.h"

#endif // WITH_EDITOR

#if WITH_EDITOR
namespace {
constexpr int32 MaxScreenshotPngBytesForBase64ForMcp = 3 * 1024 * 1024;

bool IsSafeConsoleArgumentToken(const FString &Value) {
  const FString Trimmed = Value.TrimStartAndEnd();
  return !Trimmed.IsEmpty() && !Trimmed.Contains(TEXT("\n")) &&
         !Trimmed.Contains(TEXT("\r")) && !Trimmed.Contains(TEXT("&&")) &&
         !Trimmed.Contains(TEXT("||")) && !Trimmed.Contains(TEXT(";")) &&
         !Trimmed.Contains(TEXT("|")) && !Trimmed.Contains(TEXT("`")) &&
         !Trimmed.Contains(TEXT(" ")) && !Trimmed.Contains(TEXT("\t"));
}

FString MakeSafeConsoleName(const FString &RawName, const TCHAR *Prefix) {
  FString CleanName = FPaths::GetBaseFilename(
      FPaths::GetCleanFilename(RawName.TrimStartAndEnd()));
  if (!IsSafeConsoleArgumentToken(CleanName)) {
    CleanName = FString::Printf(
        TEXT("%s_%s"), Prefix,
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }
  return CleanName;
}

FString MakeSafeScreenshotFilenameForMcp(
    const TSharedPtr<FJsonObject> &Payload) {
  FString Filename;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("filename"), Filename);
  }

  if (Filename.IsEmpty()) {
    Filename = FString::Printf(TEXT("Screenshot_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }

  Filename = FPaths::GetCleanFilename(Filename);
  if (Filename.Contains(TEXT("..")) || Filename.Contains(TEXT("/")) ||
      Filename.Contains(TEXT("\\"))) {
    Filename = FString::Printf(TEXT("Screenshot_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  }

  if (!Filename.EndsWith(TEXT(".png"))) {
    Filename += TEXT(".png");
  }

  return Filename;
}

void AddScreenshotMetadataForMcp(const TSharedPtr<FJsonObject> &Resp,
                                 const TSharedPtr<FJsonObject> &Payload) {
  if (!Resp.IsValid() || !Payload.IsValid()) {
    return;
  }

  bool bIncludeMetadata = false;
  if (!Payload->TryGetBoolField(TEXT("includeMetadata"), bIncludeMetadata) ||
      !bIncludeMetadata) {
    return;
  }

  const TSharedPtr<FJsonObject> *Metadata = nullptr;
  if (Payload->TryGetObjectField(TEXT("metadata"), Metadata) && Metadata &&
      Metadata->IsValid()) {
    Resp->SetObjectField(TEXT("metadata"), *Metadata);
  }
}

FString MakeScreenshotTooLargeMessageForMcp(int32 SizeBytes) {
  return FString::Printf(
      TEXT("Screenshot PNG is too large to return as base64 (%d bytes, max %d bytes). Retry with returnBase64=false or a smaller viewport/window."),
      SizeBytes, MaxScreenshotPngBytesForBase64ForMcp);
}

bool IsUsableSlateWindowForMcp(const TSharedPtr<SWindow> &Window) {
  return Window.IsValid() && Window->IsVisible() && !Window->IsWindowMinimized();
}

TSharedPtr<SWindow> GetFullEditorSlateWindowForMcp() {
  if (!FSlateApplication::IsInitialized() ||
      !FSlateApplication::Get().CanDisplayWindows()) {
    return nullptr;
  }

  TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
  if (IsUsableSlateWindowForMcp(RootWindow)) {
    return RootWindow;
  }

#if MCP_HAS_LEVEL_EDITOR_MODULE
  if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
    if (FLevelEditorModule* LevelEditorModule =
            FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"))) {
      TSharedPtr<IAssetViewport> ActiveViewport =
          LevelEditorModule->GetFirstActiveViewport();
      if (ActiveViewport.IsValid()) {
        TSharedPtr<SWindow> ViewportWindow =
            FSlateApplication::Get().FindWidgetWindow(ActiveViewport->AsWidget());
        if (IsUsableSlateWindowForMcp(ViewportWindow)) {
          return ViewportWindow;
        }
      }
    }
  }
#endif

  TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
  if (IsUsableSlateWindowForMcp(ActiveWindow)) {
    return ActiveWindow;
  }

  return nullptr;
}

bool CaptureSlateWindowPngForMcp(const TSharedRef<SWindow> &Window,
                                 TArray<uint8> &OutPngData,
                                 FIntVector &OutSize,
                                 FString &OutError) {
  TSharedRef<SWidget> WindowWidget = Window;
  TArray<FColor> Bitmap;

  FSlateApplication::Get().ForceRedrawWindow(Window);
  if (!FSlateApplication::Get().TakeScreenshot(WindowWidget, Bitmap, OutSize) ||
      Bitmap.Num() == 0 || OutSize.X <= 0 || OutSize.Y <= 0) {
    OutError = TEXT("Failed to capture Slate window pixels");
    return false;
  }

  for (FColor &Pixel : Bitmap) {
    Pixel.A = 255;
  }

  IImageWrapperModule &ImageWrapperModule =
      FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
  TSharedPtr<IImageWrapper> ImageWrapper =
      ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
  if (!ImageWrapper.IsValid()) {
    OutError = TEXT("Failed to create PNG image wrapper");
    return false;
  }

  TArray<uint8> RawData;
  RawData.SetNumUninitialized(Bitmap.Num() * 4);
  for (int32 PixelIndex = 0; PixelIndex < Bitmap.Num(); ++PixelIndex) {
    const FColor &Pixel = Bitmap[PixelIndex];
    RawData[PixelIndex * 4 + 0] = Pixel.R;
    RawData[PixelIndex * 4 + 1] = Pixel.G;
    RawData[PixelIndex * 4 + 2] = Pixel.B;
    RawData[PixelIndex * 4 + 3] = Pixel.A;
  }

  if (!ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), OutSize.X,
                            OutSize.Y, ERGBFormat::RGBA, 8)) {
    OutError = TEXT("Failed to prepare Slate window screenshot pixels for PNG encoding");
    return false;
  }
  OutPngData = ImageWrapper->GetCompressed(100);

  if (OutPngData.Num() == 0) {
    OutError = TEXT("Failed to encode Slate window screenshot as PNG");
    return false;
  }

  return true;
}

FEditorViewportClient* GetActiveEditorViewportClientForMcp() {
#if MCP_HAS_LEVEL_EDITOR_MODULE
  if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor"))) {
    if (FLevelEditorModule* LevelEditorModule =
            FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor"))) {
      TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule->GetFirstActiveViewport();
      if (ActiveViewport.IsValid()) {
        return &ActiveViewport->GetAssetViewportClient();
      }
    }
  }
#endif

  if (GEditor && GEditor->GetActiveViewport()) {
    return static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
  }

  return nullptr;
}
} // namespace
#endif

// Helper class for capturing export output
/* UE5.6: Use built-in FStringOutputDevice from UnrealString.h */

// Helper functions
// (ExtractVectorField and ExtractRotatorField moved to
// McpAutomationBridgeHelpers.h)

AActor *UMcpAutomationBridgeSubsystem::FindActorByName(const FString &Target, bool bExactMatchOnly) {
#if WITH_EDITOR
  if (Target.IsEmpty() || !GEditor)
    return nullptr;

  // Priority: PIE World if active
  if (GEditor->PlayWorld) {
    for (TActorIterator<AActor> It(GEditor->PlayWorld); It; ++It) {
      AActor *A = *It;
      if (!A)
        continue;
      if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
          A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
          A->GetPathName().Equals(Target, ESearchCase::IgnoreCase)) {
        return A;
      }
    }
    // If not found in PIE, do we fall back to Editor World?
    // Probably not, because interacting with Editor world during PIE is
    // confusing. But for "Editor subsystems" usage, we usually want Editor
    // world. Let's fallback if not found, just in case.
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  if (!ActorSS)
    return nullptr;

  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  AActor *ExactMatch = nullptr;
  TArray<AActor *> FuzzyMatches;

  for (AActor *A : AllActors) {
    if (!A)
      continue;
    if (A->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
        A->GetName().Equals(Target, ESearchCase::IgnoreCase) ||
        A->GetPathName().Equals(Target, ESearchCase::IgnoreCase)) {
      ExactMatch = A;
      break;
    }
    // Collect fuzzy matches ONLY if exact matching is not required
    // CRITICAL FIX: Fuzzy matching can cause delete operations to delete wrong actors
    // (e.g., "TestActor_Copy" matches when searching for "TestActor")
    if (!bExactMatchOnly && A->GetActorLabel().Contains(Target, ESearchCase::IgnoreCase)) {
      FuzzyMatches.Add(A);
    }
  }

  if (ExactMatch) {
    return ExactMatch;
  }

  // If no exact match, check fuzzy matches ONLY if exact matching is not required
  if (!bExactMatchOnly) {
    if (FuzzyMatches.Num() == 1) {
      return FuzzyMatches[0];
    } else if (FuzzyMatches.Num() > 1) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("FindActorByName: Ambiguous match for '%s'. Found %d matches."),
             *Target, FuzzyMatches.Num());
    }
  }

  // Fallback: try to load as asset if it looks like a path
  if (Target.StartsWith(TEXT("/"))) {
    const FString SafeTargetPath = SanitizeProjectRelativePath(Target);
    if (!SafeTargetPath.IsEmpty()) {
      if (UObject *Obj = UEditorAssetLibrary::LoadAsset(SafeTargetPath)) {
        return Cast<AActor>(Obj);
      }
    }
  }
#endif
  return nullptr;
}

/**
 * Spawn a native actor from a class or mesh path and apply the requested transform.
 *
 * The handler accepts optional `location`, `rotation`, and `scale` payload fields,
 * applies scale only when explicitly provided, and returns the actual actor scale
 * so callers can verify the resulting transform without an additional query.
 */
bool UMcpAutomationBridgeSubsystem::HandleControlActorSpawn(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ClassPath;
  Payload->TryGetStringField(TEXT("classPath"), ClassPath);
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
  const bool bHasScale = Payload->HasField(TEXT("scale"));
  const FVector Scale =
      ExtractVectorField(Payload, TEXT("scale"), FVector::OneVector);

  UClass *ResolvedClass = nullptr;
  FString MeshPath;
  Payload->TryGetStringField(TEXT("meshPath"), MeshPath);
  UStaticMesh *ResolvedStaticMesh = nullptr;
  USkeletalMesh *ResolvedSkeletalMesh = nullptr;

  // Skip LoadAsset for script classes (e.g. /Script/Engine.CameraActor) to
  // avoid LogEditorAssetSubsystem errors
  if ((ClassPath.StartsWith(TEXT("/")) || ClassPath.Contains(TEXT("/"))) &&
      !ClassPath.StartsWith(TEXT("/Script/"))) {
    const FString SafeClassPath = SanitizeProjectRelativePath(ClassPath);
    if (!SafeClassPath.IsEmpty()) {
      if (UObject *Loaded = UEditorAssetLibrary::LoadAsset(SafeClassPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
          ResolvedClass = BP->GeneratedClass;
        else if (UClass *C = Cast<UClass>(Loaded))
          ResolvedClass = C;
        else if (UStaticMesh *Mesh = Cast<UStaticMesh>(Loaded))
          ResolvedStaticMesh = Mesh;
        else if (USkeletalMesh *SkelMesh = Cast<USkeletalMesh>(Loaded))
          ResolvedSkeletalMesh = SkelMesh;
      }
    }
  }
  if (!ResolvedClass && !ResolvedStaticMesh && !ResolvedSkeletalMesh)
    ResolvedClass = ResolveClassByName(ClassPath);

  // If explicit mesh path provided for a general spawn request
  if (!ResolvedStaticMesh && !ResolvedSkeletalMesh && !MeshPath.IsEmpty()) {
    const FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
    if (!SafeMeshPath.IsEmpty()) {
      if (UObject *MeshObj = UEditorAssetLibrary::LoadAsset(SafeMeshPath)) {
        ResolvedStaticMesh = Cast<UStaticMesh>(MeshObj);
        if (!ResolvedStaticMesh)
          ResolvedSkeletalMesh = Cast<USkeletalMesh>(MeshObj);
      }
    }
  }

  // Force StaticMeshActor if we have a resolved mesh, regardless of class input
  // (unless it's a specific subclass)
  bool bSpawnStaticMeshActor = (ResolvedStaticMesh != nullptr);
  bool bSpawnSkeletalMeshActor = (ResolvedSkeletalMesh != nullptr);

  if (!bSpawnStaticMeshActor && !bSpawnSkeletalMeshActor && ResolvedClass) {
    bSpawnStaticMeshActor =
        ResolvedClass->IsChildOf(AStaticMeshActor::StaticClass());
    if (!bSpawnStaticMeshActor)
      bSpawnSkeletalMeshActor =
          ResolvedClass->IsChildOf(ASkeletalMeshActor::StaticClass());
  }

  // Explicitly use StaticMeshActor class if we have a mesh but no class, or if
  // we decided to spawn a static mesh actor
  if (bSpawnStaticMeshActor && !ResolvedClass) {
    ResolvedClass = AStaticMeshActor::StaticClass();
  } else if (bSpawnSkeletalMeshActor && !ResolvedClass) {
    ResolvedClass = ASkeletalMeshActor::StaticClass();
  }

  if (!ResolvedClass && !bSpawnStaticMeshActor && !bSpawnSkeletalMeshActor) {
    const FString ErrorMsg =
        FString::Printf(TEXT("Class not found: %s. Verify plugin is enabled if "
                             "using a plugin class."),
                        *ClassPath);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              ErrorMsg);
    return true;
  }

  AActor *Spawned = nullptr;

  // Use direct world spawning for both PIE and editor worlds. EditorActorSubsystem
  // routes through viewport placement and hit-proxy rendering, which crashes
  // under NullRHI even when automation provides an explicit transform.
  UWorld *TargetWorld = nullptr;
  if (GEditor->PlayWorld) {
    TargetWorld = GEditor->PlayWorld.Get();
  } else {
    TargetWorld = GEditor->GetEditorWorldContext().World();
  }

  if (!TargetWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("WORLD_NOT_FOUND"),
                              TEXT("Editor world not available"));
    return true;
  }

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
  SpawnParams.ObjectFlags |= RF_Transactional;
  if (!GEditor->PlayWorld) {
    SpawnParams.OverrideLevel = TargetWorld->GetCurrentLevel();
    TargetWorld->Modify();
  }

  UClass *ClassToSpawn =
      ResolvedClass
          ? ResolvedClass
          : (bSpawnStaticMeshActor ? AStaticMeshActor::StaticClass()
                                   : (bSpawnSkeletalMeshActor
                                          ? ASkeletalMeshActor::StaticClass()
                                          : AActor::StaticClass()));
  Spawned = TargetWorld->SpawnActor(ClassToSpawn, &Location, &Rotation,
                                    SpawnParams);

  if (Spawned) {
    Spawned->Modify();
    Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                         ETeleportType::TeleportPhysics);
    if (bSpawnStaticMeshActor) {
      if (AStaticMeshActor *StaticMeshActor = Cast<AStaticMeshActor>(Spawned)) {
        if (UStaticMeshComponent *MeshComponent =
                StaticMeshActor->GetStaticMeshComponent()) {
          if (ResolvedStaticMesh) {
            MeshComponent->SetStaticMesh(ResolvedStaticMesh);
          }
          MeshComponent->SetMobility(EComponentMobility::Movable);
          MeshComponent->MarkRenderStateDirty();
        }
      }
    } else if (bSpawnSkeletalMeshActor) {
      if (ASkeletalMeshActor *SkelActor = Cast<ASkeletalMeshActor>(Spawned)) {
        if (USkeletalMeshComponent *SkelComp =
                SkelActor->GetSkeletalMeshComponent()) {
          if (ResolvedSkeletalMesh) {
            SkelComp->SetSkeletalMesh(ResolvedSkeletalMesh);
          }
          SkelComp->SetMobility(EComponentMobility::Movable);
          SkelComp->MarkRenderStateDirty();
        }
      }
    }
  }

  if (!Spawned) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SPAWN_FAILED"),
                              TEXT("Failed to spawn actor"));

    return true;
  }

  if (bHasScale) {
    Spawned->SetActorScale3D(Scale);
  }

  if (!ActorName.IsEmpty()) {
    Spawned->SetActorLabel(ActorName);
  } else {
    // Auto-generate a friendly label from the mesh or class name
    FString BaseName;
    if (ResolvedStaticMesh) {
      BaseName = ResolvedStaticMesh->GetName();
    } else if (ResolvedSkeletalMesh) {
      BaseName = ResolvedSkeletalMesh->GetName();
    } else if (ResolvedClass) {
      BaseName = ResolvedClass->GetName();
      if (BaseName.EndsWith(TEXT("_C"))) {
        BaseName.RemoveFromEnd(TEXT("_C"));
      }
    } else {
      BaseName = TEXT("Actor");
    }
    Spawned->SetActorLabel(BaseName);
  }

  // Build response matching the outputWithActor schema:
  // { actor: { id, name, path }, actorPath, classPath?, meshPath? }
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();

  // Actor object with id, name and path
  TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
  ActorObj->SetStringField(TEXT("id"), Spawned->GetPathName());  // Use path as unique ID
  ActorObj->SetStringField(TEXT("name"), Spawned->GetActorLabel());
  ActorObj->SetStringField(TEXT("path"), Spawned->GetPathName());
  Data->SetObjectField(TEXT("actor"), ActorObj);

  // actorPath for convenience
  Data->SetStringField(TEXT("actorPath"), Spawned->GetPathName());

  // Provide the resolved class path useful for referencing
  if (ResolvedClass)
    Data->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
  else
    Data->SetStringField(TEXT("classPath"), ClassPath);

  if (ResolvedStaticMesh)
    Data->SetStringField(TEXT("meshPath"), ResolvedStaticMesh->GetPathName());
  else if (ResolvedSkeletalMesh)
    Data->SetStringField(TEXT("meshPath"), ResolvedSkeletalMesh->GetPathName());

  auto MakeVectorArray = [](const FVector &Vec) -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Values;
  };
  Data->SetArrayField(TEXT("scale"), MakeVectorArray(Spawned->GetActorScale3D()));

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Spawned);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Actor spawned"), Data);
  return true;

#else
  return false;
#endif
}

/**
 * Spawn an actor from a Blueprint class and apply the requested transform fields.
 *
 * Blueprint spawning mirrors the regular actor spawn path by accepting optional
 * `location`, `rotation`, and `scale`, then returning the applied scale in the
 * response payload for client-side verification.
 */
bool UMcpAutomationBridgeSubsystem::HandleControlActorSpawnBlueprint(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString BlueprintPath;
  Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
  if (BlueprintPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("Blueprint path required"), nullptr);
    return true;
  }

  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
  const bool bHasScale = Payload->HasField(TEXT("scale"));
  const FVector Scale =
      ExtractVectorField(Payload, TEXT("scale"), FVector::OneVector);

  UClass *ResolvedClass = nullptr;

  // Prefer the same blueprint resolution heuristics used by manage_blueprint
  // so that short names and package paths behave consistently.
  FString NormalizedPath;
  FString LoadError;
  if (!BlueprintPath.IsEmpty()) {
    UBlueprint *BlueprintAsset =
        LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
    if (BlueprintAsset && BlueprintAsset->GeneratedClass) {
      ResolvedClass = BlueprintAsset->GeneratedClass;
    }
  }

  if (!ResolvedClass && (BlueprintPath.StartsWith(TEXT("/")) ||
                         BlueprintPath.Contains(TEXT("/")))) {
    const FString SafeBlueprintPath = SanitizeProjectRelativePath(BlueprintPath);
    if (!SafeBlueprintPath.IsEmpty()) {
      if (UObject *Loaded = UEditorAssetLibrary::LoadAsset(SafeBlueprintPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
          ResolvedClass = BP->GeneratedClass;
        else if (UClass *C = Cast<UClass>(Loaded))
          ResolvedClass = C;
      }
    }
  }
  if (!ResolvedClass)
    ResolvedClass = ResolveClassByName(BlueprintPath);

  if (!ResolvedClass) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              TEXT("Blueprint class not found"), nullptr);
    return true;
  }

  AActor *Spawned = nullptr;
  UWorld *TargetWorld = nullptr;
  if (GEditor->PlayWorld) {
    TargetWorld = GEditor->PlayWorld.Get();
  } else {
    TargetWorld = GEditor->GetEditorWorldContext().World();
  }

  if (!TargetWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("WORLD_NOT_FOUND"),
                              TEXT("Editor world not available"), nullptr);
    return true;
  }

  FActorSpawnParameters SpawnParams;
  SpawnParams.SpawnCollisionHandlingOverride =
      ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
  SpawnParams.ObjectFlags |= RF_Transactional;
  if (!GEditor->PlayWorld) {
    SpawnParams.OverrideLevel = TargetWorld->GetCurrentLevel();
    TargetWorld->Modify();
  }
  Spawned = TargetWorld->SpawnActor(ResolvedClass, &Location, &Rotation,
                                    SpawnParams);
  if (Spawned) {
    Spawned->Modify();
    Spawned->SetActorLocationAndRotation(Location, Rotation, false, nullptr,
                                         ETeleportType::TeleportPhysics);
  }

  if (!Spawned) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SPAWN_FAILED"),
                              TEXT("Failed to spawn blueprint"), nullptr);
    return true;
  }

  if (bHasScale) {
    Spawned->SetActorScale3D(Scale);
  }

  if (!ActorName.IsEmpty())
    Spawned->SetActorLabel(ActorName);

  // Build response matching the outputWithActor schema:
  // { actor: { id, name, path }, actorPath, classPath }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();

  // Actor object with id, name and path
  TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
  ActorObj->SetStringField(TEXT("id"), Spawned->GetPathName());  // Use path as unique ID
  ActorObj->SetStringField(TEXT("name"), Spawned->GetActorLabel());
  ActorObj->SetStringField(TEXT("path"), Spawned->GetPathName());
  Resp->SetObjectField(TEXT("actor"), ActorObj);

  // actorPath for convenience
  Resp->SetStringField(TEXT("actorPath"), Spawned->GetPathName());
  Resp->SetStringField(TEXT("classPath"), ResolvedClass->GetPathName());
  auto MakeVectorArray = [](const FVector &Vec) -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Values.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Values;
  };
  Resp->SetArrayField(TEXT("scale"), MakeVectorArray(Spawned->GetActorScale3D()));

  // Add verification data
	McpHandlerUtils::AddVerification(Resp, Spawned);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Blueprint spawned"),
                         Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDelete(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TArray<FString> Targets;
  const TArray<TSharedPtr<FJsonValue>> *NamesArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("actorNames"), NamesArray) && NamesArray) {
    for (const TSharedPtr<FJsonValue> &Entry : *NamesArray) {
      if (Entry.IsValid() && Entry->Type == EJson::String) {
        const FString Value = Entry->AsString().TrimStartAndEnd();
        if (!Value.IsEmpty())
          Targets.AddUnique(Value);
      }
    }
  }

  FString SingleName;
  if (Targets.Num() == 0) {
    Payload->TryGetStringField(TEXT("actorName"), SingleName);
    if (!SingleName.IsEmpty())
      Targets.AddUnique(SingleName);
  }

  if (Targets.Num() == 0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName or actorNames required"));
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<FString> Deleted;
  TArray<FString> Missing;

  for (const FString &Name : Targets) {
    // CRITICAL FIX: Use exact match only for delete operations to prevent
    // fuzzy matching from deleting wrong actors (e.g., "TestActor_Copy" when
    // searching for "TestActor")
    AActor *Found = FindActorByName(Name, true);
    if (!Found) {
      Missing.Add(Name);
      continue;
    }
	if (ActorSS->DestroyActor(Found)) {
			Deleted.Add(Name);
    } else
      Missing.Add(Name);
  }

  const bool bAllDeleted = Missing.Num() == 0;
  const bool bAnyDeleted = Deleted.Num() > 0;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bAllDeleted);
  Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Name : Deleted)
    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
  Resp->SetArrayField(TEXT("deleted"), DeletedArray);

  if (Missing.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> MissingArray;
    for (const FString &Name : Missing)
      MissingArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("missing"), MissingArray);
  }

  FString Message;
  FString ErrorCode;
  if (!bAnyDeleted && Missing.Num() > 0) {
    Message = TEXT("Actors not found");
    ErrorCode = TEXT("NOT_FOUND");
  } else {
    Message = bAllDeleted ? TEXT("Actors deleted")
                          : TEXT("Some actors could not be deleted");
    ErrorCode = bAllDeleted ? FString() : TEXT("DELETE_PARTIAL");
  }

  // Add verification data for delete operations
  Resp->SetBoolField(TEXT("existsAfter"), false);
  Resp->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));

  if (!bAllDeleted && Missing.Num() > 0 && !bAnyDeleted) {
    SendStandardErrorResponse(this, Socket, RequestId, ErrorCode, Message);
  } else {
    SendStandardSuccessResponse(this, Socket, RequestId, Message, Resp);
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorApplyForce(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FVector ForceVector =
      ExtractVectorField(Payload, TEXT("force"), FVector::ZeroVector);

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  UPrimitiveComponent *Prim =
      Found->FindComponentByClass<UPrimitiveComponent>();
  if (!Prim) {
    if (UStaticMeshComponent *SMC =
            Found->FindComponentByClass<UStaticMeshComponent>())
      Prim = SMC;
  }

  if (!Prim) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_COMPONENT"),
                              TEXT("No component to apply force"), nullptr);
    return true;
  }

  if (Prim->Mobility == EComponentMobility::Static)
    Prim->SetMobility(EComponentMobility::Movable);

  // Ensure collision is enabled for physics
  if (Prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision) {
    Prim->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
  }

  // Check if collision geometry exists (common failure for empty
  // StaticMeshActors)
  if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(Prim)) {
    if (!SMC->GetStaticMesh()) {
      SendStandardErrorResponse(
          this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
          TEXT("StaticMeshComponent has no StaticMesh assigned."), nullptr);
      return true;
    }
    if (!SMC->GetStaticMesh()->GetBodySetup()) {
      SendStandardErrorResponse(
          this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
          TEXT("StaticMesh has no collision geometry (BodySetup is null)."),
          nullptr);
      return true;
    }
  }

  if (!Prim->IsSimulatingPhysics()) {
    Prim->SetSimulatePhysics(true);
    // Must recreate physics state for the body to be properly initialized in
    // Editor
    Prim->RecreatePhysicsState();
  }

  Prim->AddForce(ForceVector);
  Prim->WakeAllRigidBodies();
  Prim->MarkRenderStateDirty();

  // Verify physics state
  const bool bIsSimulating = Prim->IsSimulatingPhysics();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("simulating"), bIsSimulating);
  TArray<TSharedPtr<FJsonValue>> Applied;
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.X));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Y));
  Applied.Add(MakeShared<FJsonValueNumber>(ForceVector.Z));
  Data->SetArrayField(TEXT("applied"), Applied);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

  if (!bIsSimulating) {
    FString FailureReason = TEXT("Failed to enable physics simulation.");
    if (Prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision) {
      FailureReason += TEXT(" Collision is disabled.");
    } else if (Prim->Mobility != EComponentMobility::Movable) {
      FailureReason += TEXT(" Component is not Movable.");
    }
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("PHYSICS_FAILED"),
                              FailureReason, Data);
    return true;
  }

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Force applied"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetTransform(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  FVector Location =
      ExtractVectorField(Payload, TEXT("location"), Found->GetActorLocation());
  FRotator Rotation =
      ExtractRotatorField(Payload, TEXT("rotation"), Found->GetActorRotation());
  FVector Scale =
      ExtractVectorField(Payload, TEXT("scale"), Found->GetActorScale3D());

  Found->Modify();
  Found->SetActorLocation(Location, false, nullptr,
                          ETeleportType::TeleportPhysics);
  Found->SetActorRotation(Rotation, ETeleportType::TeleportPhysics);
  Found->SetActorScale3D(Scale);
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  // Verify transform
  const FVector NewLoc = Found->GetActorLocation();
  const FRotator NewRot = Found->GetActorRotation();
  const FVector NewScale = Found->GetActorScale3D();

  const bool bLocMatch = NewLoc.Equals(Location, 1.0f); // 1 unit tolerance
  // Rotation comparison is tricky due to normalization, skipping strict check
  // for now but logging if very different
  const bool bScaleMatch = NewScale.Equals(Scale, 0.01f);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("location"), MakeArray(NewLoc));
  Data->SetArrayField(TEXT("scale"), MakeArray(NewScale));

  if (!bLocMatch || !bScaleMatch) {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("TRANSFORM_MISMATCH"),
                              TEXT("Failed to set transform exactly"), Data);
    return true;
  }

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Actor transform updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetTransform(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"));
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"));
    return true;
  }

  const FTransform Current = Found->GetActorTransform();
  const FVector Location = Current.GetLocation();
  const FRotator Rotation = Current.GetRotation().Rotator();
  const FVector Scale = Current.GetScale3D();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("location"), MakeArray(Location));
  TArray<TSharedPtr<FJsonValue>> RotArray;
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Pitch));
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Yaw));
  RotArray.Add(MakeShared<FJsonValueNumber>(Rotation.Roll));
  Data->SetArrayField(TEXT("rotation"), RotArray);
  Data->SetArrayField(TEXT("scale"), MakeArray(Scale));

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actor transform retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetVisibility(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  bool bVisible = true;
  if (Payload->HasField(TEXT("visible")))
    Payload->TryGetBoolField(TEXT("visible"), bVisible);

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  Found->Modify();
  Found->SetActorHiddenInGame(!bVisible);
  Found->SetActorEnableCollision(bVisible);

  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp)
      continue;
    if (UPrimitiveComponent *Prim = Cast<UPrimitiveComponent>(Comp)) {
      Prim->SetVisibility(bVisible, true);
      Prim->SetHiddenInGame(!bVisible);
    }
  }

  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  // Verify visibility state
  const bool bIsHidden = Found->IsHidden();
  const bool bStateMatches = (bIsHidden == !bVisible);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("visible"), !bIsHidden);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());

  if (!bStateMatches) {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("VISIBILITY_MISMATCH"),
                              TEXT("Failed to set actor visibility"), Data);
    return true;
  }

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Actor visibility updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAddComponent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  FString ComponentType;
  Payload->TryGetStringField(TEXT("componentType"), ComponentType);
  if (ComponentType.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("componentType required"), nullptr);
    return true;
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  UClass *ComponentClass = ResolveClassByName(ComponentType);
  if (!ComponentClass ||
      !ComponentClass->IsChildOf(UActorComponent::StaticClass())) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CLASS_NOT_FOUND"),
                              TEXT("Component class not found"), nullptr);
    return true;
  }

  if (ComponentName.TrimStartAndEnd().IsEmpty())
    ComponentName = FString::Printf(TEXT("%s_%d"), *ComponentClass->GetName(),
                                    FMath::Rand());

  FName DesiredName = FName(*ComponentName);
  UActorComponent *NewComponent = NewObject<UActorComponent>(
      Found, ComponentClass, DesiredName, RF_Transactional);
  if (!NewComponent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CREATE_COMPONENT_FAILED"),
                              TEXT("Failed to create component"), nullptr);
    return true;
  }

  Found->Modify();
  NewComponent->SetFlags(RF_Transactional);
  Found->AddInstanceComponent(NewComponent);
  NewComponent->OnComponentCreated();

  if (USceneComponent *SceneComp = Cast<USceneComponent>(NewComponent)) {
    if (Found->GetRootComponent() && !SceneComp->GetAttachParent()) {
      SceneComp->SetupAttachment(Found->GetRootComponent());
    }
  }

  // Force lights to be movable to ensure they work without baking (Issue #6
  // fix) We check for "LightComponent" class name to avoid dependency issues if
  // header is obscure, but ULightComponent is standard.
  if (NewComponent->IsA(ULightComponent::StaticClass())) {
    if (USceneComponent *SC = Cast<USceneComponent>(NewComponent)) {
      SC->SetMobility(EComponentMobility::Movable);
    }
  }

  // Special handling for StaticMeshComponent meshPath convenience
  if (UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(NewComponent)) {
    FString MeshPath;
    if (Payload->TryGetStringField(TEXT("meshPath"), MeshPath) &&
        !MeshPath.IsEmpty()) {
      const FString SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
      if (!SafeMeshPath.IsEmpty()) {
        if (UObject *LoadedMesh = UEditorAssetLibrary::LoadAsset(SafeMeshPath)) {
          if (UStaticMesh *Mesh = Cast<UStaticMesh>(LoadedMesh)) {
            SMC->SetStaticMesh(Mesh);
          }
        }
      }
    }
  }

  TArray<FString> AppliedProperties;
  TArray<FString> PropertyWarnings;
  const TSharedPtr<FJsonObject> *PropertiesPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) &&
      PropertiesPtr && (*PropertiesPtr).IsValid()) {
    for (const auto &Pair : (*PropertiesPtr)->Values) {
      const FString PropertyName(*Pair.Key);
      FProperty *Property = ComponentClass->FindPropertyByName(*PropertyName);
      if (!Property) {
        PropertyWarnings.Add(
            FString::Printf(TEXT("Property not found: %s"), *PropertyName));
        continue;
      }
      FString ApplyError;
      if (ApplyJsonValueToProperty(NewComponent, Property, Pair.Value,
                                   ApplyError))
        AppliedProperties.Add(PropertyName);
      else
        PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"),
                                             *PropertyName, *ApplyError));
    }
  }

  NewComponent->RegisterComponent();
  if (USceneComponent *SceneComp = Cast<USceneComponent>(NewComponent))
    SceneComp->UpdateComponentToWorld();
  NewComponent->MarkPackageDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("componentName"), NewComponent->GetName());
  Resp->SetStringField(TEXT("componentPath"), NewComponent->GetPathName());
  Resp->SetStringField(TEXT("componentClass"), ComponentClass->GetPathName());
  if (AppliedProperties.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString &PropName : AppliedProperties)
      PropsArray.Add(MakeShared<FJsonValueString>(PropName));
    Resp->SetArrayField(TEXT("appliedProperties"), PropsArray);
  }
  if (PropertyWarnings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> WarnArray;
    for (const FString &Warning : PropertyWarnings)
      WarnArray.Add(MakeShared<FJsonValueString>(Warning));
    Resp->SetArrayField(TEXT("warnings"), WarnArray);
	}
	SendAutomationResponse(Socket, RequestId, true, TEXT("Component added"), Resp,
                         FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetComponentProperties(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  if (ComponentName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("componentName required"), nullptr);
    return true;
  }

  TSharedPtr<FJsonObject> PropertiesObject;
  const TSharedPtr<FJsonObject> *PropertiesPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("properties"), PropertiesPtr) &&
      PropertiesPtr && PropertiesPtr->IsValid()) {
    PropertiesObject = *PropertiesPtr;
  } else {
    FString PropertyName;
    Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
    if (PropertyName.IsEmpty()) {
      Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
    }
    const TSharedPtr<FJsonValue> ValueField = Payload->TryGetField(TEXT("value"));
    if (PropertyName.IsEmpty() || !ValueField.IsValid()) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                                TEXT("properties object or propertyName/propertyPath and value required"), nullptr);
      return true;
    }
    PropertiesObject = MakeShared<FJsonObject>();
    PropertiesObject->SetField(PropertyName, ValueField);
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  // CRITICAL FIX: Use FindComponentByName helper which supports fuzzy matching
  UActorComponent *TargetComponent = FindComponentByName(Found, ComponentName);

  if (!TargetComponent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("COMPONENT_NOT_FOUND"),
                              TEXT("Component not found"), nullptr);
    return true;
  }

  TArray<FString> AppliedProperties;
  TArray<FString> PropertyWarnings;
  UClass *ComponentClass = TargetComponent->GetClass();
  TargetComponent->Modify();

  // PRIORITY: Apply Mobility FIRST.
  // Physics simulation fails if the component is generic "Static".
  // Scan for Mobility key case-insensitively to ensure we find it regardless of
  // JSON casing
  const TSharedPtr<FJsonValue> *MobilityVal = nullptr;
  FString MobilityKey;
  for (const auto &Pair : PropertiesObject->Values) {
    const FString PropertyName(*Pair.Key);
    if (PropertyName.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase)) {
      MobilityVal = &Pair.Value;
      MobilityKey = PropertyName;
      break;
    }
  }

  if (MobilityVal) {
    if (USceneComponent *SC = Cast<USceneComponent>(TargetComponent)) {
      FString EnumVal;
      if ((*MobilityVal)->TryGetString(EnumVal)) {
        // Parse enum string
        int64 Val =
            StaticEnum<EComponentMobility::Type>()->GetValueByNameString(
                EnumVal);
        if (Val != INDEX_NONE) {
          SC->SetMobility((EComponentMobility::Type)Val);
		AppliedProperties.Add(MobilityKey);
		}
	} else {
		double Val;
		if ((*MobilityVal)->TryGetNumber(Val)) {
			SC->SetMobility((EComponentMobility::Type)(int32)Val);
			AppliedProperties.Add(MobilityKey);
		}
      }
    }
  }

  for (const auto &Pair : PropertiesObject->Values) {
    const FString PropertyName(*Pair.Key);
    // Skip Mobility as we already handled it
    if (PropertyName.Equals(TEXT("Mobility"), ESearchCase::IgnoreCase))
      continue;

    // Special handling for SimulatePhysics
    if (PropertyName.Equals(TEXT("SimulatePhysics"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("bSimulatePhysics"), ESearchCase::IgnoreCase)) {
      if (UPrimitiveComponent *Prim =
              Cast<UPrimitiveComponent>(TargetComponent)) {
        bool bVal = false;
        if (Pair.Value->TryGetBool(bVal)) {
			Prim->SetSimulatePhysics(bVal);
				AppliedProperties.Add(PropertyName);
				continue;
        }
      }
    }

    FProperty *Property = ComponentClass->FindPropertyByName(*PropertyName);
    if (!Property) {
      PropertyWarnings.Add(
          FString::Printf(TEXT("Property not found: %s"), *PropertyName));
      continue;
    }
    FString ApplyError;
    if (ApplyJsonValueToProperty(TargetComponent, Property, Pair.Value,
                                 ApplyError))
      AppliedProperties.Add(PropertyName);
    else
      PropertyWarnings.Add(FString::Printf(TEXT("Failed to set %s: %s"),
                                           *PropertyName, *ApplyError));
  }

  if (USceneComponent *SceneComponent =
          Cast<USceneComponent>(TargetComponent)) {
    SceneComponent->MarkRenderStateDirty();
    SceneComponent->UpdateComponentToWorld();
  }
  TargetComponent->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  if (AppliedProperties.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> PropsArray;
    for (const FString &PropName : AppliedProperties)
      PropsArray.Add(MakeShared<FJsonValueString>(PropName));
    Data->SetArrayField(TEXT("applied"), PropsArray);
  }

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Component properties updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetComponents(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);

  // Also accept "objectPath" as an alias, common in inspections
  if (TargetName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("objectPath"), TargetName);
  }

  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName or objectPath required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  // Fallback: Check if it's a Blueprint asset to inspect CDO components
  if (!Found) {
    const FString SafeTargetPath = SanitizeProjectRelativePath(TargetName);
    if (!SafeTargetPath.IsEmpty()) {
      if (UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeTargetPath)) {
        if (UBlueprint *BP = Cast<UBlueprint>(Asset)) {
          if (BP->GeneratedClass) {
            Found = Cast<AActor>(BP->GeneratedClass->GetDefaultObject());
          }
        }
      }
    }
  }

  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor or Blueprint not found"), nullptr);
    return true;
  }

  TArray<TSharedPtr<FJsonValue>> ComponentsArray;
  for (UActorComponent *Comp : Found->GetComponents()) {
    if (!Comp)
      continue;
    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("name"), Comp->GetName());
    Entry->SetStringField(TEXT("class"), Comp->GetClass()
                                             ? Comp->GetClass()->GetPathName()
                                             : TEXT(""));
    Entry->SetStringField(TEXT("path"), Comp->GetPathName());
    if (USceneComponent *SceneComp = Cast<USceneComponent>(Comp)) {
      FVector Loc = SceneComp->GetRelativeLocation();
      FRotator Rot = SceneComp->GetRelativeRotation();
      FVector Scale = SceneComp->GetRelativeScale3D();

      TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
      LocObj->SetNumberField(TEXT("x"), Loc.X);
      LocObj->SetNumberField(TEXT("y"), Loc.Y);
      LocObj->SetNumberField(TEXT("z"), Loc.Z);
      Entry->SetObjectField(TEXT("relativeLocation"), LocObj);

      TSharedPtr<FJsonObject> RotObj = McpHandlerUtils::CreateResultObject();
      RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
      RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
      RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
      Entry->SetObjectField(TEXT("relativeRotation"), RotObj);

      TSharedPtr<FJsonObject> ScaleObj = McpHandlerUtils::CreateResultObject();
      ScaleObj->SetNumberField(TEXT("x"), Scale.X);
      ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
      ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
      Entry->SetObjectField(TEXT("relativeScale"), ScaleObj);
    }
    ComponentsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("components"), ComponentsArray);
  Data->SetNumberField(TEXT("count"), ComponentsArray.Num());

  // Add verification data
  if (Found) {
    McpHandlerUtils::AddVerification(Data, Found);
  }

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Actor components retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDuplicate(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  FVector Offset =
      ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  AActor *Duplicated =
      ActorSS->DuplicateActor(Found, Found->GetWorld(), Offset);
  if (!Duplicated) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("DUPLICATE_FAILED"),
                              TEXT("Failed to duplicate actor"), nullptr);
    return true;
  }

  FString NewName;
  Payload->TryGetStringField(TEXT("newName"), NewName);
  if (!NewName.TrimStartAndEnd().IsEmpty())
    Duplicated->SetActorLabel(NewName);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("source"), Found->GetActorLabel());
  Data->SetStringField(TEXT("actorName"), Duplicated->GetActorLabel());
  Data->SetStringField(TEXT("actorPath"), Duplicated->GetPathName());

  // Add verification data
  McpHandlerUtils::AddVerification(Data, Duplicated);

  TArray<TSharedPtr<FJsonValue>> OffsetArray;
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.X));
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Y));
  OffsetArray.Add(MakeShared<FJsonValueNumber>(Offset.Z));
  Data->SetArrayField(TEXT("offset"), OffsetArray);

	SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor duplicated"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAttach(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ChildName;
  Payload->TryGetStringField(TEXT("childActor"), ChildName);
  FString ParentName;
  Payload->TryGetStringField(TEXT("parentActor"), ParentName);
  if (ChildName.IsEmpty() || ParentName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("childActor and parentActor required"), nullptr);
    return true;
  }

  AActor *Child = FindActorByName(ChildName);
  AActor *Parent = FindActorByName(ParentName);
  if (!Child || !Parent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Child or parent actor not found"), nullptr);
    return true;
  }

  if (Child == Parent) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("CYCLE_DETECTED"),
                              TEXT("Cannot attach actor to itself"), nullptr);
    return true;
  }

  USceneComponent *ChildRoot = Child->GetRootComponent();
  USceneComponent *ParentRoot = Parent->GetRootComponent();
  if (!ChildRoot || !ParentRoot) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ROOT_MISSING"),
                              TEXT("Actor missing root component"), nullptr);
    return true;
  }

  Child->Modify();
  ChildRoot->Modify();
  ChildRoot->AttachToComponent(ParentRoot,
                               FAttachmentTransformRules::KeepWorldTransform);
  Child->SetOwner(Parent);
  Child->MarkPackageDirty();
  Parent->MarkPackageDirty();

  // Verify attachment
  bool bAttached = false;
  if (Child->GetRootComponent() &&
      Child->GetRootComponent()->GetAttachParent() == ParentRoot) {
    bAttached = true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("child"), Child->GetActorLabel());
  Data->SetStringField(TEXT("parent"), Parent->GetActorLabel());
  Data->SetBoolField(TEXT("attached"), bAttached);

  if (!bAttached) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ATTACH_FAILED"),
                              TEXT("Failed to attach actor"), Data);
    return true;
  }

  // Add verification data for the child actor
	McpHandlerUtils::AddVerification(Data, Child);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Actor attached"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDetach(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  USceneComponent *RootComp = Found->GetRootComponent();
  if (!RootComp || !RootComp->GetAttachParent()) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("note"), TEXT("Actor was not attached"));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Actor already detached"), Resp, FString());
    return true;
  }

  Found->Modify();
  RootComp->Modify();
  RootComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
  Found->SetOwner(nullptr);
  Found->MarkPackageDirty();

  // Verify detachment
  const bool bDetached = (RootComp->GetAttachParent() == nullptr);

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetBoolField(TEXT("detached"), bDetached);

  if (!bDetached) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("DETACH_FAILED"),
                              TEXT("Failed to detach actor"), Data);
    return true;
  }

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Actor detached"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TagValue.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("tag required"), nullptr);
    return true;
  }

  // Security: Validate tag format - reject path traversal attempts
  if (TagValue.Contains(TEXT("..")) || TagValue.Contains(TEXT("\\")) || TagValue.Contains(TEXT("/"))) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid tag: '%s'. Path separators and traversal characters are not allowed."), *TagValue), nullptr);
    return true;
  }

  FString MatchType;
  Payload->TryGetStringField(TEXT("matchType"), MatchType);
  MatchType = MatchType.ToLower();
  FName TagName(*TagValue);
  TArray<TSharedPtr<FJsonValue>> Matches;

  // Log tag search details
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleControlActorFindByTag: Searching for tag '%s' (FName: %s)"),
         *TagValue, *TagName.ToString());
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();


  // Log total actors being searched
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleControlActorFindByTag: Searching %d actors in level"), AllActors.Num());
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    bool bMatches = false;
    if (MatchType == TEXT("contains")) {
      for (const FName &Existing : Actor->Tags) {
        if (Existing.ToString().Contains(TagValue, ESearchCase::IgnoreCase)) {
          bMatches = true;
          break;
        }
      }
    } else {
      bMatches = Actor->ActorHasTag(TagName);
    }

    // Log actor tags for troubleshooting at verbose level
    if (Actor->Tags.Num() > 0) {
      FString TagList;
      for (const FName& T : Actor->Tags) {
        TagList += T.ToString() + TEXT(", ");
      }
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("HandleControlActorFindByTag: Actor '%s' has tags: [%s] - match=%d"),
             *Actor->GetActorLabel(), *TagList, bMatches);
    }
    if (bMatches) {
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("name"), Actor->GetActorLabel());
      Entry->SetStringField(TEXT("path"), Actor->GetPathName());
      Entry->SetStringField(TEXT("class"),
                            Actor->GetClass() ? Actor->GetClass()->GetPathName()
                                              : TEXT(""));
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetNumberField(TEXT("count"), Matches.Num());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actors found"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAddTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TargetName.IsEmpty() || TagValue.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName and tag required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FName TagName(*TagValue);
  const bool bAlreadyHad = Found->Tags.Contains(TagName);

  Found->Modify();
  Found->Tags.AddUnique(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasPresent"), bAlreadyHad);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("tag"), TagName.ToString());

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Tag applied to actor"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByName(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Query;
  Payload->TryGetStringField(TEXT("name"), Query);
  if (Query.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("name required"), nullptr);
    return true;
  }

  // Security: Validate query format - reject path traversal attempts
  if (Query.Contains(TEXT("..")) || Query.Contains(TEXT("\\")) || Query.Contains(TEXT("/"))) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid name query: '%s'. Path separators and traversal characters are not allowed."), *Query), nullptr);
    return true;
  }

  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  const TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  TArray<TSharedPtr<FJsonValue>> Matches;
  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    const FString Label = Actor->GetActorLabel();
    const FString Name = Actor->GetName();
    const FString Path = Actor->GetPathName();
    const bool bMatches = Label.Contains(Query, ESearchCase::IgnoreCase) ||
                          Name.Contains(Query, ESearchCase::IgnoreCase) ||
                          Path.Contains(Query, ESearchCase::IgnoreCase);
    if (bMatches) {
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("label"), Label);
      Entry->SetStringField(TEXT("name"), Name);
      Entry->SetStringField(TEXT("path"), Path);
      Entry->SetStringField(TEXT("class"),
                            Actor->GetClass() ? Actor->GetClass()->GetPathName()
                                              : TEXT(""));
      Matches.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetNumberField(TEXT("count"), Matches.Num());
  Data->SetArrayField(TEXT("actors"), Matches);
  Data->SetStringField(TEXT("query"), Query);
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actor query executed"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDeleteByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TagValue.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("tag required"), nullptr);
    return true;
  }

  const FName TagName(*TagValue);
  UEditorActorSubsystem *ActorSS =
      GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
  const TArray<AActor *> AllActors = ActorSS->GetAllLevelActors();
  TArray<FString> Deleted;

  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    if (Actor->ActorHasTag(TagName)) {
      const FString Label = Actor->GetActorLabel();
      if (ActorSS->DestroyActor(Actor))
        Deleted.Add(Label);
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("tag"), TagName.ToString());
  Data->SetNumberField(TEXT("deletedCount"), Deleted.Num());
  TArray<TSharedPtr<FJsonValue>> DeletedArray;
  for (const FString &Name : Deleted)
    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
  Data->SetArrayField(TEXT("deleted"), DeletedArray);

  // Add verification data for delete operations
  Data->SetBoolField(TEXT("existsAfter"), false);
  Data->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Actors deleted by tag"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetBlueprintVariables(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  const TSharedPtr<FJsonObject> *VariablesPtr = nullptr;
  if (!(Payload->TryGetObjectField(TEXT("variables"), VariablesPtr) &&
        VariablesPtr && VariablesPtr->IsValid())) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("variables object required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  UClass *ActorClass = Found->GetClass();
  Found->Modify();
  TArray<FString> Applied;
  TArray<FString> Warnings;

  for (const auto &Pair : (*VariablesPtr)->Values) {
    const FString VariableName(*Pair.Key);
    FProperty *Property = ActorClass->FindPropertyByName(*VariableName);
    if (!Property) {
      Warnings.Add(FString::Printf(TEXT("Property not found: %s"), *VariableName));
      continue;
    }

    FString ApplyError;
    if (ApplyJsonValueToProperty(Found, Property, Pair.Value, ApplyError))
      Applied.Add(VariableName);
    else
      Warnings.Add(FString::Printf(TEXT("Failed to set %s: %s"), *VariableName,
                                   *ApplyError));
  }

  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  if (Applied.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> AppliedArray;
    for (const FString &Name : Applied)
      AppliedArray.Add(MakeShared<FJsonValueString>(Name));
    Data->SetArrayField(TEXT("updated"), AppliedArray);
  }

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Variables updated"), Data, Warnings);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorCreateSnapshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  FString SnapshotName;
  Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
  if (SnapshotName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("snapshotName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FString SnapshotKey =
      FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
  CachedActorSnapshots.Add(SnapshotKey, Found->GetActorTransform());

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Snapshot created"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRestoreSnapshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  FString SnapshotName;
  Payload->TryGetStringField(TEXT("snapshotName"), SnapshotName);
  if (SnapshotName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("snapshotName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FString SnapshotKey =
      FString::Printf(TEXT("%s::%s"), *Found->GetPathName(), *SnapshotName);
  if (!CachedActorSnapshots.Contains(SnapshotKey)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SNAPSHOT_NOT_FOUND"),
                              TEXT("Snapshot not found"), nullptr);
    return true;
  }

  const FTransform &SavedTransform = CachedActorSnapshots[SnapshotKey];
  Found->Modify();
  Found->SetActorTransform(SavedTransform);
  Found->MarkComponentsRenderStateDirty();
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("snapshotName"), SnapshotName);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Snapshot restored"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorExport(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  FMcpOutputCapture OutputCapture;
  UExporter::ExportToOutputDevice(nullptr, Found, nullptr, OutputCapture,
                                  TEXT("T3D"), 0, 0, false);
  FString OutputString = FString::Join(OutputCapture.Consume(), TEXT("\n"));

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("t3d"), OutputString);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor exported"),
                              Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetBoundingBox(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  FVector Origin, BoxExtent;
  Found->GetActorBounds(false, Origin, BoxExtent);

  auto BuildLandscapeBox = [](const FTransform& Transform, double MinX,
                              double MinY, double MaxX, double MaxY,
                              double MinZ, double MaxZ) {
    FBox LandscapeBox(EForceInit::ForceInit);
    const FVector Corners[] = {
        FVector(MinX, MinY, MinZ), FVector(MinX, MaxY, MinZ),
        FVector(MaxX, MinY, MinZ), FVector(MaxX, MaxY, MinZ),
        FVector(MinX, MinY, MaxZ), FVector(MinX, MaxY, MaxZ),
        FVector(MaxX, MinY, MaxZ), FVector(MaxX, MaxY, MaxZ),
    };
    for (const FVector& Corner : Corners) {
      LandscapeBox += Transform.TransformPosition(Corner);
    }
    return LandscapeBox;
  };

  if (ALandscape* Landscape = Cast<ALandscape>(Found);
      Landscape && BoxExtent.IsNearlyZero()) {
    ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    if (LandscapeInfo && LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)) {
      const FBox LandscapeBox = BuildLandscapeBox(
          Landscape->GetTransform(), MinX, MinY, MaxX, MaxY, -256.0, 256.0);
      Origin = LandscapeBox.GetCenter();
      BoxExtent = LandscapeBox.GetExtent();
    }

    if (BoxExtent.IsNearlyZero()) {
      const FBox CompleteBounds = Landscape->GetCompleteBounds();
      if (CompleteBounds.IsValid) {
        Origin = CompleteBounds.GetCenter();
        BoxExtent = CompleteBounds.GetExtent();
      }
    }

    if (BoxExtent.IsNearlyZero()) {
      FMcpLandscapeMetadata Metadata;
      if (McpLandscapeMetadataTags::DecodeLandscapeMetadata(Landscape, Metadata)) {
        const double MaxXFromMetadata = Metadata.ComponentsX * Metadata.QuadsPerComponent;
        const double MaxYFromMetadata = Metadata.ComponentsY * Metadata.QuadsPerComponent;
        const FBox LandscapeBox = BuildLandscapeBox(
            Landscape->GetTransform(), 0.0, 0.0, MaxXFromMetadata,
            MaxYFromMetadata, -256.0, 256.0);
        Origin = LandscapeBox.GetCenter();
        BoxExtent = LandscapeBox.GetExtent();
      }
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();

  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };

  Data->SetArrayField(TEXT("origin"), MakeArray(Origin));
  Data->SetArrayField(TEXT("extent"), MakeArray(BoxExtent));
  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Bounding box retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("name"), Found->GetName());
  Data->SetStringField(TEXT("label"), Found->GetActorLabel());
  Data->SetStringField(TEXT("path"), Found->GetPathName());
  Data->SetStringField(TEXT("class"), Found->GetClass()
                                          ? Found->GetClass()->GetPathName()
                                          : TEXT(""));

  TArray<TSharedPtr<FJsonValue>> TagsArray;
  for (const FName &Tag : Found->Tags) {
    TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
  }
  Data->SetArrayField(TEXT("tags"), TagsArray);

  const FTransform Current = Found->GetActorTransform();
  auto MakeArray = [](const FVector &Vec) {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };
  Data->SetArrayField(TEXT("location"), MakeArray(Current.GetLocation()));

  SendStandardSuccessResponse(this, Socket, RequestId,
                              TEXT("Metadata retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRemoveTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  if (TargetName.IsEmpty() || TagValue.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName and tag required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FName TagName(*TagValue);
  if (!Found->Tags.Contains(TagName)) {
    // Idempotent success
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("wasPresent"), false);
    Resp->SetStringField(TEXT("actorName"), Found->GetActorLabel());
    Resp->SetStringField(TEXT("tag"), TagValue);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Tag not present (idempotent)"), Resp,
                           FString());
    return true;
  }

  Found->Modify();
  Found->Tags.Remove(TagName);
  Found->MarkPackageDirty();

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetBoolField(TEXT("wasPresent"), true);
  Data->SetStringField(TEXT("actorName"), Found->GetActorLabel());
  Data->SetStringField(TEXT("tag"), TagValue);

  // Add verification data
	McpHandlerUtils::AddVerification(Data, Found);

	SendAutomationResponse(Socket, RequestId, true, TEXT("Tag removed from actor"), Data);
  return true;
#else
  return false;
#endif
}

// Additional handlers for test compatibility

bool UMcpAutomationBridgeSubsystem::HandleControlActorFindByClass(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ClassName;
  Payload->TryGetStringField(TEXT("className"), ClassName);
  if (ClassName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("class"), ClassName);
  }

  if (ClassName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("className or class is required"), nullptr);
    return true;
  }

  // Security: Validate class name format - reject path traversal attempts
  // Valid formats: "/Script/Module.ClassName", "/Game/Path/ClassName.ClassName", "ClassName"
  // Invalid: Contains "..", "\" (Windows paths), or other traversal patterns
  if (ClassName.Contains(TEXT("..")) || ClassName.Contains(TEXT("\\"))) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid class name format: '%s'. Path traversal characters are not allowed."), *ClassName), nullptr);
    return true;
  }

  // Additional security: Reject absolute filesystem paths
  if (ClassName.StartsWith(TEXT("/")) && !ClassName.StartsWith(TEXT("/Script/")) &&
      !ClassName.StartsWith(TEXT("/Game/")) && !ClassName.StartsWith(TEXT("/Engine/"))) {
    // Could be a path traversal attempt disguised as a valid path
    if (ClassName.Contains(TEXT("/etc/")) || ClassName.Contains(TEXT("/usr/")) ||
        ClassName.Contains(TEXT("/var/")) || ClassName.Contains(TEXT("/home/")) ||
        ClassName.Contains(TEXT("/root/")) || ClassName.Contains(TEXT("/tmp/")) ||
        ClassName.Contains(TEXT("C:\\")) || ClassName.Contains(TEXT("D:\\"))) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              FString::Printf(TEXT("Invalid class name format: '%s'. Filesystem paths are not allowed."), *ClassName), nullptr);
      return true;
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> ActorsArray;

  if (UWorld* World = GEditor->GetEditorWorldContext().World()) {
    UClass* ClassToFind = nullptr;

    // CRITICAL FIX: Use ResolveClassByName for proper engine class resolution
    // This handles: full paths, short names like "StaticMeshActor", and loads classes if needed
    // Without this, FindObject only finds already-loaded classes, missing engine classes like
    // AStaticMeshActor, APawn, etc. that haven't been accessed yet
    ClassToFind = ResolveClassByName(ClassName);

    if (ClassToFind) {
      for (TActorIterator<AActor> It(World, ClassToFind); It; ++It) {
        if (AActor* Actor = *It) {
          TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
          ActorObj->SetStringField(TEXT("name"), Actor->GetActorLabel());
          ActorObj->SetStringField(TEXT("path"), Actor->GetPathName());
          ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
        }
      }
    } else {
      // Class not found - return empty result (this is valid for searches)
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleControlActorFindByClass: Class '%s' not found"), *ClassName);
    }
  }

  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  SendStandardSuccessResponse(this, Socket, RequestId,
                              FString::Printf(TEXT("Found %d actors"), ActorsArray.Num()), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRemoveComponent(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("actor_name"), ActorName);
  }

  FString ComponentName;
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  if (ComponentName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("component_name"), ComponentName);
  }

  if (ActorName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName is required"), TEXT("MISSING_PARAM"));
    return true;
  }

  if (ComponentName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("componentName is required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Actor not found: %s"), *ActorName),
                        TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // CRITICAL FIX: Use FindComponentByName helper which supports fuzzy matching
  UActorComponent* Component = FindComponentByName(Actor, ComponentName);
  if (Component) {
    Component->DestroyComponent();
    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetStringField(TEXT("actorName"), ActorName);
    Data->SetStringField(TEXT("componentName"), ComponentName);

    // Add verification data for delete operations
    Data->SetBoolField(TEXT("existsAfter"), false);
    Data->SetStringField(TEXT("action"), TEXT("control_actor:deleted"));

    SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Component removed"), Data);
    return true;
  }

  SendAutomationError(Socket, RequestId,
                      FString::Printf(TEXT("Component not found: %s"), *ComponentName),
                      TEXT("COMPONENT_NOT_FOUND"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetComponentProperty(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName, ComponentName, PropertyName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("componentName"), ComponentName);
  Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
  if (PropertyName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("propertyPath"), PropertyName);
  }

  if (ActorName.IsEmpty() || ComponentName.IsEmpty() || PropertyName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName, componentName, and propertyName are required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // CRITICAL FIX: Use FindComponentByName helper which supports fuzzy matching
  // This handles cases where component names have numeric suffixes (e.g., "StaticMeshComponent0")
  UActorComponent* Component = FindComponentByName(Actor, ComponentName);
  if (!Component) {
    SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Component not found: %s on actor: %s"), *ComponentName, *ActorName),
        TEXT("COMPONENT_NOT_FOUND"));
    return true;
  }

  // Get property using reflection
  FProperty* Property = Component->GetClass()->FindPropertyByName(*PropertyName);
  if (!Property) {
    SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Property not found: %s on component: %s"), *PropertyName, *ComponentName),
        TEXT("PROPERTY_NOT_FOUND"));
    return true;
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  Data->SetStringField(TEXT("componentName"), ComponentName);
  Data->SetStringField(TEXT("propertyName"), PropertyName);
  Data->SetStringField(TEXT("propertyType"), Property->GetClass()->GetName());

  // Extract property value using the existing helper function
  TSharedPtr<FJsonValue> PropertyValue = ExportPropertyToJsonValue(Component, Property);
  if (PropertyValue.IsValid()) {
    Data->SetField(TEXT("value"), PropertyValue);
  } else {
    Data->SetStringField(TEXT("value"), TEXT("<unsupported property type>"));
  }

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Property retrieved"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSetCollision(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  bool bCollisionEnabled = true;

  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("actor_name"), ActorName);
  }

  if (Payload->HasField(TEXT("collisionEnabled"))) {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collisionEnabled"), true);
  } else if (Payload->HasField(TEXT("collision_enabled"))) {
    bCollisionEnabled = GetJsonBoolField(Payload, TEXT("collision_enabled"), true);
  }

  if (ActorName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName is required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // Set collision on root component
  if (USceneComponent* RootComp = Actor->GetRootComponent()) {
    if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(RootComp)) {
      if (bCollisionEnabled) {
        PrimComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
      } else {
        PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
      }
    }
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("actorName"), ActorName);
  Data->SetBoolField(TEXT("collisionEnabled"), bCollisionEnabled);
  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Collision setting updated"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorCallFunction(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName, FunctionName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  Payload->TryGetStringField(TEXT("functionName"), FunctionName);

  if (ActorName.IsEmpty() || FunctionName.IsEmpty()) {
    SendAutomationError(Socket, RequestId, TEXT("actorName and functionName are required"), TEXT("MISSING_PARAM"));
    return true;
  }

  AActor* Actor = FindActorByName(ActorName);
  if (!Actor) {
    SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
    return true;
  }

  // Find and call the function
  UFunction* Function = Actor->FindFunction(*FunctionName);
  if (Function) {
    // Check if function has parameters - passing nullptr to a function expecting
    // parameters can cause crashes or undefined behavior
    if (Function->ParmsSize > 0) {
      // Function has parameters - we need to provide a buffer
      // Allocate zeroed memory for parameters
      void* ParmsBuffer = FMemory::Malloc(Function->ParmsSize, 16);
      FMemory::Memzero(ParmsBuffer, Function->ParmsSize);

      // Call with parameter buffer
      Actor->ProcessEvent(Function, ParmsBuffer);

      // Free the buffer
      FMemory::Free(ParmsBuffer);
    } else {
      // No parameters, safe to pass nullptr
      Actor->ProcessEvent(Function, nullptr);
    }

    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetStringField(TEXT("actorName"), ActorName);
    Data->SetStringField(TEXT("functionName"), FunctionName);
    SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Function called"), Data);
    return true;
  }

  SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Function not found: %s"), *FunctionName), TEXT("FUNCTION_NOT_FOUND"));
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("control_actor"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("control_actor")))
    return false;
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("control_actor payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();


#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }
  if (!GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING"),
                              TEXT("EditorActorSubsystem not available"), nullptr);
    return true;
  }

  if (LowerSub == TEXT("spawn") || LowerSub == TEXT("spawn_actor"))
    return HandleControlActorSpawn(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("spawn_blueprint"))
    return HandleControlActorSpawnBlueprint(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("delete") || LowerSub == TEXT("remove") ||
      LowerSub == TEXT("destroy_actor"))
    return HandleControlActorDelete(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("apply_force") ||
      LowerSub == TEXT("apply_force_to_actor"))
    return HandleControlActorApplyForce(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_transform") ||
      LowerSub == TEXT("set_actor_transform") ||
      LowerSub == TEXT("teleport_actor") ||
      LowerSub == TEXT("set_actor_location") ||
      LowerSub == TEXT("set_actor_rotation") ||
      LowerSub == TEXT("set_actor_scale"))
    return HandleControlActorSetTransform(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_transform") ||
      LowerSub == TEXT("get_actor_transform"))
    return HandleControlActorGetTransform(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_visibility") ||
      LowerSub == TEXT("set_actor_visible") ||
      LowerSub == TEXT("set_actor_visibility"))
    return HandleControlActorSetVisibility(RequestId, Payload,
                                           RequestingSocket);
  if (LowerSub == TEXT("add_component"))
    return HandleControlActorAddComponent(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_component_properties") ||
      LowerSub == TEXT("set_component_property"))
    return HandleControlActorSetComponentProperties(RequestId, Payload,
                                                    RequestingSocket);
  if (LowerSub == TEXT("get_components") ||
      LowerSub == TEXT("get_actor_components"))
    return HandleControlActorGetComponents(RequestId, Payload,
                                           RequestingSocket);
  if (LowerSub == TEXT("duplicate"))
    return HandleControlActorDuplicate(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("attach") || LowerSub == TEXT("attach_actor"))
    return HandleControlActorAttach(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("detach") || LowerSub == TEXT("detach_actor"))
    return HandleControlActorDetach(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_tag"))
    return HandleControlActorFindByTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("add_tag"))
    return HandleControlActorAddTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("remove_tag"))
    return HandleControlActorRemoveTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_name") || LowerSub == TEXT("find_actors_by_name"))
    return HandleControlActorFindByName(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("delete_by_tag"))
    return HandleControlActorDeleteByTag(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_blueprint_variables"))
    return HandleControlActorSetBlueprintVariables(RequestId, Payload,
                                                   RequestingSocket);
  if (LowerSub == TEXT("create_snapshot"))
    return HandleControlActorCreateSnapshot(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("restore_snapshot"))
    return HandleControlActorRestoreSnapshot(RequestId, Payload,
                                             RequestingSocket);
  if (LowerSub == TEXT("export"))
    return HandleControlActorExport(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_bounding_box") || LowerSub == TEXT("get_actor_bounds"))
    return HandleControlActorGetBoundingBox(RequestId, Payload,
                                            RequestingSocket);
  if (LowerSub == TEXT("get_metadata"))
    return HandleControlActorGetMetadata(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("list") || LowerSub == TEXT("list_actors") || LowerSub == TEXT("list_objects"))
    return HandleControlActorList(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get") || LowerSub == TEXT("get_actor") ||
      LowerSub == TEXT("get_actor_by_name"))
    return HandleControlActorGet(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("find_by_class") || LowerSub == TEXT("find_actors_by_class"))
    return HandleControlActorFindByClass(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("remove_component"))
    return HandleControlActorRemoveComponent(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("get_component_property"))
    return HandleControlActorGetComponentProperty(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_property"))
    return HandleSetObjectProperty(RequestId, TEXT("set_object_property"), Payload, RequestingSocket);
  if (LowerSub == TEXT("get_property"))
    return HandleGetObjectProperty(RequestId, TEXT("get_object_property"), Payload, RequestingSocket);
  if (LowerSub == TEXT("set_collision") || LowerSub == TEXT("set_actor_collision"))
    return HandleControlActorSetCollision(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("call_function") || LowerSub == TEXT("call_actor_function"))
    return HandleControlActorCallFunction(RequestId, Payload, RequestingSocket);

  SendStandardErrorResponse(
      this, RequestingSocket, RequestId, TEXT("UNKNOWN_ACTION"),
      FString::Printf(TEXT("Unknown actor control action: %s"), *LowerSub), nullptr);
  return true;
#else
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Actor control requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPlay(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("alreadyPlaying"), true);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Play session already active"), Resp,
                           FString());
    return true;
  }

  FRequestPlaySessionParams PlayParams;
  PlayParams.WorldType = EPlaySessionWorldType::PlayInEditor;
#if MCP_HAS_LEVEL_EDITOR_PLAY_SETTINGS
  PlayParams.EditorPlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
#endif
#if MCP_HAS_LEVEL_EDITOR_MODULE
  if (FLevelEditorModule *LevelEditorModule =
          FModuleManager::GetModulePtr<FLevelEditorModule>(
              TEXT("LevelEditor"))) {
    TSharedPtr<IAssetViewport> DestinationViewport =
        LevelEditorModule->GetFirstActiveViewport();
    if (DestinationViewport.IsValid())
      PlayParams.DestinationSlateViewport = DestinationViewport;
  }
#endif

  GEditor->RequestPlaySession(PlayParams);
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Play in Editor started"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStop(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("alreadyStopped"), true);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Play session not active"), Resp, FString());
    return true;
  }

  GEditor->RequestEndPlayMap();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Play in Editor stopped"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorEject(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor->PlayWorld) {
    TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
    ErrorDetails->SetBoolField(TEXT("notInPIE"), true);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("Cannot eject: Play session not active"), ErrorDetails);
    return true;
  }

  // Use Eject console command instead of RequestEndPlayMap
  // This ejects the player from the possessed pawn without stopping PIE
  GEditor->Exec(GEditor->PlayWorld, TEXT("Eject"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("ejected"), true);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Ejected from possessed actor"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPossess(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);

  // Also try "objectPath" as fallback since schema might use that
  if (ActorName.IsEmpty())
    Payload->TryGetStringField(TEXT("objectPath"), ActorName);

  if (ActorName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(ActorName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr);
    return true;
  }

  if (GEditor) {
    GEditor->SelectNone(true, true, false);
    GEditor->SelectActor(Found, true, true, true);
    // 'POSSESS' command works on selected actor in PIE
    if (GEditor->PlayWorld) {
      GEditor->Exec(GEditor->PlayWorld, TEXT("POSSESS"));
      SendAutomationResponse(Socket, RequestId, true, TEXT("Possessed actor"),
                             nullptr);
    } else {
      // If not in PIE, we can't possess
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IN_PIE"),
                              TEXT("Cannot possess actor while not in PIE"), nullptr);
    }
    return true;
  }

  SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorFocusActor(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString ActorName;
  Payload->TryGetStringField(TEXT("actorName"), ActorName);
  if (ActorName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  if (UEditorActorSubsystem *ActorSS =
          GEditor->GetEditorSubsystem<UEditorActorSubsystem>()) {
    TArray<AActor *> Actors = ActorSS->GetAllLevelActors();
    for (AActor *Actor : Actors) {
      if (!Actor)
        continue;
      if (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase)) {
        GEditor->SelectNone(true, true, false);
        GEditor->SelectActor(Actor, true, true, true);
        GEditor->Exec(nullptr, TEXT("EDITORTEMPVIEWPORT"));
        GEditor->MoveViewportCamerasToActor(*Actor, false);
        SendAutomationResponse(Socket, RequestId, true,
                               TEXT("Viewport focused on actor"), nullptr,
                               FString());
        return true;
      }
    }
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }
  return false;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetCamera(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  const TSharedPtr<FJsonObject> *Loc = nullptr;
  FVector Location(0, 0, 0);
  FRotator Rotation(0, 0, 0);
  if (Payload->TryGetObjectField(TEXT("location"), Loc) && Loc &&
      (*Loc).IsValid())
    ReadVectorField(*Loc, TEXT(""), Location, Location);
  if (Payload->TryGetObjectField(TEXT("rotation"), Loc) && Loc &&
      (*Loc).IsValid())
    ReadRotatorField(*Loc, TEXT(""), Rotation, Rotation);

#if defined(MCP_HAS_UNREALEDITOR_SUBSYSTEM)
  if (UUnrealEditorSubsystem *UES =
          GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()) {
    UES->SetLevelViewportCameraInfo(Location, Rotation);
#if defined(MCP_HAS_LEVELEDITOR_SUBSYSTEM)
    if (ULevelEditorSubsystem *LES =
            GEditor->GetEditorSubsystem<ULevelEditorSubsystem>())
      LES->EditorInvalidateViewports();
#endif
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp,
                           FString());
    return true;
  }
#endif
  if (FEditorViewportClient *ViewportClient =
          GEditor->GetActiveViewport()
              ? (FEditorViewportClient *)GEditor->GetActiveViewport()
                    ->GetClient()
              : nullptr) {
    ViewportClient->SetViewLocation(Location);
    ViewportClient->SetViewRotation(Rotation);
    ViewportClient->Invalidate();
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(Socket, RequestId, true, TEXT("Camera set"), Resp,
                           FString());
    return true;
  }
  return false;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Mode;
  Payload->TryGetStringField(TEXT("viewMode"), Mode);
  FString LowerMode = Mode.ToLower();
  FString Chosen;
  EViewModeIndex ViewModeIndex = VMI_Lit;
  bool bHasNativeViewMode = true;
  if (LowerMode == TEXT("lit"))
  {
    Chosen = TEXT("Lit");
    ViewModeIndex = VMI_Lit;
  }
  else if (LowerMode == TEXT("unlit"))
  {
    Chosen = TEXT("Unlit");
    ViewModeIndex = VMI_Unlit;
  }
  else if (LowerMode == TEXT("wireframe"))
  {
    Chosen = TEXT("Wireframe");
    ViewModeIndex = VMI_Wireframe;
  }
  else if (LowerMode == TEXT("detaillighting"))
  {
    Chosen = TEXT("DetailLighting");
    ViewModeIndex = VMI_Lit_DetailLighting;
  }
  else if (LowerMode == TEXT("lightingonly"))
  {
    Chosen = TEXT("LightingOnly");
    ViewModeIndex = VMI_LightingOnly;
  }
  else if (LowerMode == TEXT("lightcomplexity"))
  {
    Chosen = TEXT("LightComplexity");
    ViewModeIndex = VMI_LightComplexity;
  }
  else if (LowerMode == TEXT("shadercomplexity"))
  {
    Chosen = TEXT("ShaderComplexity");
    ViewModeIndex = VMI_ShaderComplexity;
  }
  else if (LowerMode == TEXT("lightmapdensity"))
  {
    Chosen = TEXT("LightmapDensity");
    ViewModeIndex = VMI_LightmapDensity;
  }
  else if (LowerMode == TEXT("stationarylightoverlap"))
  {
    Chosen = TEXT("StationaryLightOverlap");
    ViewModeIndex = VMI_StationaryLightOverlap;
  }
  else if (LowerMode == TEXT("reflectionoverride"))
  {
    Chosen = TEXT("ReflectionOverride");
    ViewModeIndex = VMI_ReflectionOverride;
  }
  else if (IsSafeConsoleArgumentToken(Mode))
  {
    Chosen = Mode;
    bHasNativeViewMode = false;
  }
  else {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("Invalid viewMode"), nullptr);
    return true;
  }

  if (bHasNativeViewMode) {
    if (FEditorViewportClient* ViewportClient = GetActiveEditorViewportClientForMcp()) {
      ViewportClient->SetViewMode(ViewModeIndex);
      ViewportClient->Invalidate();

      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("viewMode"), Chosen);
      Resp->SetStringField(TEXT("method"), TEXT("viewport_client"));
      SendAutomationResponse(Socket, RequestId, true, TEXT("View mode set"), Resp,
                             FString());
      return true;
    }
  }

  const FString Cmd = FString::Printf(TEXT("viewmode %s"), *Chosen);
  if (GEditor->Exec(nullptr, *Cmd)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("viewMode"), Chosen);
    SendAutomationResponse(Socket, RequestId, true, TEXT("View mode set"), Resp,
                           FString());
    return true;
  }
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("EXEC_FAILED"),
                              TEXT("View mode command failed"), nullptr);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetCameraFov(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double Fov = 90.0;
  Payload->TryGetNumberField(TEXT("fov"), Fov);
  if (Fov <= 1.0 || Fov >= 179.0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("fov must be between 1 and 179 degrees"), nullptr);
    return true;
  }

  if (FEditorViewportClient* ViewportClient = GetActiveEditorViewportClientForMcp()) {
    ViewportClient->ViewFOV = static_cast<float>(Fov);
    ViewportClient->FOVAngle = static_cast<float>(Fov);
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetNumberField(TEXT("fov"), Fov);
    Resp->SetStringField(TEXT("method"), TEXT("viewport_client"));
    SendAutomationResponse(Socket, RequestId, true, TEXT("Camera FOV set"), Resp,
                           FString());
    return true;
  }

  SendStandardErrorResponse(this, Socket, RequestId, TEXT("VIEWPORT_NOT_AVAILABLE"),
                            TEXT("No editor viewport available for FOV update"), nullptr);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGameSpeed(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double Speed = 1.0;
  Payload->TryGetNumberField(TEXT("speed"), Speed);
  if (Speed <= 0.0 || Speed > 20.0) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("speed must be greater than 0 and no more than 20"), nullptr);
    return true;
  }

  UWorld* World = GEditor && GEditor->PlayWorld
      ? GEditor->PlayWorld.Get()
      : (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
  if (!World || !World->GetWorldSettings()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_WORLD"),
                              TEXT("No world available for game speed update"), nullptr);
    return true;
  }

  World->GetWorldSettings()->SetTimeDilation(static_cast<float>(Speed));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("speed"), Speed);
  Resp->SetNumberField(TEXT("timeDilation"), World->GetWorldSettings()->GetEffectiveTimeDilation());
  SendAutomationResponse(Socket, RequestId, true, TEXT("Game speed set"), Resp,
                         FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("control_editor"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("control_editor")))
    return false;
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("control_editor payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  if (LowerSub == TEXT("play"))
    return HandleControlEditorPlay(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("stop") || LowerSub == TEXT("stop_pie"))
    return HandleControlEditorStop(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("eject"))
    return HandleControlEditorEject(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("possess"))
    return HandleControlEditorPossess(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("focus_actor"))
    return HandleControlEditorFocusActor(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_camera") ||
      LowerSub == TEXT("set_camera_position") ||
      LowerSub == TEXT("set_viewport_camera"))
    return HandleControlEditorSetCamera(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_view_mode"))
    return HandleControlEditorSetViewMode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_camera_fov"))
    return HandleControlEditorSetCameraFov(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_game_speed"))
    return HandleControlEditorSetGameSpeed(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("open_asset"))
    return HandleControlEditorOpenAsset(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("screenshot") || LowerSub == TEXT("take_screenshot"))
    return HandleControlEditorScreenshot(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("pause"))
    return HandleControlEditorPause(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("resume"))
    return HandleControlEditorResume(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("console_command") || LowerSub == TEXT("execute_command"))
    return HandleControlEditorConsoleCommand(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("step_frame"))
    return HandleControlEditorStepFrame(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("start_recording"))
    return HandleControlEditorStartRecording(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("stop_recording"))
    return HandleControlEditorStopRecording(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("create_bookmark"))
    return HandleControlEditorCreateBookmark(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("jump_to_bookmark"))
    return HandleControlEditorJumpToBookmark(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_preferences"))
    return HandleControlEditorSetPreferences(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_viewport_realtime"))
    return HandleControlEditorSetViewportRealtime(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("simulate_input"))
    return HandleControlEditorSimulateInput(RequestId, Payload, RequestingSocket);
  // Additional actions for test compatibility
  if (LowerSub == TEXT("close_asset"))
    return HandleControlEditorCloseAsset(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("save_all"))
    return HandleControlEditorSaveAll(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("undo"))
    return HandleControlEditorUndo(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("redo"))
    return HandleControlEditorRedo(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_editor_mode"))
    return HandleControlEditorSetEditorMode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("show_stats"))
    return HandleControlEditorShowStats(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("hide_stats"))
    return HandleControlEditorHideStats(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_game_view"))
    return HandleControlEditorSetGameView(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_immersive_mode"))
    return HandleControlEditorSetImmersiveMode(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("single_frame_step"))
    return HandleControlEditorStepFrame(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("set_fixed_delta_time"))
    return HandleControlEditorSetFixedDeltaTime(RequestId, Payload, RequestingSocket);
  if (LowerSub == TEXT("open_level"))
    return HandleControlEditorOpenLevel(RequestId, Payload, RequestingSocket);

  SendStandardErrorResponse(
      this, RequestingSocket, RequestId, TEXT("UNKNOWN_ACTION"),
      FString::Printf(TEXT("Unknown editor control action: %s"), *LowerSub), nullptr);
  return true;
#else
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Editor control requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorOpenAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("assetPath required"), nullptr);
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SECURITY_VIOLATION"),
                              TEXT("Invalid assetPath"), nullptr);
    return true;
  }

  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  UAssetEditorSubsystem *AssetEditorSS =
      GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!AssetEditorSS) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SUBSYSTEM_MISSING"),
                              TEXT("AssetEditorSubsystem not available"), nullptr);
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ASSET_NOT_FOUND"),
                              TEXT("Asset not found"), nullptr);
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("LOAD_FAILED"),
                              TEXT("Failed to load asset"), nullptr);
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetName());
    Resp->SetBoolField(TEXT("loaded"), true);
    Resp->SetBoolField(TEXT("editorOpened"), false);
    Resp->SetBoolField(TEXT("headlessSafe"), true);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Asset loaded; editor window skipped under NullRHI"), Resp,
                           FString());
    return true;
  }

  const bool bOpened = AssetEditorSS->OpenEditorForAsset(Asset);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bOpened);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);

  if (bOpened) {
    SendAutomationResponse(Socket, RequestId, true, TEXT("Asset opened"), Resp,
                           FString());
  } else {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("OPEN_FAILED"),
                              TEXT("Failed to open asset editor"), Resp);
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorScreenshot(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString Mode;
  Payload->TryGetStringField(TEXT("mode"), Mode);
  Mode = Mode.TrimStartAndEnd().ToLower();
  if (Mode.IsEmpty()) {
    Mode = TEXT("editor_viewport");
  }

  if (Mode == TEXT("game_viewport")) {
    return HandleUiAction(RequestId, TEXT("system_control"), Payload, Socket);
  }

  if (Mode != TEXT("editor_viewport") && Mode != TEXT("full_editor_window")) {
    SendStandardErrorResponse(
        this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
        TEXT("Invalid screenshot mode. Supported modes: editor_viewport, game_viewport, full_editor_window"),
        nullptr);
    return true;
  }

  const FString Filename = MakeSafeScreenshotFilenameForMcp(Payload);

  // Build the full path - save to project's Saved/Screenshots folder
  const FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
  IFileManager::Get().MakeDirectory(*ScreenshotDir, true);
  const FString FullPath = ScreenshotDir / Filename;

  if (Mode == TEXT("full_editor_window")) {
    TSharedPtr<SWindow> EditorWindow = GetFullEditorSlateWindowForMcp();
    if (!EditorWindow.IsValid()) {
      SendStandardErrorResponse(this, Socket, RequestId,
                                TEXT("EDITOR_WINDOW_NOT_AVAILABLE"),
                                TEXT("No visible editor window available for full editor screenshot"),
                                nullptr);
      return true;
    }

    TArray<uint8> PngData;
    FIntVector ImageSize(0, 0, 0);
    FString CaptureError;
    if (!CaptureSlateWindowPngForMcp(EditorWindow.ToSharedRef(), PngData,
                                     ImageSize, CaptureError)) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("CAPTURE_FAILED"),
                                CaptureError, nullptr);
      return true;
    }

    const bool bSaved = FFileHelper::SaveArrayToFile(PngData, *FullPath);

    bool bReturnBase64 = true;
    Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("filename"), Filename);
    Resp->SetStringField(TEXT("mode"), Mode);
    Resp->SetBoolField(TEXT("saved"), bSaved);
    Resp->SetNumberField(TEXT("width"), ImageSize.X);
    Resp->SetNumberField(TEXT("height"), ImageSize.Y);
    Resp->SetNumberField(TEXT("sizeBytes"), PngData.Num());
    Resp->SetStringField(TEXT("mimeType"), TEXT("image/png"));
    if (bSaved) {
      Resp->SetStringField(TEXT("path"), FullPath);
      Resp->SetStringField(TEXT("screenshotPath"), FullPath);
    }
    AddScreenshotMetadataForMcp(Resp, Payload);
    if (!bSaved && !bReturnBase64) {
      const FString SaveError = TEXT("Full editor window screenshot captured but failed to save, and returnBase64=false leaves no image output.");
      Resp->SetBoolField(TEXT("success"), false);
      Resp->SetStringField(TEXT("error"), SaveError);
      Resp->SetStringField(TEXT("message"), SaveError);
      SendAutomationResponse(Socket, RequestId, false, SaveError, Resp,
                             TEXT("SAVE_FAILED"));
      return true;
    }
    if (bReturnBase64 && PngData.Num() > MaxScreenshotPngBytesForBase64ForMcp) {
      const FString SizeError = MakeScreenshotTooLargeMessageForMcp(PngData.Num());
      Resp->SetBoolField(TEXT("success"), false);
      Resp->SetStringField(TEXT("error"), SizeError);
      Resp->SetStringField(TEXT("message"), SizeError);
      SendAutomationResponse(Socket, RequestId, false, SizeError, Resp,
                             TEXT("IMAGE_TOO_LARGE"));
      return true;
    }
    if (bReturnBase64) {
      Resp->SetStringField(TEXT("imageBase64"), FBase64::Encode(PngData));
    }
    Resp->SetStringField(TEXT("message"),
        bReturnBase64
            ? TEXT("Full editor window screenshot captured and returned as image/png base64.")
            : TEXT("Full editor window screenshot captured."));

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Full editor window screenshot captured"), Resp,
                           FString());
    return true;
  }

  // Get the active viewport
  FViewport* Viewport = GEditor->GetActiveViewport();
  if (!Viewport) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("VIEWPORT_NOT_AVAILABLE"),
                              TEXT("No active viewport available"), nullptr);
    return true;
  }

  // Request a screenshot — async, captured on the next rendered viewport frame.
  // The file will NOT exist immediately; the editor window must be visible and
  // actively rendering for the capture to complete.
  FScreenshotRequest::RequestScreenshot(FullPath, false, false);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("filename"), Filename);
  Resp->SetStringField(TEXT("mode"), Mode);
  Resp->SetStringField(TEXT("path"), FullPath);
  Resp->SetBoolField(TEXT("async"), true);
  Resp->SetStringField(TEXT("message"),
      TEXT("Screenshot queued. File will be written on the next rendered frame "
           "(typically within 100-200ms). Poll the file path to confirm availability. "
           "The editor window must be visible for capture to complete."));
  Resp->SetStringField(TEXT("expectedDelay"), TEXT("200ms"));
  AddScreenshotMetadataForMcp(Resp, Payload);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Screenshot queued"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Screenshot requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorPause(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("No active PIE session to pause"), nullptr);
    return true;
  }

  // Pause PIE execution
  GEditor->PlayWorld->bDebugPauseExecution = true;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("state"), TEXT("paused"));
  Resp->SetStringField(TEXT("message"), TEXT("PIE session paused"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("PIE session paused"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Pause requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorResume(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("No active PIE session to resume"), nullptr);
    return true;
  }

  // Resume PIE execution
  GEditor->PlayWorld->bDebugPauseExecution = false;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("state"), TEXT("resumed"));
  Resp->SetStringField(TEXT("message"), TEXT("PIE session resumed"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("PIE session resumed"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Resume requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorConsoleCommand(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  return HandleConsoleCommandAction(RequestId, TEXT("console_command"), Payload, Socket);
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Console command requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStepFrame(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Check if we're in PIE
  if (!GEditor->PlayWorld) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("NO_ACTIVE_SESSION"),
                              TEXT("No active PIE session to step"), nullptr);
    return true;
  }

  // Step one frame - set debug step flag and unpause momentarily
  GEditor->PlayWorld->bDebugFrameStepExecution = true;
  GEditor->PlayWorld->bDebugPauseExecution = false;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("message"), TEXT("Stepped one frame"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Frame stepped"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Step frame requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStartRecording(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString RecordingName;
  // Accept both 'name' and 'filename' fields for flexibility
  // TS handler sends 'filename', so we check that first
  Payload->TryGetStringField(TEXT("filename"), RecordingName);
  if (RecordingName.IsEmpty()) {
    Payload->TryGetStringField(TEXT("name"), RecordingName);
  }
  if (RecordingName.IsEmpty()) {
    RecordingName = FString::Printf(TEXT("Recording_%s"),
        *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
  } else {
    RecordingName = MakeSafeConsoleName(RecordingName, TEXT("Recording"));
  }

  // Use console command to start demo recording
  // UE 5.7: TObjectPtr requires explicit cast to UWorld*
  UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
  if (World) {
    FString Command = FString::Printf(TEXT("DemoRec %s"), *RecordingName);
    GEditor->Exec(World, *Command);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("recordingName"), RecordingName);
  Resp->SetStringField(TEXT("message"), TEXT("Recording started"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Recording started"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Recording requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorStopRecording(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Use console command to stop demo recording
  // UE 5.7: TObjectPtr requires explicit cast to UWorld*
  UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
  if (World) {
    GEditor->Exec(World, TEXT("DemoStop"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("message"), TEXT("Recording stopped"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Recording stopped"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Recording requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCreateBookmark(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  int32 BookmarkIndex = 0;
  Payload->TryGetNumberField(TEXT("index"), BookmarkIndex);
  if (!Payload->HasField(TEXT("index"))) {
    Payload->TryGetNumberField(TEXT("id"), BookmarkIndex);
  }

  // Clamp to valid bookmark range (0-9)
  BookmarkIndex = FMath::Clamp(BookmarkIndex, 0, 9);

  // Use console command to set bookmark
  FString Command = FString::Printf(TEXT("SetBookmark %d"), BookmarkIndex);
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("index"), BookmarkIndex);
  Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Bookmark %d created"), BookmarkIndex));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Bookmark created"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Bookmarks require editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorJumpToBookmark(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  int32 BookmarkIndex = 0;
  Payload->TryGetNumberField(TEXT("index"), BookmarkIndex);
  if (!Payload->HasField(TEXT("index"))) {
    Payload->TryGetNumberField(TEXT("id"), BookmarkIndex);
  }

  // Clamp to valid bookmark range (0-9)
  BookmarkIndex = FMath::Clamp(BookmarkIndex, 0, 9);

  // Use console command to jump to bookmark
  FString Command = FString::Printf(TEXT("JumpToBookmark %d"), BookmarkIndex);
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("index"), BookmarkIndex);
  Resp->SetStringField(TEXT("message"), FString::Printf(TEXT("Jumped to bookmark %d"), BookmarkIndex));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Jumped to bookmark"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Bookmarks require editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetPreferences(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  TArray<FString> AppliedSettings;
  TArray<FString> FailedSettings;
  FString Category;
  Payload->TryGetStringField(TEXT("category"), Category);

  // Get preferences object from payload
  const TSharedPtr<FJsonObject>* PrefsPtr = nullptr;
  if (Payload->TryGetObjectField(TEXT("preferences"), PrefsPtr) && PrefsPtr && (*PrefsPtr).IsValid()) {
    for (const auto& Pair : (*PrefsPtr)->Values) {
      const FString PreferenceName(*Pair.Key);
      // Try to set via console variable first
      IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*PreferenceName);
      if (CVar) {
        FString Value;
        if (Pair.Value->TryGetString(Value)) {
          CVar->Set(*Value);
          AppliedSettings.Add(PreferenceName);
        } else {
          double NumVal;
          if (Pair.Value->TryGetNumber(NumVal)) {
            CVar->Set((float)NumVal);
            AppliedSettings.Add(PreferenceName);
          } else {
            bool BoolVal;
            if (Pair.Value->TryGetBool(BoolVal)) {
              CVar->Set(BoolVal ? 1 : 0);
              AppliedSettings.Add(PreferenceName);
            } else {
              FailedSettings.Add(PreferenceName);
            }
          }
        }
      } else if (Category.Equals(TEXT("LevelEditor"), ESearchCase::IgnoreCase) && PreferenceName.Equals(TEXT("RealtimeAudio"), ESearchCase::IgnoreCase)) {
        bool BoolVal;
        if (Pair.Value->TryGetBool(BoolVal)) {
          GEditor->MuteRealTimeAudio(!BoolVal);
          AppliedSettings.Add(PreferenceName);
        } else {
          FailedSettings.Add(PreferenceName);
        }
      } else {
        FailedSettings.Add(PreferenceName);
      }
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  const bool bAnyPreferenceApplied = AppliedSettings.Num() > 0;
  const bool bPreferencesUpdated = bAnyPreferenceApplied && FailedSettings.Num() == 0;
  Resp->SetBoolField(TEXT("success"), bPreferencesUpdated);
  Resp->SetNumberField(TEXT("appliedCount"), AppliedSettings.Num());

  if (AppliedSettings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> AppliedArray;
    for (const FString& Name : AppliedSettings)
      AppliedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("applied"), AppliedArray);
  }

  if (FailedSettings.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> FailedArray;
    for (const FString& Name : FailedSettings)
      FailedArray.Add(MakeShared<FJsonValueString>(Name));
    Resp->SetArrayField(TEXT("failed"), FailedArray);
  }

  const FString ResponseMessage = bPreferencesUpdated
      ? TEXT("Preferences updated")
      : (bAnyPreferenceApplied ? TEXT("Preferences partially updated") : TEXT("No preferences updated"));
  const FString ResponseErrorCode = bPreferencesUpdated
      ? FString()
      : (bAnyPreferenceApplied ? FString(TEXT("PREFERENCES_PARTIALLY_APPLIED")) : FString(TEXT("PREFERENCES_NOT_APPLIED")));
  SendAutomationResponse(Socket, RequestId, bPreferencesUpdated,
                         ResponseMessage, Resp, ResponseErrorCode);
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Preferences require editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetViewportRealtime(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  bool bRealtime = true;
  Payload->TryGetBoolField(TEXT("realtime"), bRealtime);

#if MCP_HAS_LEVEL_EDITOR_MODULE
  // Get the level editor module and active viewport
  FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
  TSharedPtr<IAssetViewport> ActiveViewport = LevelEditorModule.GetFirstActiveViewport();

  if (ActiveViewport.IsValid()) {
    FEditorViewportClient& ViewportClient = ActiveViewport->GetAssetViewportClient();
    ViewportClient.SetRealtime(bRealtime);

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetBoolField(TEXT("realtime"), bRealtime);
    Resp->SetStringField(TEXT("message"), bRealtime ? TEXT("Viewport realtime enabled") : TEXT("Viewport realtime disabled"));

    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Viewport realtime updated"), Resp, FString());
    return true;
  }
#endif

  // Fallback: use console command
  FString Command = bRealtime ? TEXT("Viewport Realtime") : TEXT("Viewport Realtime 0");
  UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
  GEditor->Exec(World, *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("realtime"), bRealtime);
  Resp->SetStringField(TEXT("message"), bRealtime ? TEXT("Viewport realtime enabled") : TEXT("Viewport realtime disabled"));

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Viewport realtime updated"), Resp, FString());
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Viewport realtime requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSimulateInput(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Accept multiple field names for flexibility
  // - 'type': C++ native field (key_down, key_up, mouse_click, mouse_move)
  // - 'inputType': Alternative name
  // - 'inputAction': Action-based naming (pressed, released, click, move)
  // CRITICAL: Do NOT read from 'action' field - that's the routing action (e.g., "simulate_input")
  // and will always be present in the payload. Only use type/inputType/inputAction for input type.
  FString InputType;
  Payload->TryGetStringField(TEXT("type"), InputType);
  if (InputType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("inputType"), InputType);
  }
  if (InputType.IsEmpty()) {
    Payload->TryGetStringField(TEXT("inputAction"), InputType);
  }

  // Map action values to C++ expected type values
  InputType = InputType.ToLower();
  if (InputType == TEXT("pressed") || InputType == TEXT("down")) {
    InputType = TEXT("key_down");
  } else if (InputType == TEXT("released") || InputType == TEXT("up")) {
    InputType = TEXT("key_up");
  } else if (InputType == TEXT("click")) {
    InputType = TEXT("mouse_click");
  } else if (InputType == TEXT("move")) {
    InputType = TEXT("mouse_move");
  }

  FString Key;
  Payload->TryGetStringField(TEXT("key"), Key);

  bool bSuccess = false;
  bool bRoutedToPIE = false;
  bool bHandledByPIE = false;
  bool bHandledBySlate = false;
  FString Message;

  auto RouteKeyToPIE = [](const FKey &InputKey, const EInputEvent InputEvent,
                          bool &bOutHandledByPIE) -> bool {
    bOutHandledByPIE = false;
    if (!GEditor || !GEditor->PlayWorld) {
      return false;
    }

    UWorld *PlayWorld = GEditor->PlayWorld.Get();
    if (!PlayWorld) {
      return false;
    }

    APlayerController *PlayerController = PlayWorld->GetFirstPlayerController();
    if (!PlayerController) {
      return false;
    }

    const double AmountDepressed = InputEvent == IE_Released ? 0.0 : 1.0;
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    FInputKeyParams KeyParams(InputKey, InputEvent, AmountDepressed, false);
    bOutHandledByPIE = PlayerController->InputKey(KeyParams);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    return true;
  };

  if (InputType == TEXT("key_down") || InputType == TEXT("keydown")) {
    if (!Key.IsEmpty()) {
      FKey InputKey(*Key);
      if (InputKey.IsValid()) {
        bRoutedToPIE = RouteKeyToPIE(InputKey, IE_Pressed, bHandledByPIE);
        if (!bRoutedToPIE) {
          FSlateApplication& SlateApp = FSlateApplication::Get();
          FKeyEvent KeyEvent(InputKey, FModifierKeysState(), 0, false, 0, 0);
          bHandledBySlate = SlateApp.ProcessKeyDownEvent(KeyEvent);
        }
        bSuccess = true;
        Message = bRoutedToPIE
            ? FString::Printf(TEXT("Key down: %s (delivered to PIE)"), *Key)
            : FString::Printf(TEXT("Key down: %s"), *Key);
      } else {
        Message = FString::Printf(TEXT("Invalid key: %s"), *Key);
      }
    } else {
      Message = TEXT("Key parameter required for key_down");
    }
  } else if (InputType == TEXT("key_up") || InputType == TEXT("keyup")) {
    if (!Key.IsEmpty()) {
      FKey InputKey(*Key);
      if (InputKey.IsValid()) {
        bRoutedToPIE = RouteKeyToPIE(InputKey, IE_Released, bHandledByPIE);
        if (!bRoutedToPIE) {
          FSlateApplication& SlateApp = FSlateApplication::Get();
          FKeyEvent KeyEvent(InputKey, FModifierKeysState(), 0, false, 0, 0);
          bHandledBySlate = SlateApp.ProcessKeyUpEvent(KeyEvent);
        }
        bSuccess = true;
        Message = bRoutedToPIE
            ? FString::Printf(TEXT("Key up: %s (delivered to PIE)"), *Key)
            : FString::Printf(TEXT("Key up: %s"), *Key);
      } else {
        Message = FString::Printf(TEXT("Invalid key: %s"), *Key);
      }
    } else {
      Message = TEXT("Key parameter required for key_up");
    }
  } else if (InputType == TEXT("mouse_click") || InputType == TEXT("click")) {
    double X = 0, Y = 0;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    FString Button = TEXT("left");
    Payload->TryGetStringField(TEXT("button"), Button);

    // UE 5.7: EKeys::Type is deprecated, use FKey directly
    FKey MouseButtonKey = EKeys::LeftMouseButton;
    if (Button.ToLower() == TEXT("right")) MouseButtonKey = EKeys::RightMouseButton;
    else if (Button.ToLower() == TEXT("middle")) MouseButtonKey = EKeys::MiddleMouseButton;

    FSlateApplication& SlateApp = FSlateApplication::Get();
    FVector2D Position((float)X, (float)Y);

    // UE 5.7: FPointerEvent constructor signature changed
    // FPointerEvent(uint32 InPointerIndex, ScreenSpacePosition, LastScreenSpacePosition, Delta, PressedButtons, ModifierKeys)
    TSet<FKey> PressedButtons;
    PressedButtons.Add(MouseButtonKey);

    // Simulate mouse down then up for a click
    FPointerEvent MouseDownEvent(
        0,  // PointerIndex
        Position,  // ScreenSpacePosition
        Position,  // LastScreenSpacePosition
        FVector2D(0.0f, 0.0f),  // Delta
        PressedButtons,
        FModifierKeysState()
    );
    SlateApp.ProcessMouseButtonDownEvent(nullptr, MouseDownEvent);

    TSet<FKey> ReleasedButtons;  // Empty set for mouse up
    FPointerEvent MouseUpEvent(
        0,  // PointerIndex
        Position,  // ScreenSpacePosition
        Position,  // LastScreenSpacePosition
        FVector2D(0.0f, 0.0f),  // Delta
        ReleasedButtons,
        FModifierKeysState()
    );
    SlateApp.ProcessMouseButtonUpEvent(MouseUpEvent);
    bHandledBySlate = true;

    bSuccess = true;
    Message = FString::Printf(TEXT("Mouse click at (%f, %f)"), X, Y);
  } else if (InputType == TEXT("mouse_move") || InputType == TEXT("move")) {
    double X = 0, Y = 0;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    FSlateApplication& SlateApp = FSlateApplication::Get();
    FVector2D Position((float)X, (float)Y);
    SlateApp.SetCursorPos(Position);
    bHandledBySlate = true;

    bSuccess = true;
    Message = FString::Printf(TEXT("Mouse moved to (%f, %f)"), X, Y);
  } else {
    Message = FString::Printf(TEXT("Unknown input type: %s. Supported: key_down, key_up, mouse_click, mouse_move"), *InputType);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetStringField(TEXT("type"), InputType);
  Resp->SetStringField(TEXT("message"), Message);
  Resp->SetBoolField(TEXT("routedToPIE"), bRoutedToPIE);
  Resp->SetBoolField(TEXT("handledByPIE"), bHandledByPIE);
  Resp->SetBoolField(TEXT("handledBySlate"), bHandledBySlate);

  if (bSuccess) {
    SendAutomationResponse(Socket, RequestId, true, Message, Resp, FString());
  } else {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INPUT_FAILED"), Message, Resp);
  }
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                              TEXT("Simulate input requires editor build."), nullptr);
  return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCloseAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("assetPath required"), nullptr);
    return true;
  }

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SECURITY_VIOLATION"),
                              TEXT("Invalid assetPath"), nullptr);
    return true;
  }

  UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
  if (!AssetEditorSS) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SUBSYSTEM_MISSING"),
                              TEXT("AssetEditorSubsystem unavailable"), nullptr);
    return true;
  }

  UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("LOAD_FAILED"),
                              TEXT("Failed to load asset"), nullptr);
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), AssetPath);
    Resp->SetBoolField(TEXT("loaded"), true);
    Resp->SetBoolField(TEXT("editorClosed"), false);
    Resp->SetBoolField(TEXT("headlessSafe"), true);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Asset verified; editor close skipped under NullRHI"), Resp,
                           FString());
    return true;
  }

  AssetEditorSS->CloseAllEditorsForAsset(Asset);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset editor closed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSaveAll(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  TArray<UPackage*> DirtyWorldPackages;
  TArray<UPackage*> DirtyContentPackages;
  FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
  FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);

  bool bSuccess = true;
  int32 SavedWorldCount = 0;
  int32 SavedContentCount = 0;
  int32 SkippedCount = 0;
  int32 TotalDirty = 0;
  TArray<FString> SkippedPackages;
  TArray<FString> FailedPackages;
  TSet<UPackage*> ProcessedPackages;

  auto ShouldSkipPackage = [](UPackage* Package) -> bool {
    if (!Package || Package->HasAnyFlags(RF_Transient)) {
      return true;
    }
    const FString PackagePath = Package->GetPathName();
    return PackagePath.StartsWith(TEXT("/Temp/")) ||
           PackagePath.StartsWith(TEXT("/Transient/")) ||
           PackagePath.StartsWith(TEXT("/Engine/Transient"));
  };

  auto ProcessPackage = [&](UPackage* Package) {
    if (!Package || ProcessedPackages.Contains(Package)) {
      return;
    }
    ProcessedPackages.Add(Package);
    TotalDirty++;

    FString PackagePath = Package->GetPathName();
    if (ShouldSkipPackage(Package)) {
      SkippedCount++;
      SkippedPackages.Add(PackagePath);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("HandleControlEditorSaveAll: Skipping transient/temp package: %s"), *PackagePath);
      return;
    }

    UWorld* PackageWorld = UWorld::FindWorldInPackage(Package);
    if (PackageWorld) {
      if (PackageWorld->PersistentLevel && McpSafeLevelSave(PackageWorld->PersistentLevel, PackagePath)) {
        SavedWorldCount++;
      } else {
        bSuccess = false;
        FailedPackages.Add(PackagePath);
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("HandleControlEditorSaveAll: Failed to save world package: %s"), *PackagePath);
      }
      return;
    }

    if (McpSafeAssetSave(Package)) {
      SavedContentCount++;
    } else {
      bSuccess = false;
      FailedPackages.Add(PackagePath);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleControlEditorSaveAll: Failed to save content package: %s"), *PackagePath);
    }
  };

  for (UPackage* Package : DirtyWorldPackages) {
    ProcessPackage(Package);
  }

  for (UPackage* Package : DirtyContentPackages) {
    ProcessPackage(Package);
  }

  auto MakeStringArray = [](const TArray<FString>& Values) {
    TArray<TSharedPtr<FJsonValue>> Result;
    for (const FString& Value : Values) {
      Result.Add(MakeShared<FJsonValueString>(Value));
    }
    return Result;
  };

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetNumberField(TEXT("savedCount"), SavedWorldCount + SavedContentCount);
  Resp->SetNumberField(TEXT("savedWorldCount"), SavedWorldCount);
  Resp->SetNumberField(TEXT("savedContentCount"), SavedContentCount);
  Resp->SetNumberField(TEXT("skippedCount"), SkippedCount);
  Resp->SetNumberField(TEXT("failedCount"), FailedPackages.Num());
  Resp->SetNumberField(TEXT("totalDirty"), TotalDirty);
  Resp->SetArrayField(TEXT("skippedPackages"), MakeStringArray(SkippedPackages));
  Resp->SetArrayField(TEXT("failedPackages"), MakeStringArray(FailedPackages));

  // Only report outer success if the operation actually succeeded
  if (bSuccess || TotalDirty == 0) {
    SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Saved %d world and %d content packages (skipped %d transient/temp)"), SavedWorldCount, SavedContentCount, SkippedCount),
                           Resp, FString());
  } else {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SAVE_FAILED"),
                              FString::Printf(TEXT("Failed to save all packages. Saved %d of %d dirty packages."),
                                               SavedWorldCount + SavedContentCount,
                                               TotalDirty - SkippedCount),
                              Resp);
  }
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorUndo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Execute undo via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("Undo"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("command"), TEXT("Undo"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Undo executed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorRedo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Execute redo via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("Redo"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("command"), TEXT("Redo"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Redo executed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetEditorMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Mode;
  Payload->TryGetStringField(TEXT("mode"), Mode);
  if (Mode.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("mode required"), nullptr);
    return true;
  }

  if (!IsSafeConsoleArgumentToken(Mode)) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("Invalid editor mode"), nullptr);
    return true;
  }

  // Execute editor mode command via console
  FString Command = FString::Printf(TEXT("mode %s"), *Mode);
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("mode"), Mode);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Editor mode set to %s"), *Mode), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorShowStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  UWorld* World = GEditor->GetEditorWorldContext().World();
  TArray<FString> StatsShown;
  if (World) {
    GEditor->Exec(World, TEXT("Stat FPS"));
    StatsShown.Add(TEXT("FPS"));
    GEditor->Exec(World, TEXT("Stat Unit"));
    StatsShown.Add(TEXT("Unit"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> StatsArray;
  for (const FString& Stat : StatsShown) {
    StatsArray.Add(MakeShared<FJsonValueString>(Stat));
  }
  Resp->SetArrayField(TEXT("statsShown"), StatsArray);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Stats displayed"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorHideStats(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  UWorld* World = GEditor->GetEditorWorldContext().World();
  if (World) {
    GEditor->Exec(World, TEXT("Stat None"));
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("command"), TEXT("Stat None"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Stats hidden"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGameView(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);

  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Toggle game view via console command
  GEditor->Exec(GEditor->GetEditorWorldContext().World(),
                bEnabled ? TEXT("ToggleGameView 1") : TEXT("ToggleGameView 0"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("gameViewEnabled"), bEnabled);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Game view %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")),
                         Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetImmersiveMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);

  // Toggle immersive mode - this is viewport-specific
  if (GEditor && GEditor->GetActiveViewport()) {
    FViewport* Viewport = GEditor->GetActiveViewport();
    if (Viewport) {
      // Immersive mode toggle via console
      GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("ToggleImmersive"));
    }
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("immersiveModeEnabled"), bEnabled);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Immersive mode toggled"), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetFixedDeltaTime(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  double DeltaTime = 0.01667; // Default ~60fps
  if (Payload->HasField(TEXT("deltaTime"))) {
    TSharedPtr<FJsonValue> Value = Payload->TryGetField(TEXT("deltaTime"));
    if (Value.IsValid() && Value->Type == EJson::Number) {
      DeltaTime = Value->AsNumber();
    }
  }

  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Set fixed delta time via console
  FString Command = FString::Printf(TEXT("r.FixedDeltaTime %f"), DeltaTime);
  GEditor->Exec(GEditor->GetEditorWorldContext().World(), *Command);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetNumberField(TEXT("fixedDeltaTime"), DeltaTime);
  SendAutomationResponse(Socket, RequestId, true,
                         FString::Printf(TEXT("Fixed delta time set to %f"), DeltaTime), Resp, FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorOpenLevel(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString LevelPath;
  // Accept multiple parameter names for flexibility
  // levelPath is the primary, path and assetPath are aliases
  Payload->TryGetStringField(TEXT("levelPath"), LevelPath);
  if (LevelPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("path"), LevelPath);
  }
  if (LevelPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("assetPath"), LevelPath);
  }
  if (LevelPath.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("levelPath, path, or assetPath required"), nullptr);
    return true;
  }

  // Normalize the level path
  if (!LevelPath.StartsWith(TEXT("/"))) {
    LevelPath = FString::Printf(TEXT("/Game/%s"), *LevelPath);
  }

  LevelPath = SanitizeProjectRelativePath(LevelPath);
  if (LevelPath.IsEmpty() ||
      !(LevelPath.StartsWith(TEXT("/Game/")) || LevelPath.StartsWith(TEXT("/Engine/")))) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SECURITY_VIOLATION"),
                              TEXT("Invalid levelPath"), nullptr);
    return true;
  }

  // Remove map suffix if present
  if (LevelPath.EndsWith(TEXT(".umap"))) {
    LevelPath.LeftChopInline(5);
  }

  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  // Use FEditorFileUtils to load the map
  FString MapPath = LevelPath + TEXT(".umap");

  // CRITICAL FIX: Unreal stores levels in TWO possible path patterns:
  // 1. Folder-based (standard): /Game/Path/LevelName/LevelName.umap
  // 2. Flat (legacy): /Game/Path/LevelName.umap
  // We must check BOTH paths before returning FILE_NOT_FOUND.

  // Build both possible paths
  FString FlatMapPath = LevelPath + TEXT(".umap");
  // Check if path is /Engine/ or /Game/ and extract accordingly
  int32 PrefixLen = 6; // Default: "/Game/" is 6 chars
  FString ContentDir = FPaths::ProjectContentDir();
  if (LevelPath.StartsWith(TEXT("/Engine/"))) {
    PrefixLen = 8; // "/Engine/" is 8 chars
    ContentDir = FPaths::EngineContentDir();
  }
  FString FullFlatMapPath = ContentDir + FlatMapPath.Mid(PrefixLen);
  FullFlatMapPath = FPaths::ConvertRelativePathToFull(FullFlatMapPath);

  // Folder-based path: /Game/Path/LevelName -> /Game/Path/LevelName/LevelName.umap
  FString LevelName = FPaths::GetBaseFilename(LevelPath);
  FString FolderMapPath = LevelPath + TEXT("/") + LevelName + TEXT(".umap");
  FString FullFolderMapPath = ContentDir + FolderMapPath.Mid(PrefixLen);
  FullFolderMapPath = FPaths::ConvertRelativePathToFull(FullFolderMapPath);

  // Check which path exists
  FString MapPathToLoad;
  FString FullMapPath;

  // Prefer folder-based path (Unreal's standard) if it exists
  if (FPaths::FileExists(FullFolderMapPath)) {
    MapPathToLoad = FolderMapPath;
    FullMapPath = FullFolderMapPath;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("OpenLevel: Found level at folder-based path: %s"), *FullFolderMapPath);
  } else if (FPaths::FileExists(FullFlatMapPath)) {
    // Fallback to flat path (legacy format)
    MapPathToLoad = FlatMapPath;
    FullMapPath = FullFlatMapPath;
    UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
           TEXT("OpenLevel: Found level at flat path: %s"), *FullFlatMapPath);
  } else {
    // Neither path exists - return detailed error
    TSharedPtr<FJsonObject> ErrorDetails = McpHandlerUtils::CreateResultObject();
    ErrorDetails->SetStringField(TEXT("levelPath"), LevelPath);
    ErrorDetails->SetStringField(TEXT("checkedFolderBased"), FullFolderMapPath);
    ErrorDetails->SetStringField(TEXT("checkedFlat"), FullFlatMapPath);
    ErrorDetails->SetStringField(TEXT("hint"), TEXT("Unreal levels are typically stored as /Game/Path/LevelName/LevelName.umap"));
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("FILE_NOT_FOUND"),
                              FString::Printf(TEXT("Level file not found. Checked:\n  Folder: %s\n  Flat: %s"),
                                            *FullFolderMapPath, *FullFlatMapPath),
                              ErrorDetails);
    return true;
  }

  if (FApp::IsUnattended() || IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) {
    TArray<UPackage*> DirtyWorldPackages;
    TArray<UPackage*> DirtyContentPackages;
    FEditorFileUtils::GetDirtyWorldPackages(DirtyWorldPackages);
    FEditorFileUtils::GetDirtyContentPackages(DirtyContentPackages);
    auto IsBlockingDirtyPackage = [](UPackage* Package) -> bool {
      if (!Package || Package->HasAnyFlags(RF_Transient)) {
        return false;
      }
      const FString PackagePath = Package->GetPathName();
      return !PackagePath.StartsWith(TEXT("/Temp/")) &&
             !PackagePath.StartsWith(TEXT("/Transient/")) &&
             !PackagePath.StartsWith(TEXT("/Engine/Transient"));
    };
    int32 BlockingWorldPackages = 0;
    int32 BlockingContentPackages = 0;
    for (UPackage* Package : DirtyWorldPackages) {
      if (IsBlockingDirtyPackage(Package)) {
        BlockingWorldPackages++;
      }
    }
    for (UPackage* Package : DirtyContentPackages) {
      if (IsBlockingDirtyPackage(Package)) {
        BlockingContentPackages++;
      }
    }
    if (BlockingWorldPackages + BlockingContentPackages > 0) {
      TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
      Details->SetNumberField(TEXT("dirtyWorldPackages"), BlockingWorldPackages);
      Details->SetNumberField(TEXT("dirtyContentPackages"), BlockingContentPackages);
      Details->SetStringField(TEXT("levelPath"), LevelPath);
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("DIRTY_PACKAGES"),
                                TEXT("Cannot open a level in unattended/headless mode while packages are dirty. Save or discard changes first."),
                                Details);
      return true;
    }
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakThis, Socket, RequestId, LevelPath,
                                     MapPathToLoad](float) {
        if (!WeakThis.IsValid()) {
          return false;
        }

        bool bOpened = McpSafeLoadMap(MapPathToLoad);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), bOpened);
        Resp->SetStringField(TEXT("levelPath"), LevelPath);
        Resp->SetStringField(TEXT("loadedPath"), MapPathToLoad);

        if (bOpened) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Display,
                 TEXT("OpenLevel: Successfully opened level: %s"), *MapPathToLoad);
          WeakThis->SendAutomationResponse(Socket, RequestId, true,
                                           TEXT("Level opened"), Resp, FString());
        } else {
          SendStandardErrorResponse(WeakThis.Get(), Socket, RequestId,
                                    TEXT("OPEN_FAILED"),
                                    TEXT("Failed to open level"), Resp);
        }

        return false;
      }),
      0.0f);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorList(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);

  double LimitValue = 0.0;
  Payload->TryGetNumberField(TEXT("limit"), LimitValue);
  const int32 Limit = LimitValue > 0.0
      ? FMath::Max(1, static_cast<int32>(LimitValue))
      : 0;

  TArray<AActor *> AllActors;
  UWorld *SourceWorld = nullptr;
  bool bUsingPieWorld = false;

  if (GEditor->PlayWorld) {
    SourceWorld = GEditor->PlayWorld.Get();
    bUsingPieWorld = true;
    if (!SourceWorld) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("WORLD_NOT_FOUND"),
                                TEXT("PIE world unavailable"), nullptr);
      return true;
    }

    for (TActorIterator<AActor> It(SourceWorld); It; ++It) {
      AllActors.Add(*It);
    }
  } else {
    SourceWorld = GEditor->GetEditorWorldContext().World();
    UEditorActorSubsystem *ActorSS =
        GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSS) {
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("SUBSYSTEM_MISSING"),
                                TEXT("EditorActorSubsystem unavailable"), nullptr);
      return true;
    }

    AllActors = ActorSS->GetAllLevelActors();
  }

  TArray<TSharedPtr<FJsonValue>> ActorsArray;
  int32 TotalCount = 0;

  for (AActor *Actor : AllActors) {
    if (!Actor)
      continue;
    const FString Label = Actor->GetActorLabel();
    const FString Name = Actor->GetName();
    if (!Filter.IsEmpty() &&
        !Label.Contains(Filter, ESearchCase::IgnoreCase) &&
        !Name.Contains(Filter, ESearchCase::IgnoreCase))
      continue;
    ++TotalCount;

    if (Limit > 0 && ActorsArray.Num() >= Limit)
      continue;

    TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
    Entry->SetStringField(TEXT("label"), Label);
    Entry->SetStringField(TEXT("name"), Name);
    Entry->SetStringField(TEXT("path"), Actor->GetPathName());
    Entry->SetStringField(TEXT("class"), Actor->GetClass()
                                             ? Actor->GetClass()->GetPathName()
                                             : TEXT(""));
    ActorsArray.Add(MakeShared<FJsonValueObject>(Entry));
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetArrayField(TEXT("actors"), ActorsArray);
  Data->SetNumberField(TEXT("count"), ActorsArray.Num());
  Data->SetNumberField(TEXT("totalCount"), TotalCount);
  Data->SetBoolField(TEXT("isPieWorld"), bUsingPieWorld);
  if (SourceWorld)
    Data->SetStringField(TEXT("worldName"), SourceWorld->GetName());
  if (!Filter.IsEmpty())
    Data->SetStringField(TEXT("filter"), Filter);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Actors listed"), Data);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGet(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  if (TargetName.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                              TEXT("actorName required"), nullptr);
    return true;
  }

  AActor *Found = FindActorByName(TargetName);
  if (!Found) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("ACTOR_NOT_FOUND"),
                              TEXT("Actor not found"), nullptr);
    return true;
  }

  const FTransform Current = Found->GetActorTransform();
  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetStringField(TEXT("name"), Found->GetName());
  Data->SetStringField(TEXT("label"), Found->GetActorLabel());
  Data->SetStringField(TEXT("path"), Found->GetPathName());
  Data->SetStringField(TEXT("class"), Found->GetClass()
                                          ? Found->GetClass()->GetPathName()
                                          : TEXT(""));

  TArray<TSharedPtr<FJsonValue>> TagsArray;
  for (const FName &Tag : Found->Tags) {
    TagsArray.Add(MakeShared<FJsonValueString>(Tag.ToString()));
  }
  Data->SetArrayField(TEXT("tags"), TagsArray);

  auto MakeArray = [](const FVector &Vec) -> TArray<TSharedPtr<FJsonValue>> {
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.X));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Y));
    Arr.Add(MakeShared<FJsonValueNumber>(Vec.Z));
    return Arr;
  };
  Data->SetArrayField(TEXT("location"), MakeArray(Current.GetLocation()));
  Data->SetArrayField(TEXT("scale"), MakeArray(Current.GetScale3D()));

  SendStandardSuccessResponse(this, Socket, RequestId, TEXT("Actor retrieved"),
                              Data);
  return true;
#else
  return false;
#endif
}
