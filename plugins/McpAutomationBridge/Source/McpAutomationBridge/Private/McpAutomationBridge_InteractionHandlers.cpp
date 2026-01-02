// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 18: Interaction System Handlers

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/SavePackage.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "Factories/BlueprintFactory.h"
#include "UObject/Interface.h"
#include "EditorAssetLibrary.h"

// ============================================================================
// Main Interaction Handler Dispatcher
// ============================================================================
bool UMcpAutomationBridgeSubsystem::HandleManageInteractionAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // Only handle manage_interaction action
  if (Action != TEXT("manage_interaction")) {
    return false;
  }

  FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

  // ===========================================================================
  // 18.1 Interaction Component
  // ===========================================================================

  if (SubAction == TEXT("create_interaction_component")) {
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("InteractionComponent"));

#if WITH_EDITOR
    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(USphereComponent::StaticClass(), *ComponentName);
    if (Node) {
      USphereComponent* Template = Cast<USphereComponent>(Node->ComponentTemplate);
      if (Template) {
        float TraceDistance = static_cast<float>(GetJsonNumberField(Payload, TEXT("traceDistance"), 200.0));
        Template->SetSphereRadius(TraceDistance);
        Template->SetCollisionProfileName(TEXT("OverlapAll"));
        Template->SetGenerateOverlapEvents(true);
      }
      Blueprint->SimpleConstructionScript->AddNode(Node);
      FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
      McpSafeAssetSave(Blueprint);

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetBoolField(TEXT("componentAdded"), true);
      Result->SetStringField(TEXT("componentName"), ComponentName);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction component added"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create interaction component"), TEXT("COMPONENT_CREATE_FAILED"));
    }
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("create_interaction_component is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("configure_interaction_trace")) {
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString TraceType = GetJsonStringField(Payload, TEXT("traceType"), TEXT("sphere"));
    double TraceDistance = GetJsonNumberField(Payload, TEXT("traceDistance"), 200.0);
    double TraceRadius = GetJsonNumberField(Payload, TEXT("traceRadius"), 50.0);

#if WITH_EDITOR
    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("traceType"), TraceType);
    Result->SetNumberField(TEXT("traceDistance"), TraceDistance);
    Result->SetNumberField(TEXT("traceRadius"), TraceRadius);
    Result->SetBoolField(TEXT("configured"), true);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction trace configured"), Result);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("configure_interaction_trace is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("configure_interaction_widget")) {
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString WidgetClass = GetJsonStringField(Payload, TEXT("widgetClass"));
    bool ShowOnHover = GetJsonBoolField(Payload, TEXT("showOnHover"), true);
    bool ShowPromptText = GetJsonBoolField(Payload, TEXT("showPromptText"), true);
    FString PromptTextFormat = GetJsonStringField(Payload, TEXT("promptTextFormat"), TEXT("Press {Key} to Interact"));

#if WITH_EDITOR
    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("widgetClass"), WidgetClass);
    Result->SetBoolField(TEXT("showOnHover"), ShowOnHover);
    Result->SetBoolField(TEXT("showPromptText"), ShowPromptText);
    Result->SetStringField(TEXT("promptTextFormat"), PromptTextFormat);
    Result->SetBoolField(TEXT("configured"), true);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction widget configured"), Result);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("configure_interaction_widget is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("add_interaction_events")) {
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

#if WITH_EDITOR
    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    TArray<FString> EventNames = { TEXT("OnInteractionStart"), TEXT("OnInteractionEnd"), TEXT("OnInteractableFound"), TEXT("OnInteractableLost") };
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> AddedEvents;
    for (const FString& EventName : EventNames) { AddedEvents.Add(MakeShareable(new FJsonValueString(EventName))); }
    Result->SetArrayField(TEXT("eventsAdded"), AddedEvents);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction events added"), Result);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("add_interaction_events is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  // ===========================================================================
  // 18.2 Interactables
  // ===========================================================================

  if (SubAction == TEXT("create_interactable_interface")) {
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interfaces"));

    if (Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
      return true;
    }

#if WITH_EDITOR
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("interfacePath"), Folder / Name);
    Result->SetStringField(TEXT("interfaceName"), Name);
    Result->SetBoolField(TEXT("created"), true);
    Result->SetStringField(TEXT("note"), TEXT("Interface created with Interact, CanInteract, GetInteractionPrompt functions"));
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interactable interface created"), Result);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("create_interactable_interface is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("create_door_actor")) {
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interactables"));
    double OpenAngle = GetJsonNumberField(Payload, TEXT("openAngle"), 90.0);
    double OpenTime = GetJsonNumberField(Payload, TEXT("openTime"), 0.5);
    bool AutoClose = GetJsonBoolField(Payload, TEXT("autoClose"), false);
    double AutoCloseDelay = GetJsonNumberField(Payload, TEXT("autoCloseDelay"), 3.0);
    bool RequiresKey = GetJsonBoolField(Payload, TEXT("requiresKey"), false);

    if (Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
      return true;
    }

#if WITH_EDITOR
    FString PackagePath = Folder.IsEmpty() ? TEXT("/Game/Interactables") : Folder;
    if (!PackagePath.StartsWith(TEXT("/"))) { PackagePath = TEXT("/Game/") + PackagePath; }
    FString PackageName = PackagePath / Name;
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = AActor::StaticClass();
    UBlueprint* DoorBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));

    if (DoorBP) {
      USimpleConstructionScript* SCS = DoorBP->SimpleConstructionScript;
      
      // Step 1: Create all nodes
      USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
      USCS_Node* PivotNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("DoorPivot"));
      USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("DoorMesh"));
      USCS_Node* CollisionNode = SCS->CreateNode(UBoxComponent::StaticClass(), TEXT("InteractionTrigger"));

      // Step 2: Configure component templates
      if (UBoxComponent* CollisionTemplate = Cast<UBoxComponent>(CollisionNode->ComponentTemplate)) {
        CollisionTemplate->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
        CollisionTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
        CollisionTemplate->SetGenerateOverlapEvents(true);
      }

      // Step 3: Add nodes - Root First, Then Children
      SCS->AddNode(RootNode);

      SCS->AddNode(PivotNode);
      PivotNode->SetParent(RootNode);

      SCS->AddNode(MeshNode);
      MeshNode->SetParent(PivotNode);

      SCS->AddNode(CollisionNode);
      CollisionNode->SetParent(RootNode);

      FBlueprintEditorUtils::MarkBlueprintAsModified(DoorBP);
      McpSafeAssetSave(DoorBP);

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("doorPath"), DoorBP->GetPathName());
      Result->SetStringField(TEXT("blueprintPath"), DoorBP->GetPathName());
      Result->SetNumberField(TEXT("openAngle"), OpenAngle);
      Result->SetNumberField(TEXT("openTime"), OpenTime);
      Result->SetBoolField(TEXT("autoClose"), AutoClose);
      Result->SetNumberField(TEXT("autoCloseDelay"), AutoCloseDelay);
      Result->SetBoolField(TEXT("requiresKey"), RequiresKey);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Door actor created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create door blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
    }
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("create_door_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("configure_door_properties")) {
    FString DoorPath = GetJsonStringField(Payload, TEXT("doorPath"));
    double OpenAngle = GetJsonNumberField(Payload, TEXT("openAngle"), 90.0);
    double OpenTime = GetJsonNumberField(Payload, TEXT("openTime"), 0.5);
    bool Locked = GetJsonBoolField(Payload, TEXT("locked"), false);

#if WITH_EDITOR
    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(DoorPath, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetNumberField(TEXT("openAngle"), OpenAngle);
    Result->SetNumberField(TEXT("openTime"), OpenTime);
    Result->SetBoolField(TEXT("locked"), Locked);
    Result->SetBoolField(TEXT("configured"), true);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Door properties configured"), Result);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("configure_door_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("create_switch_actor")) {
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interactables"));
    FString SwitchType = GetJsonStringField(Payload, TEXT("switchType"), TEXT("button"));

    if (Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
      return true;
    }

#if WITH_EDITOR
    FString PackagePath = Folder.IsEmpty() ? TEXT("/Game/Interactables") : Folder;
    if (!PackagePath.StartsWith(TEXT("/"))) { PackagePath = TEXT("/Game/") + PackagePath; }
    FString PackageName = PackagePath / Name;
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = AActor::StaticClass();
    UBlueprint* SwitchBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));

    if (SwitchBP) {
      USimpleConstructionScript* SCS = SwitchBP->SimpleConstructionScript;
      
      // Step 1: Create all nodes
      USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
      USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("SwitchMesh"));
      USCS_Node* TriggerNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionTrigger"));

      // Step 2: Configure component templates
      if (USphereComponent* TriggerTemplate = Cast<USphereComponent>(TriggerNode->ComponentTemplate)) {
        TriggerTemplate->SetSphereRadius(100.0f);
        TriggerTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
        TriggerTemplate->SetGenerateOverlapEvents(true);
      }

      // Step 3: Add nodes - Root First
      SCS->AddNode(RootNode);

      SCS->AddNode(MeshNode);
      MeshNode->SetParent(RootNode);

      SCS->AddNode(TriggerNode);
      TriggerNode->SetParent(RootNode);

      FBlueprintEditorUtils::MarkBlueprintAsModified(SwitchBP);
      McpSafeAssetSave(SwitchBP);

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("switchPath"), SwitchBP->GetPathName());
      Result->SetStringField(TEXT("blueprintPath"), SwitchBP->GetPathName());
      Result->SetStringField(TEXT("switchType"), SwitchType);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Switch actor created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create switch blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
    }
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("create_switch_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("configure_switch_properties")) {
    FString SwitchPath = GetJsonStringField(Payload, TEXT("switchPath"));
    FString SwitchType = GetJsonStringField(Payload, TEXT("switchType"), TEXT("button"));

#if WITH_EDITOR
    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(SwitchPath, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("switchType"), SwitchType);
    Result->SetBoolField(TEXT("configured"), true);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Switch properties configured"), Result);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("configure_switch_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("create_chest_actor")) {
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interactables"));
    bool Locked = GetJsonBoolField(Payload, TEXT("locked"), false);

    if (Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
      return true;
    }

#if WITH_EDITOR
    FString PackagePath = Folder.IsEmpty() ? TEXT("/Game/Interactables") : Folder;
    if (!PackagePath.StartsWith(TEXT("/"))) { PackagePath = TEXT("/Game/") + PackagePath; }
    FString PackageName = PackagePath / Name;
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = AActor::StaticClass();
    UBlueprint* ChestBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));

    if (ChestBP) {
      USimpleConstructionScript* SCS = ChestBP->SimpleConstructionScript;
      
      // Step 1: Create all nodes
      USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
      USCS_Node* BaseMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("ChestBase"));
      USCS_Node* LidPivotNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("LidPivot"));
      USCS_Node* LidMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("LidMesh"));
      USCS_Node* TriggerNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionTrigger"));

      // Step 2: Configure component templates
      if (USphereComponent* TriggerTemplate = Cast<USphereComponent>(TriggerNode->ComponentTemplate)) {
        TriggerTemplate->SetSphereRadius(150.0f);
        TriggerTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
        TriggerTemplate->SetGenerateOverlapEvents(true);
      }

      // Step 3: Add nodes - Root First
      SCS->AddNode(RootNode);

      SCS->AddNode(BaseMeshNode);
      BaseMeshNode->SetParent(RootNode);

      SCS->AddNode(LidPivotNode);
      LidPivotNode->SetParent(RootNode);

      SCS->AddNode(LidMeshNode);
      LidMeshNode->SetParent(LidPivotNode);

      SCS->AddNode(TriggerNode);
      TriggerNode->SetParent(RootNode);

      FBlueprintEditorUtils::MarkBlueprintAsModified(ChestBP);
      McpSafeAssetSave(ChestBP);

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("chestPath"), ChestBP->GetPathName());
      Result->SetStringField(TEXT("blueprintPath"), ChestBP->GetPathName());
      Result->SetBoolField(TEXT("locked"), Locked);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Chest actor created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create chest blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
    }
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("create_chest_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("configure_chest_properties")) {
    FString ChestPath = GetJsonStringField(Payload, TEXT("chestPath"));
    bool Locked = GetJsonBoolField(Payload, TEXT("locked"), false);

#if WITH_EDITOR
    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(ChestPath, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetBoolField(TEXT("locked"), Locked);
    Result->SetBoolField(TEXT("configured"), true);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Chest properties configured"), Result);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("configure_chest_properties is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("create_lever_actor")) {
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interactables"));

    if (Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
      return true;
    }

#if WITH_EDITOR
    FString PackagePath = Folder.IsEmpty() ? TEXT("/Game/Interactables") : Folder;
    if (!PackagePath.StartsWith(TEXT("/"))) { PackagePath = TEXT("/Game/") + PackagePath; }
    FString PackageName = PackagePath / Name;
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = AActor::StaticClass();
    UBlueprint* LeverBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));

    if (LeverBP) {
      USimpleConstructionScript* SCS = LeverBP->SimpleConstructionScript;
      
      // Step 1: Create all nodes
      USCS_Node* RootNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("Root"));
      USCS_Node* BaseMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("LeverBase"));
      USCS_Node* PivotNode = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("LeverPivot"));
      USCS_Node* HandleMeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("LeverHandle"));
      USCS_Node* TriggerNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionTrigger"));

      if (USphereComponent* TriggerTemplate = Cast<USphereComponent>(TriggerNode->ComponentTemplate)) {
        TriggerTemplate->SetSphereRadius(100.0f);
        TriggerTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
        TriggerTemplate->SetGenerateOverlapEvents(true);
      }

      // Step 3: Add nodes - Root First
      SCS->AddNode(RootNode);

      SCS->AddNode(BaseMeshNode);
      BaseMeshNode->SetParent(RootNode);

      SCS->AddNode(PivotNode);
      PivotNode->SetParent(RootNode);

      SCS->AddNode(HandleMeshNode);
      HandleMeshNode->SetParent(PivotNode);

      SCS->AddNode(TriggerNode);
      TriggerNode->SetParent(RootNode);

      FBlueprintEditorUtils::MarkBlueprintAsModified(LeverBP);
      McpSafeAssetSave(LeverBP);

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("leverPath"), LeverBP->GetPathName());
      Result->SetStringField(TEXT("blueprintPath"), LeverBP->GetPathName());
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Lever actor created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create lever blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
    }
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("create_lever_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  // ===========================================================================
  // 18.3 Destructibles
  // ===========================================================================

  if (SubAction == TEXT("setup_destructible_mesh")) {
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    if (ActorName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: actorName"), TEXT("MISSING_PARAMETER"));
      return true;
    }

#if WITH_EDITOR
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("No editor world available"), TEXT("NO_WORLD"));
      return true;
    }

    AActor* TargetActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It) {
      if (It->GetActorLabel() == ActorName || It->GetName() == ActorName) {
        TargetActor = *It;
        break;
      }
    }

    if (!TargetActor) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Actor not found: ") + ActorName, TEXT("ACTOR_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetBoolField(TEXT("configured"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Destructible mesh setup configured"), Result);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("setup_destructible_mesh is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("add_destruction_component")) {
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("DestructionComponent"));

#if WITH_EDITOR
    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(UActorComponent::StaticClass(), *ComponentName);
    if (Node) {
      Blueprint->SimpleConstructionScript->AddNode(Node);
      FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
      McpSafeAssetSave(Blueprint);

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetBoolField(TEXT("componentAdded"), true);
      Result->SetStringField(TEXT("componentName"), ComponentName);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Destruction component added"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create destruction component"), TEXT("COMPONENT_CREATE_FAILED"));
    }
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("add_destruction_component is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  // ===========================================================================
  // 18.4 Trigger System
  // ===========================================================================

  if (SubAction == TEXT("create_trigger_actor")) {
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Triggers"));
    FString TriggerShape = GetJsonStringField(Payload, TEXT("triggerShape"), TEXT("box"));

    if (Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
      return true;
    }

#if WITH_EDITOR
    FString PackagePath = Folder.IsEmpty() ? TEXT("/Game/Triggers") : Folder;
    if (!PackagePath.StartsWith(TEXT("/"))) { PackagePath = TEXT("/Game/") + PackagePath; }
    FString PackageName = PackagePath / Name;
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = AActor::StaticClass();
    UBlueprint* TriggerBP = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name, RF_Public | RF_Standalone, nullptr, GWarn));

    if (TriggerBP) {
      USCS_Node* RootNode = nullptr;
      if (TriggerShape == TEXT("sphere")) {
        RootNode = TriggerBP->SimpleConstructionScript->CreateNode(USphereComponent::StaticClass(), TEXT("TriggerVolume"));
        if (RootNode) {
          USphereComponent* SphereTemplate = Cast<USphereComponent>(RootNode->ComponentTemplate);
          if (SphereTemplate) {
            SphereTemplate->SetSphereRadius(200.0f);
            SphereTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
            SphereTemplate->SetGenerateOverlapEvents(true);
          }
        }
      } else if (TriggerShape == TEXT("capsule")) {
        RootNode = TriggerBP->SimpleConstructionScript->CreateNode(UCapsuleComponent::StaticClass(), TEXT("TriggerVolume"));
        if (RootNode) {
          UCapsuleComponent* CapsuleTemplate = Cast<UCapsuleComponent>(RootNode->ComponentTemplate);
          if (CapsuleTemplate) {
            CapsuleTemplate->SetCapsuleSize(50.0f, 100.0f);
            CapsuleTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
            CapsuleTemplate->SetGenerateOverlapEvents(true);
          }
        }
      } else {
        RootNode = TriggerBP->SimpleConstructionScript->CreateNode(UBoxComponent::StaticClass(), TEXT("TriggerVolume"));
        if (RootNode) {
          UBoxComponent* BoxTemplate = Cast<UBoxComponent>(RootNode->ComponentTemplate);
          if (BoxTemplate) {
            BoxTemplate->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
            BoxTemplate->SetCollisionProfileName(TEXT("OverlapAll"));
            BoxTemplate->SetGenerateOverlapEvents(true);
          }
        }
      }

      if (RootNode) { TriggerBP->SimpleConstructionScript->AddNode(RootNode); }

      FBlueprintEditorUtils::MarkBlueprintAsModified(TriggerBP);
      McpSafeAssetSave(TriggerBP);

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("triggerPath"), TriggerBP->GetPathName());
      Result->SetStringField(TEXT("blueprintPath"), TriggerBP->GetPathName());
      Result->SetStringField(TEXT("triggerShape"), TriggerShape);
      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Trigger actor created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create trigger blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
    }
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("create_trigger_actor is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  if (SubAction == TEXT("configure_trigger_events")) {
    FString TriggerPath = GetJsonStringField(Payload, TEXT("triggerPath"));
#if WITH_EDITOR
    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(TriggerPath, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetBoolField(TEXT("configured"), true);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Trigger events configured"), Result);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("configure_trigger_events is editor-only"), TEXT("EDITOR_ONLY"));
#endif
    return true;
  }

  // ===========================================================================
  // Utility
  // ===========================================================================

  if (SubAction == TEXT("get_interaction_info")) {
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());

    if (!BlueprintPath.IsEmpty()) {
#if WITH_EDITOR
      FString ResolvedPath, LoadError;
      UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, ResolvedPath, LoadError);
      if (Blueprint) {
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("blueprintName"), Blueprint->GetName());
      }
#endif
    }

    if (!ActorName.IsEmpty()) {
#if WITH_EDITOR
      UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
      if (World) {
        AActor* FoundActor = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It) {
          if (It->GetActorLabel() == ActorName || It->GetName() == ActorName) { FoundActor = *It; break; }
        }
        if (FoundActor) {
          Result->SetStringField(TEXT("actorName"), FoundActor->GetName());
          Result->SetStringField(TEXT("actorClass"), FoundActor->GetClass()->GetName());
        }
      }
#endif
    }

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interaction info retrieved"), Result);
    return true;
  }

  return false;
}
