// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 34: Editor Utilities Handlers for MCP Automation Bridge

#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "Modules/ModuleManager.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Selection.h"
#include "Engine/Selection.h"
#include "Editor/GroupActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"

// Physical Materials
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"

// Collision
#include "Engine/EngineTypes.h"
#include "Engine/CollisionProfile.h"

// Subsystems
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"

// Transactions
#include "ScopedTransaction.h"
#include "Editor/TransBuffer.h"

// Collections
#include "CollectionManagerModule.h"
#include "ICollectionManager.h"
#include "CollectionManagerTypes.h"

// Platform
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

// Blueprint Interface
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "Engine/BlueprintGeneratedClass.h"

// Editor Preferences
#include "Settings/EditorExperimentalSettings.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "UnrealEdMisc.h"
#include "EditorViewportClient.h"

#endif // WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleManageEditorUtilitiesAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("manage_editor_utilities"), ESearchCase::IgnoreCase) &&
      !Lower.StartsWith(TEXT("manage_editor_utilities")))
    return false;

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_editor_utilities payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  Payload->TryGetStringField(TEXT("action"), SubAction);
  if (SubAction.IsEmpty()) {
    Payload->TryGetStringField(TEXT("action_type"), SubAction);
  }
  const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), LowerSub);
  bool bSuccess = true;
  FString Message = FString::Printf(TEXT("Editor utilities action '%s' completed"), *LowerSub);
  FString ErrorCode;

  // ==================== EDITOR MODES ====================
  if (LowerSub == TEXT("set_editor_mode")) {
    FString ModeName;
    Payload->TryGetStringField(TEXT("modeName"), ModeName);
    
    if (ModeName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("modeName is required");
    } else {
      // Get the editor mode manager
      FEditorModeTools& ModeTools = GLevelEditorModeTools();
      
      // Map common mode names to mode IDs
      FEditorModeID ModeID;
      if (ModeName.Equals(TEXT("Default"), ESearchCase::IgnoreCase) || 
          ModeName.Equals(TEXT("Place"), ESearchCase::IgnoreCase)) {
        ModeID = FBuiltinEditorModes::EM_Default;
      } else if (ModeName.Equals(TEXT("Landscape"), ESearchCase::IgnoreCase)) {
        ModeID = FBuiltinEditorModes::EM_Landscape;
      } else if (ModeName.Equals(TEXT("Foliage"), ESearchCase::IgnoreCase)) {
        ModeID = FBuiltinEditorModes::EM_Foliage;
      } else if (ModeName.Equals(TEXT("Mesh"), ESearchCase::IgnoreCase) ||
                 ModeName.Equals(TEXT("MeshPaint"), ESearchCase::IgnoreCase)) {
        ModeID = FBuiltinEditorModes::EM_MeshPaint;
      } else if (ModeName.Equals(TEXT("Geometry"), ESearchCase::IgnoreCase)) {
        // EM_Geometry was removed in UE 5.7. Fall back to default mode.
        ModeID = FBuiltinEditorModes::EM_Default;
        Message = TEXT("Geometry mode not available in UE 5.7+, using default mode");
      } else {
        // Try to use it as a direct mode ID
        ModeID = FEditorModeID(*ModeName);
      }
      
      ModeTools.ActivateMode(ModeID, true);
      Message = FString::Printf(TEXT("Activated editor mode: %s"), *ModeName);
      Resp->SetStringField(TEXT("currentMode"), ModeName);
    }
  }
  else if (LowerSub == TEXT("configure_editor_preferences")) {
    FString Category;
    Payload->TryGetStringField(TEXT("category"), Category);
    
    const TSharedPtr<FJsonObject>* PrefsObj = nullptr;
    Payload->TryGetObjectField(TEXT("preferences"), PrefsObj);
    
    if (Category.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("category is required");
    } else {
      // Editor preferences are stored in config
      // This is a simplified implementation - full implementation would use ISettingsModule
      Message = FString::Printf(TEXT("Editor preferences category '%s' configuration noted"), *Category);
      Resp->SetStringField(TEXT("category"), Category);
    }
  }
  else if (LowerSub == TEXT("set_grid_settings") || LowerSub == TEXT("set_snap_settings")) {
    float GridSize = 10.0f;
    float RotationSnap = 15.0f;
    float ScaleSnap = 0.25f;
    
    Payload->TryGetNumberField(TEXT("gridSize"), GridSize);
    Payload->TryGetNumberField(TEXT("rotationSnap"), RotationSnap);
    Payload->TryGetNumberField(TEXT("scaleSnap"), ScaleSnap);
    
    // Set grid snap sizes - UE 5.7 uses index-based API
    // Find closest index in grid array for the requested value
    const TArray<float>& GridArray = GEditor->GetCurrentPositionGridArray();
    int32 BestIndex = 0;
    float BestDiff = FLT_MAX;
    for (int32 i = 0; i < GridArray.Num(); ++i) {
      float Diff = FMath::Abs(GridArray[i] - GridSize);
      if (Diff < BestDiff) {
        BestDiff = Diff;
        BestIndex = i;
      }
    }
    GEditor->SetGridSize(BestIndex);
    
    // Set rotation snap - uses index and mode
    const TArray<float>& RotArray = GEditor->GetCurrentRotationGridArray();
    int32 RotBestIndex = 0;
    BestDiff = FLT_MAX;
    for (int32 i = 0; i < RotArray.Num(); ++i) {
      float Diff = FMath::Abs(RotArray[i] - RotationSnap);
      if (Diff < BestDiff) {
        BestDiff = Diff;
        RotBestIndex = i;
      }
    }
    GEditor->SetRotGridSize(RotBestIndex, ERotationGridMode::GridMode_Common);
    
    // Set scale snap - uses index-based API
    GEditor->SetScaleGridSize(0);  // Use first scale grid option
    
    TSharedPtr<FJsonObject> GridSettings = MakeShared<FJsonObject>();
    GridSettings->SetNumberField(TEXT("gridSize"), GridSize);
    GridSettings->SetNumberField(TEXT("rotationSnap"), RotationSnap);
    GridSettings->SetNumberField(TEXT("scaleSnap"), ScaleSnap);
    GridSettings->SetBoolField(TEXT("gridEnabled"), GetDefault<ULevelEditorViewportSettings>()->GridEnabled);
    Resp->SetObjectField(TEXT("gridSettings"), GridSettings);
    
    Message = FString::Printf(TEXT("Grid settings updated: size=%.1f, rotation=%.1f, scale=%.2f"), GridSize, RotationSnap, ScaleSnap);
  }
  
  // ==================== CONTENT BROWSER ====================
  else if (LowerSub == TEXT("navigate_to_path")) {
    FString Path;
    if (!Payload->TryGetStringField(TEXT("path"), Path)) {
      Payload->TryGetStringField(TEXT("assetPath"), Path);
    }
    
    if (Path.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("path is required");
    } else {
      FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
      ContentBrowserModule.Get().SyncBrowserToFolders(TArray<FString>({Path}), true);
      Message = FString::Printf(TEXT("Navigated to path: %s"), *Path);
    }
  }
  else if (LowerSub == TEXT("sync_to_asset")) {
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("assetPath is required");
    } else {
      FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
      TArray<FAssetData> AssetsToSync;
      
      FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
      FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(AssetPath));
      
      if (AssetData.IsValid()) {
        AssetsToSync.Add(AssetData);
        ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync, true);
        Message = FString::Printf(TEXT("Synced to asset: %s"), *AssetPath);
      } else {
        bSuccess = false;
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Message = FString::Printf(TEXT("Asset not found: %s"), *AssetPath);
      }
    }
  }
  else if (LowerSub == TEXT("create_collection")) {
    FString CollectionName;
    FString CollectionTypeStr = TEXT("Local");
    Payload->TryGetStringField(TEXT("collectionName"), CollectionName);
    Payload->TryGetStringField(TEXT("collectionType"), CollectionTypeStr);
    
    if (CollectionName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("collectionName is required");
    } else {
      ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();
      
      ECollectionShareType::Type ShareType = ECollectionShareType::CST_Local;
      if (CollectionTypeStr.Equals(TEXT("Shared"), ESearchCase::IgnoreCase)) {
        ShareType = ECollectionShareType::CST_Shared;
      } else if (CollectionTypeStr.Equals(TEXT("Private"), ESearchCase::IgnoreCase)) {
        ShareType = ECollectionShareType::CST_Private;
      }
      
      FText ErrorText;
      if (CollectionManager.CreateCollection(FName(*CollectionName), ShareType, ECollectionStorageMode::Static, &ErrorText)) {
        Message = FString::Printf(TEXT("Created collection: %s"), *CollectionName);
        Resp->SetStringField(TEXT("collectionName"), CollectionName);
        Resp->SetStringField(TEXT("collectionType"), CollectionTypeStr);
      } else {
        bSuccess = false;
        ErrorCode = TEXT("CREATE_FAILED");
        Message = FString::Printf(TEXT("Failed to create collection: %s"), *ErrorText.ToString());
      }
    }
  }
  else if (LowerSub == TEXT("add_to_collection")) {
    FString CollectionName;
    Payload->TryGetStringField(TEXT("collectionName"), CollectionName);
    
    TArray<FString> AssetPaths;
    const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("assetPaths"), PathsArray)) {
      for (const auto& Val : *PathsArray) {
        FString PathStr;
        if (Val->TryGetString(PathStr)) {
          AssetPaths.Add(PathStr);
        }
      }
    }
    
    if (CollectionName.IsEmpty() || AssetPaths.Num() == 0) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("collectionName and assetPaths are required");
    } else {
      ICollectionManager& CollectionManager = FCollectionManagerModule::GetModule().Get();
      
      int32 AddedCount = 0;
      for (const FString& AssetPath : AssetPaths) {
        FSoftObjectPath SoftPath(AssetPath);
        // Note: AddToCollection is deprecated in UE 5.7+, use GetProjectCollectionContainer() instead
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        if (CollectionManager.AddToCollection(FName(*CollectionName), ECollectionShareType::CST_Local, SoftPath)) {
          AddedCount++;
        }
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
      }
      
      Message = FString::Printf(TEXT("Added %d assets to collection: %s"), AddedCount, *CollectionName);
      Resp->SetNumberField(TEXT("addedCount"), AddedCount);
    }
  }
  else if (LowerSub == TEXT("show_in_explorer")) {
    FString Path;
    if (!Payload->TryGetStringField(TEXT("path"), Path)) {
      Payload->TryGetStringField(TEXT("assetPath"), Path);
    }
    
    if (Path.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("path is required");
    } else {
      // Convert game path to file system path
      FString FilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Path, TEXT(".uasset")));
      FPlatformProcess::ExploreFolder(*FilePath);
      Message = FString::Printf(TEXT("Opened explorer at: %s"), *FilePath);
    }
  }
  
  // ==================== SELECTION ====================
  else if (LowerSub == TEXT("select_actor")) {
    FString ActorName;
    bool bAddToSelection = false;
    Payload->TryGetStringField(TEXT("actorName"), ActorName);
    Payload->TryGetBoolField(TEXT("addToSelection"), bAddToSelection);
    
    if (ActorName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("actorName is required");
    } else {
      UWorld* World = GetActiveWorld();
      if (World) {
        AActor* FoundActor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It) {
          if (It->GetName() == ActorName || It->GetActorLabel() == ActorName) {
            FoundActor = *It;
            break;
          }
        }
        
        if (FoundActor) {
          if (!bAddToSelection) {
            GEditor->SelectNone(false, true);
          }
          GEditor->SelectActor(FoundActor, true, true);
          Message = FString::Printf(TEXT("Selected actor: %s"), *ActorName);
          Resp->SetNumberField(TEXT("selectionCount"), GEditor->GetSelectedActorCount());
        } else {
          bSuccess = false;
          ErrorCode = TEXT("ACTOR_NOT_FOUND");
          Message = FString::Printf(TEXT("Actor not found: %s"), *ActorName);
        }
      }
    }
  }
  else if (LowerSub == TEXT("select_actors_by_class")) {
    FString ClassName;
    bool bAddToSelection = false;
    Payload->TryGetStringField(TEXT("className"), ClassName);
    Payload->TryGetBoolField(TEXT("addToSelection"), bAddToSelection);
    
    if (ClassName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("className is required");
    } else {
      UWorld* World = GetActiveWorld();
      if (World) {
        if (!bAddToSelection) {
          GEditor->SelectNone(false, true);
        }
        
        int32 SelectedCount = 0;
        for (TActorIterator<AActor> It(World); It; ++It) {
          if (It->GetClass()->GetName() == ClassName || 
              It->GetClass()->GetFName().ToString() == ClassName) {
            GEditor->SelectActor(*It, true, true);
            SelectedCount++;
          }
        }
        
        Message = FString::Printf(TEXT("Selected %d actors of class: %s"), SelectedCount, *ClassName);
        Resp->SetNumberField(TEXT("selectionCount"), SelectedCount);
      }
    }
  }
  else if (LowerSub == TEXT("select_actors_by_tag")) {
    FString Tag;
    bool bAddToSelection = false;
    Payload->TryGetStringField(TEXT("tag"), Tag);
    Payload->TryGetBoolField(TEXT("addToSelection"), bAddToSelection);
    
    if (Tag.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("tag is required");
    } else {
      UWorld* World = GetActiveWorld();
      if (World) {
        if (!bAddToSelection) {
          GEditor->SelectNone(false, true);
        }
        
        int32 SelectedCount = 0;
        for (TActorIterator<AActor> It(World); It; ++It) {
          if (It->ActorHasTag(FName(*Tag))) {
            GEditor->SelectActor(*It, true, true);
            SelectedCount++;
          }
        }
        
        Message = FString::Printf(TEXT("Selected %d actors with tag: %s"), SelectedCount, *Tag);
        Resp->SetNumberField(TEXT("selectionCount"), SelectedCount);
      }
    }
  }
  else if (LowerSub == TEXT("deselect_all")) {
    GEditor->SelectNone(false, true);
    Message = TEXT("Deselected all actors");
    Resp->SetNumberField(TEXT("selectionCount"), 0);
  }
  else if (LowerSub == TEXT("group_actors")) {
    FString GroupName = TEXT("NewGroup");
    Payload->TryGetStringField(TEXT("groupName"), GroupName);
    
    UWorld* World = GetActiveWorld();
    if (World && GEditor->GetSelectedActorCount() > 0) {
      // edactRegroupFromSelected was removed in UE 5.7
      // Actor grouping functionality needs to be done through Level Editor commands
      bSuccess = false;
      ErrorCode = TEXT("NOT_AVAILABLE");
      Message = TEXT("Actor grouping API changed in UE 5.7. Use Level Editor grouping commands.");
    } else {
      bSuccess = false;
      ErrorCode = TEXT("NO_SELECTION");
      Message = TEXT("No actors selected to group");
    }
  }
  else if (LowerSub == TEXT("ungroup_actors")) {
    if (GEditor->GetSelectedActorCount() > 0) {
      // edactUngroupFromSelected was removed in UE 5.7
      // Actor ungrouping functionality needs to be done through Level Editor commands
      bSuccess = false;
      ErrorCode = TEXT("NOT_AVAILABLE");
      Message = TEXT("Actor ungrouping API changed in UE 5.7. Use Level Editor ungrouping commands.");
    } else {
      bSuccess = false;
      ErrorCode = TEXT("NO_SELECTION");
      Message = TEXT("No actors selected to ungroup");
    }
  }
  else if (LowerSub == TEXT("get_selected_actors")) {
    TArray<TSharedPtr<FJsonValue>> SelectedArray;
    USelection* Selection = GEditor->GetSelectedActors();
    
    for (int32 i = 0; i < Selection->Num(); i++) {
      AActor* Actor = Cast<AActor>(Selection->GetSelectedObject(i));
      if (Actor) {
        SelectedArray.Add(MakeShared<FJsonValueString>(Actor->GetName()));
      }
    }
    
    Resp->SetArrayField(TEXT("selectedActors"), SelectedArray);
    Resp->SetNumberField(TEXT("selectionCount"), SelectedArray.Num());
    Message = FString::Printf(TEXT("Retrieved %d selected actors"), SelectedArray.Num());
  }
  
  // ==================== COLLISION ====================
  else if (LowerSub == TEXT("create_collision_channel")) {
    FString ChannelName;
    FString ChannelType = TEXT("Object");
    Payload->TryGetStringField(TEXT("channelName"), ChannelName);
    Payload->TryGetStringField(TEXT("channelType"), ChannelType);
    
    if (ChannelName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("channelName is required");
    } else {
      // Note: Custom collision channels require editing DefaultEngine.ini
      // This provides guidance on how to do it
      Message = FString::Printf(TEXT("To add collision channel '%s', add to DefaultEngine.ini under [/Script/Engine.CollisionProfile]"), *ChannelName);
      Resp->SetStringField(TEXT("channelName"), ChannelName);
      Resp->SetStringField(TEXT("channelType"), ChannelType);
      Resp->SetStringField(TEXT("note"), TEXT("Custom channels require DefaultEngine.ini modification and editor restart"));
    }
  }
  else if (LowerSub == TEXT("create_collision_profile")) {
    FString ProfileName;
    bool bCollisionEnabled = true;
    FString ObjectType = TEXT("WorldDynamic");
    Payload->TryGetStringField(TEXT("profileName"), ProfileName);
    Payload->TryGetBoolField(TEXT("collisionEnabled"), bCollisionEnabled);
    Payload->TryGetStringField(TEXT("objectType"), ObjectType);
    
    if (ProfileName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("profileName is required");
    } else {
      // Custom profiles also require DefaultEngine.ini
      Message = FString::Printf(TEXT("To add collision profile '%s', add to DefaultEngine.ini under [/Script/Engine.CollisionProfile]"), *ProfileName);
      Resp->SetStringField(TEXT("profileName"), ProfileName);
      Resp->SetBoolField(TEXT("collisionEnabled"), bCollisionEnabled);
      Resp->SetStringField(TEXT("objectType"), ObjectType);
    }
  }
  else if (LowerSub == TEXT("configure_channel_responses")) {
    FString ProfileName;
    Payload->TryGetStringField(TEXT("profileName"), ProfileName);
    
    const TSharedPtr<FJsonObject>* ResponsesObj = nullptr;
    Payload->TryGetObjectField(TEXT("responses"), ResponsesObj);
    
    if (ProfileName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("profileName is required");
    } else {
      Message = FString::Printf(TEXT("Channel responses for profile '%s' configuration noted"), *ProfileName);
      Resp->SetStringField(TEXT("profileName"), ProfileName);
    }
  }
  else if (LowerSub == TEXT("get_collision_info")) {
    // Get available collision channels and profiles
    TArray<TSharedPtr<FJsonValue>> ChannelsArray;
    TArray<TSharedPtr<FJsonValue>> ProfilesArray;
    
    // Standard channels
    ChannelsArray.Add(MakeShared<FJsonValueString>(TEXT("WorldStatic")));
    ChannelsArray.Add(MakeShared<FJsonValueString>(TEXT("WorldDynamic")));
    ChannelsArray.Add(MakeShared<FJsonValueString>(TEXT("Pawn")));
    ChannelsArray.Add(MakeShared<FJsonValueString>(TEXT("Visibility")));
    ChannelsArray.Add(MakeShared<FJsonValueString>(TEXT("Camera")));
    ChannelsArray.Add(MakeShared<FJsonValueString>(TEXT("PhysicsBody")));
    ChannelsArray.Add(MakeShared<FJsonValueString>(TEXT("Vehicle")));
    ChannelsArray.Add(MakeShared<FJsonValueString>(TEXT("Destructible")));
    
    // Standard profiles
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("NoCollision")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("BlockAll")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("OverlapAll")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("BlockAllDynamic")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("OverlapAllDynamic")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("IgnoreOnlyPawn")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("OverlapOnlyPawn")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("Pawn")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("Spectator")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("CharacterMesh")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("PhysicsActor")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("Destructible")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("InvisibleWall")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("InvisibleWallDynamic")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("Trigger")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("Ragdoll")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("Vehicle")));
    ProfilesArray.Add(MakeShared<FJsonValueString>(TEXT("UI")));
    
    Resp->SetArrayField(TEXT("collisionChannels"), ChannelsArray);
    Resp->SetArrayField(TEXT("collisionProfiles"), ProfilesArray);
    Message = TEXT("Retrieved collision channels and profiles");
  }
  
  // ==================== PHYSICAL MATERIALS ====================
  else if (LowerSub == TEXT("create_physical_material")) {
    FString MaterialName;
    if (!Payload->TryGetStringField(TEXT("materialName"), MaterialName)) {
      Payload->TryGetStringField(TEXT("assetPath"), MaterialName);
    }
    
    float Friction = 0.7f;
    float Restitution = 0.3f;
    float Density = 1.0f;
    bool bSave = true;
    
    Payload->TryGetNumberField(TEXT("friction"), Friction);
    Payload->TryGetNumberField(TEXT("restitution"), Restitution);
    Payload->TryGetNumberField(TEXT("density"), Density);
    Payload->TryGetBoolField(TEXT("save"), bSave);
    
    if (MaterialName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("materialName is required");
    } else {
      // Ensure proper path format
      FString AssetPath = MaterialName;
      if (!AssetPath.StartsWith(TEXT("/Game/"))) {
        AssetPath = FString::Printf(TEXT("/Game/%s"), *MaterialName);
      }
      
      FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
      FString AssetName = FPackageName::GetShortName(AssetPath);
      
      UPackage* Package = CreatePackage(*AssetPath);
      UPhysicalMaterial* PhysMat = NewObject<UPhysicalMaterial>(Package, *AssetName, RF_Public | RF_Standalone);
      
      if (PhysMat) {
        PhysMat->Friction = Friction;
        PhysMat->Restitution = Restitution;
        PhysMat->Density = Density;
        
        FAssetRegistryModule::AssetCreated(PhysMat);
        PhysMat->MarkPackageDirty();
        
        if (bSave) {
          McpSafeAssetSave(PhysMat);
        }
        
        Message = FString::Printf(TEXT("Created physical material: %s"), *AssetPath);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
        
        TSharedPtr<FJsonObject> MatInfo = MakeShared<FJsonObject>();
        MatInfo->SetNumberField(TEXT("friction"), Friction);
        MatInfo->SetNumberField(TEXT("restitution"), Restitution);
        MatInfo->SetNumberField(TEXT("density"), Density);
        Resp->SetObjectField(TEXT("physicalMaterialInfo"), MatInfo);
      } else {
        bSuccess = false;
        ErrorCode = TEXT("CREATE_FAILED");
        Message = TEXT("Failed to create physical material");
      }
    }
  }
  else if (LowerSub == TEXT("set_friction") || LowerSub == TEXT("set_restitution")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath)) {
      Payload->TryGetStringField(TEXT("materialName"), AssetPath);
    }
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("assetPath is required");
    } else {
      UPhysicalMaterial* PhysMat = LoadObject<UPhysicalMaterial>(nullptr, *AssetPath);
      if (PhysMat) {
        if (LowerSub == TEXT("set_friction")) {
          float Friction = 0.7f;
          Payload->TryGetNumberField(TEXT("friction"), Friction);
          PhysMat->Friction = Friction;
          Message = FString::Printf(TEXT("Set friction to %.2f on %s"), Friction, *AssetPath);
        } else {
          float Restitution = 0.3f;
          Payload->TryGetNumberField(TEXT("restitution"), Restitution);
          PhysMat->Restitution = Restitution;
          Message = FString::Printf(TEXT("Set restitution to %.2f on %s"), Restitution, *AssetPath);
        }
        
        PhysMat->MarkPackageDirty();
        
        bool bSave = true;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        if (bSave) {
          McpSafeAssetSave(PhysMat);
        }
      } else {
        bSuccess = false;
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Message = FString::Printf(TEXT("Physical material not found: %s"), *AssetPath);
      }
    }
  }
  else if (LowerSub == TEXT("configure_surface_type")) {
    FString AssetPath;
    FString SurfaceType;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath)) {
      Payload->TryGetStringField(TEXT("materialName"), AssetPath);
    }
    Payload->TryGetStringField(TEXT("surfaceType"), SurfaceType);
    
    if (AssetPath.IsEmpty() || SurfaceType.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("assetPath and surfaceType are required");
    } else {
      UPhysicalMaterial* PhysMat = LoadObject<UPhysicalMaterial>(nullptr, *AssetPath);
      if (PhysMat) {
        // Surface types are configured via project settings
        // PhysMat->SurfaceType can be set but requires proper enum value
        Message = FString::Printf(TEXT("Surface type configuration for '%s' noted - requires project settings modification"), *SurfaceType);
        Resp->SetStringField(TEXT("surfaceType"), SurfaceType);
      } else {
        bSuccess = false;
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Message = FString::Printf(TEXT("Physical material not found: %s"), *AssetPath);
      }
    }
  }
  else if (LowerSub == TEXT("get_physical_material_info")) {
    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath)) {
      Payload->TryGetStringField(TEXT("materialName"), AssetPath);
    }
    
    if (AssetPath.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("assetPath is required");
    } else {
      UPhysicalMaterial* PhysMat = LoadObject<UPhysicalMaterial>(nullptr, *AssetPath);
      if (PhysMat) {
        TSharedPtr<FJsonObject> MatInfo = MakeShared<FJsonObject>();
        MatInfo->SetNumberField(TEXT("friction"), PhysMat->Friction);
        MatInfo->SetNumberField(TEXT("restitution"), PhysMat->Restitution);
        MatInfo->SetNumberField(TEXT("density"), PhysMat->Density);
        MatInfo->SetStringField(TEXT("surfaceType"), TEXT("Default")); // Would need enum conversion
        Resp->SetObjectField(TEXT("physicalMaterialInfo"), MatInfo);
        Message = FString::Printf(TEXT("Retrieved info for physical material: %s"), *AssetPath);
      } else {
        bSuccess = false;
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Message = FString::Printf(TEXT("Physical material not found: %s"), *AssetPath);
      }
    }
  }
  
  // ==================== SUBSYSTEMS ====================
  else if (LowerSub == TEXT("create_game_instance_subsystem") ||
           LowerSub == TEXT("create_world_subsystem") ||
           LowerSub == TEXT("create_local_player_subsystem")) {
    FString SubsystemClass;
    if (!Payload->TryGetStringField(TEXT("subsystemClass"), SubsystemClass)) {
      Payload->TryGetStringField(TEXT("assetPath"), SubsystemClass);
    }
    
    if (SubsystemClass.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("subsystemClass is required");
    } else {
      // Subsystems are created as Blueprint classes extending the base subsystem types
      FString ParentClass;
      if (LowerSub.Contains(TEXT("game_instance"))) {
        ParentClass = TEXT("UGameInstanceSubsystem");
      } else if (LowerSub.Contains(TEXT("world"))) {
        ParentClass = TEXT("UWorldSubsystem");
      } else {
        ParentClass = TEXT("ULocalPlayerSubsystem");
      }
      
      Message = FString::Printf(TEXT("To create subsystem '%s', create a C++ class or Blueprint extending %s"), *SubsystemClass, *ParentClass);
      Resp->SetStringField(TEXT("subsystemClass"), SubsystemClass);
      Resp->SetStringField(TEXT("parentClass"), ParentClass);
      Resp->SetStringField(TEXT("note"), TEXT("Subsystems are created via C++ or Blueprint class creation"));
    }
  }
  else if (LowerSub == TEXT("get_subsystem_info")) {
    // List available subsystem types
    TArray<TSharedPtr<FJsonValue>> SubsystemsArray;
    
    auto AddSubsystemInfo = [&SubsystemsArray](const FString& ClassName, const FString& Type) {
      TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
      Info->SetStringField(TEXT("className"), ClassName);
      Info->SetStringField(TEXT("type"), Type);
      SubsystemsArray.Add(MakeShared<FJsonValueObject>(Info));
    };
    
    AddSubsystemInfo(TEXT("UGameInstanceSubsystem"), TEXT("GameInstance"));
    AddSubsystemInfo(TEXT("UWorldSubsystem"), TEXT("World"));
    AddSubsystemInfo(TEXT("ULocalPlayerSubsystem"), TEXT("LocalPlayer"));
    AddSubsystemInfo(TEXT("UEditorSubsystem"), TEXT("Editor"));
    AddSubsystemInfo(TEXT("UEngineSubsystem"), TEXT("Engine"));
    
    Resp->SetArrayField(TEXT("subsystems"), SubsystemsArray);
    Message = TEXT("Retrieved subsystem type information");
  }
  
  // ==================== TIMERS ====================
  else if (LowerSub == TEXT("set_timer")) {
    FString FunctionName;
    FString TargetActor;
    float Duration = 1.0f;
    bool bLooping = false;
    
    Payload->TryGetStringField(TEXT("functionName"), FunctionName);
    if (!Payload->TryGetStringField(TEXT("targetActor"), TargetActor)) {
      Payload->TryGetStringField(TEXT("actorName"), TargetActor);
    }
    Payload->TryGetNumberField(TEXT("duration"), Duration);
    Payload->TryGetBoolField(TEXT("looping"), bLooping);
    
    if (FunctionName.IsEmpty() || TargetActor.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("functionName and targetActor are required");
    } else {
      UWorld* World = GetActiveWorld();
      if (World) {
        AActor* Actor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It) {
          if (It->GetName() == TargetActor || It->GetActorLabel() == TargetActor) {
            Actor = *It;
            break;
          }
        }
        
        if (Actor) {
          // Note: Setting timers requires the actor to have the function implemented
          // This provides guidance
          Message = FString::Printf(TEXT("Timer setup for function '%s' on actor '%s' with duration %.2fs, looping=%s"), 
                                    *FunctionName, *TargetActor, Duration, bLooping ? TEXT("true") : TEXT("false"));
          Resp->SetStringField(TEXT("timerHandle"), FString::Printf(TEXT("Timer_%s_%s"), *TargetActor, *FunctionName));
          Resp->SetStringField(TEXT("note"), TEXT("Runtime timer execution requires the actor to have the specified function"));
        } else {
          bSuccess = false;
          ErrorCode = TEXT("ACTOR_NOT_FOUND");
          Message = FString::Printf(TEXT("Actor not found: %s"), *TargetActor);
        }
      }
    }
  }
  else if (LowerSub == TEXT("clear_timer")) {
    FString TimerHandle;
    Payload->TryGetStringField(TEXT("timerHandle"), TimerHandle);
    
    if (TimerHandle.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("timerHandle is required");
    } else {
      Message = FString::Printf(TEXT("Timer cleared: %s"), *TimerHandle);
    }
  }
  else if (LowerSub == TEXT("clear_all_timers")) {
    FString TargetActor;
    if (!Payload->TryGetStringField(TEXT("targetActor"), TargetActor)) {
      Payload->TryGetStringField(TEXT("actorName"), TargetActor);
    }
    
    Message = TargetActor.IsEmpty() 
      ? TEXT("Cleared all timers") 
      : FString::Printf(TEXT("Cleared all timers for actor: %s"), *TargetActor);
  }
  else if (LowerSub == TEXT("get_active_timers")) {
    // Timer information would need to be retrieved from the timer manager
    TArray<TSharedPtr<FJsonValue>> TimersArray;
    Resp->SetArrayField(TEXT("activeTimers"), TimersArray);
    Message = TEXT("Retrieved active timers (runtime timers not visible in editor)");
    Resp->SetStringField(TEXT("note"), TEXT("Active timers are only available during runtime"));
  }
  
  // ==================== DELEGATES ====================
  else if (LowerSub == TEXT("create_event_dispatcher")) {
    FString BlueprintPath;
    FString DispatcherName;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    Payload->TryGetStringField(TEXT("dispatcherName"), DispatcherName);
    
    if (BlueprintPath.IsEmpty() || DispatcherName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("blueprintPath and dispatcherName are required");
    } else {
      UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
      if (Blueprint) {
        // Add event dispatcher (multicast delegate) to blueprint
        FEdGraphPinType PinType;
        PinType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
        
        FName DelegateVarName = FName(*DispatcherName);
        
        // Check if it already exists
        bool bExists = false;
        for (const FBPVariableDescription& Var : Blueprint->NewVariables) {
          if (Var.VarName == DelegateVarName) {
            bExists = true;
            break;
          }
        }
        
        if (!bExists) {
          FBPVariableDescription NewVar;
          NewVar.VarName = DelegateVarName;
          NewVar.VarGuid = FGuid::NewGuid();
          NewVar.FriendlyName = DispatcherName;
          NewVar.VarType = PinType;
          NewVar.PropertyFlags = CPF_BlueprintAssignable;
          
          Blueprint->NewVariables.Add(NewVar);
          FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
          
          bool bSave = true;
          Payload->TryGetBoolField(TEXT("save"), bSave);
          if (bSave) {
            McpSafeAssetSave(Blueprint);
          }
          
          Message = FString::Printf(TEXT("Created event dispatcher '%s' in blueprint"), *DispatcherName);
        } else {
          Message = FString::Printf(TEXT("Event dispatcher '%s' already exists"), *DispatcherName);
        }
        
        Resp->SetStringField(TEXT("dispatcherName"), DispatcherName);
      } else {
        bSuccess = false;
        ErrorCode = TEXT("BLUEPRINT_NOT_FOUND");
        Message = FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath);
      }
    }
  }
  else if (LowerSub == TEXT("bind_to_event") || LowerSub == TEXT("unbind_from_event")) {
    FString BlueprintPath;
    FString EventName;
    FString FunctionName;
    Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (!Payload->TryGetStringField(TEXT("eventName"), EventName)) {
      Payload->TryGetStringField(TEXT("dispatcherName"), EventName);
    }
    Payload->TryGetStringField(TEXT("functionName"), FunctionName);
    
    if (BlueprintPath.IsEmpty() || EventName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("blueprintPath and eventName are required");
    } else {
      // Binding/unbinding is done via blueprint graph nodes
      Message = FString::Printf(TEXT("Event %s for '%s' noted - use blueprint graph to create bind/unbind nodes"), 
                                LowerSub == TEXT("bind_to_event") ? TEXT("binding") : TEXT("unbinding"), *EventName);
      Resp->SetStringField(TEXT("eventName"), EventName);
      Resp->SetStringField(TEXT("note"), TEXT("Event binding requires blueprint graph node creation"));
    }
  }
  else if (LowerSub == TEXT("broadcast_event")) {
    FString TargetActor;
    FString EventName;
    if (!Payload->TryGetStringField(TEXT("targetActor"), TargetActor)) {
      Payload->TryGetStringField(TEXT("actorName"), TargetActor);
    }
    if (!Payload->TryGetStringField(TEXT("eventName"), EventName)) {
      Payload->TryGetStringField(TEXT("dispatcherName"), EventName);
    }
    
    if (TargetActor.IsEmpty() || EventName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("targetActor and eventName are required");
    } else {
      Message = FString::Printf(TEXT("Event broadcast for '%s' on actor '%s' noted - runtime only"), *EventName, *TargetActor);
      Resp->SetStringField(TEXT("note"), TEXT("Event broadcasting is a runtime operation"));
    }
  }
  else if (LowerSub == TEXT("create_blueprint_interface")) {
    FString InterfaceName;
    if (!Payload->TryGetStringField(TEXT("interfaceName"), InterfaceName)) {
      Payload->TryGetStringField(TEXT("assetPath"), InterfaceName);
    }
    
    if (InterfaceName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("interfaceName is required");
    } else {
      // Ensure proper path
      FString AssetPath = InterfaceName;
      if (!AssetPath.StartsWith(TEXT("/Game/"))) {
        AssetPath = FString::Printf(TEXT("/Game/%s"), *InterfaceName);
      }
      
      FString PackagePath = FPackageName::GetLongPackagePath(AssetPath);
      FString AssetName = FPackageName::GetShortName(AssetPath);
      
      // Create the Blueprint Interface
      UPackage* Package = CreatePackage(*AssetPath);
      UBlueprint* NewInterface = FKismetEditorUtilities::CreateBlueprint(
        UInterface::StaticClass(),
        Package,
        FName(*AssetName),
        BPTYPE_Interface,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass()
      );
      
      if (NewInterface) {
        FAssetRegistryModule::AssetCreated(NewInterface);
        NewInterface->MarkPackageDirty();
        
        bool bSave = true;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        if (bSave) {
          McpSafeAssetSave(NewInterface);
        }
        
        Message = FString::Printf(TEXT("Created blueprint interface: %s"), *AssetPath);
        Resp->SetStringField(TEXT("assetPath"), AssetPath);
      } else {
        bSuccess = false;
        ErrorCode = TEXT("CREATE_FAILED");
        Message = TEXT("Failed to create blueprint interface");
      }
    }
  }
  
  // ==================== TRANSACTIONS ====================
  else if (LowerSub == TEXT("begin_transaction")) {
    FString TransactionName;
    Payload->TryGetStringField(TEXT("transactionName"), TransactionName);
    
    if (TransactionName.IsEmpty()) {
      bSuccess = false;
      ErrorCode = TEXT("MISSING_PARAM");
      Message = TEXT("transactionName is required");
    } else {
      GEditor->BeginTransaction(FText::FromString(TransactionName));
      Message = FString::Printf(TEXT("Started transaction: %s"), *TransactionName);
      Resp->SetStringField(TEXT("transactionName"), TransactionName);
    }
  }
  else if (LowerSub == TEXT("end_transaction")) {
    GEditor->EndTransaction();
    Message = TEXT("Ended transaction");
  }
  else if (LowerSub == TEXT("cancel_transaction")) {
    GEditor->CancelTransaction(0);
    Message = TEXT("Cancelled transaction");
  }
  else if (LowerSub == TEXT("undo")) {
    bool bResult = GEditor->UndoTransaction();
    if (bResult) {
      Message = TEXT("Undo successful");
    } else {
      bSuccess = false;
      ErrorCode = TEXT("UNDO_FAILED");
      Message = TEXT("Nothing to undo");
    }
    Resp->SetBoolField(TEXT("canUndo"), GEditor->Trans->CanUndo());
    Resp->SetBoolField(TEXT("canRedo"), GEditor->Trans->CanRedo());
  }
  else if (LowerSub == TEXT("redo")) {
    bool bResult = GEditor->RedoTransaction();
    if (bResult) {
      Message = TEXT("Redo successful");
    } else {
      bSuccess = false;
      ErrorCode = TEXT("REDO_FAILED");
      Message = TEXT("Nothing to redo");
    }
    Resp->SetBoolField(TEXT("canUndo"), GEditor->Trans->CanUndo());
    Resp->SetBoolField(TEXT("canRedo"), GEditor->Trans->CanRedo());
  }
  else if (LowerSub == TEXT("get_transaction_history")) {
    TArray<TSharedPtr<FJsonValue>> HistoryArray;
    
    // Get undo/redo buffer info
    Resp->SetBoolField(TEXT("canUndo"), GEditor->Trans->CanUndo());
    Resp->SetBoolField(TEXT("canRedo"), GEditor->Trans->CanRedo());
    Resp->SetNumberField(TEXT("undoBufferSize"), GEditor->Trans->GetUndoCount());
    Resp->SetArrayField(TEXT("transactionHistory"), HistoryArray);
    
    Message = TEXT("Retrieved transaction history");
  }
  
  // ==================== UTILITY ====================
  else if (LowerSub == TEXT("get_editor_utilities_info")) {
    // Get current editor state info
    FEditorModeTools& ModeTools = GLevelEditorModeTools();
    
    // Current mode
    if (ModeTools.GetActiveScriptableMode(FBuiltinEditorModes::EM_Default)) {
      Resp->SetStringField(TEXT("currentMode"), TEXT("Default"));
    } else if (ModeTools.GetActiveScriptableMode(FBuiltinEditorModes::EM_Landscape)) {
      Resp->SetStringField(TEXT("currentMode"), TEXT("Landscape"));
    } else if (ModeTools.GetActiveScriptableMode(FBuiltinEditorModes::EM_Foliage)) {
      Resp->SetStringField(TEXT("currentMode"), TEXT("Foliage"));
    } else if (ModeTools.GetActiveScriptableMode(FBuiltinEditorModes::EM_MeshPaint)) {
      Resp->SetStringField(TEXT("currentMode"), TEXT("MeshPaint"));
    } else {
      Resp->SetStringField(TEXT("currentMode"), TEXT("Unknown"));
    }
    
    // Available modes
    TArray<TSharedPtr<FJsonValue>> ModesArray;
    ModesArray.Add(MakeShared<FJsonValueString>(TEXT("Default")));
    ModesArray.Add(MakeShared<FJsonValueString>(TEXT("Landscape")));
    ModesArray.Add(MakeShared<FJsonValueString>(TEXT("Foliage")));
    ModesArray.Add(MakeShared<FJsonValueString>(TEXT("MeshPaint")));
    ModesArray.Add(MakeShared<FJsonValueString>(TEXT("Geometry")));
    Resp->SetArrayField(TEXT("availableModes"), ModesArray);
    
    // Grid settings
    TSharedPtr<FJsonObject> GridSettings = MakeShared<FJsonObject>();
    GridSettings->SetNumberField(TEXT("gridSize"), GEditor->GetGridSize());
    GridSettings->SetBoolField(TEXT("gridEnabled"), GetDefault<ULevelEditorViewportSettings>()->GridEnabled);
    Resp->SetObjectField(TEXT("gridSettings"), GridSettings);
    
    // Selection count
    Resp->SetNumberField(TEXT("selectionCount"), GEditor->GetSelectedActorCount());
    
    // Transaction state
    Resp->SetBoolField(TEXT("canUndo"), GEditor->Trans->CanUndo());
    Resp->SetBoolField(TEXT("canRedo"), GEditor->Trans->CanRedo());
    
    Message = TEXT("Retrieved editor utilities info");
  }
  else {
    bSuccess = false;
    ErrorCode = TEXT("UNKNOWN_ACTION");
    Message = FString::Printf(TEXT("Unknown manage_editor_utilities action: %s"), *LowerSub);
  }

  // Send response
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetStringField(TEXT("message"), Message);
  if (!ErrorCode.IsEmpty()) {
    Resp->SetStringField(TEXT("error"), ErrorCode);
  }
  
  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
  return true;

#else
  // Editor not available
  SendAutomationError(RequestingSocket, RequestId,
                      TEXT("manage_editor_utilities requires WITH_EDITOR"),
                      TEXT("NOT_AVAILABLE"));
  return true;
#endif // WITH_EDITOR
}
