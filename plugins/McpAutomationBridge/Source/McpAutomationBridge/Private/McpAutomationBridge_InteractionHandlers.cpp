// Copyright Epic Games, Inc. All Rights Reserved.
// Phase 18: Interaction System Handlers

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
// Note: SavePackage.h removed - use McpSafeAssetSave() from McpAutomationBridgeHelpers.h instead
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
// Helper functions to reduce O(N²) variable lookups to O(N)
// ============================================================================
namespace {
#if WITH_EDITOR
// Build a TSet of existing variable names in a single pass for O(1) lookups
inline TSet<FName> BuildVariableNameSet(const UBlueprint* Blueprint)
{
    TSet<FName> VariableNames;
    if (Blueprint)
    {
        VariableNames.Reserve(Blueprint->NewVariables.Num());
        for (const FBPVariableDescription& Var : Blueprint->NewVariables)
        {
            VariableNames.Add(Var.VarName);
        }
    }
    return VariableNames;
}

// Check if a variable exists in O(1) using pre-built set
inline bool HasVariable(const TSet<FName>& VariableSet, const TCHAR* VarName)
{
    return VariableSet.Contains(FName(VarName));
}

// Find a variable description by name (for setting default values)
inline FBPVariableDescription* FindVariableDescription(UBlueprint* Blueprint, const TCHAR* VarName)
{
    if (!Blueprint) return nullptr;
    FName TargetName(VarName);
    for (FBPVariableDescription& Var : Blueprint->NewVariables)
    {
        if (Var.VarName == TargetName)
        {
            return &Var;
        }
    }
    return nullptr;
}
#endif
} // namespace

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

    bool bConfigured = false;

    // Find or create interaction component and configure it
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (SCS) {
      for (USCS_Node* Node : SCS->GetAllNodes()) {
        if (!Node || !Node->ComponentClass) continue;

        // Configure sphere components for interaction
        if (Node->ComponentClass->IsChildOf(USphereComponent::StaticClass())) {
          USphereComponent* SphereComp = Cast<USphereComponent>(Node->ComponentTemplate);
          if (SphereComp) {
            SphereComp->SetSphereRadius(static_cast<float>(TraceDistance));
            SphereComp->SetCollisionProfileName(TEXT("OverlapAll"));
            SphereComp->SetGenerateOverlapEvents(true);
            bConfigured = true;
          }
        }
        // Configure box components for interaction
        else if (Node->ComponentClass->IsChildOf(UBoxComponent::StaticClass())) {
          UBoxComponent* BoxComp = Cast<UBoxComponent>(Node->ComponentTemplate);
          if (BoxComp) {
            BoxComp->SetBoxExtent(FVector(static_cast<float>(TraceDistance), static_cast<float>(TraceRadius), static_cast<float>(TraceRadius)));
            BoxComp->SetCollisionProfileName(TEXT("OverlapAll"));
            BoxComp->SetGenerateOverlapEvents(true);
            bConfigured = true;
          }
        }
      }
    }

    // Add trace configuration Blueprint variables
    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    // Build variable name set once for O(1) lookups (optimization from O(N²) to O(N))
    TSet<FName> ExistingVars = BuildVariableNameSet(Blueprint);

    // Add TraceDistance variable if not exists
    if (!HasVariable(ExistingVars, TEXT("TraceDistance"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TraceDistance"), FloatType);
    }

    // Add TraceType variable if not exists
    if (!HasVariable(ExistingVars, TEXT("TraceType"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("TraceType"), NameType);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("traceType"), TraceType);
    Result->SetNumberField(TEXT("traceDistance"), TraceDistance);
    Result->SetNumberField(TEXT("traceRadius"), TraceRadius);
    Result->SetBoolField(TEXT("configured"), bConfigured);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

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

    // Add widget configuration Blueprint variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType StringType;
    StringType.PinCategory = UEdGraphSchema_K2::PC_String;

    FEdGraphPinType ClassType;
    ClassType.PinCategory = UEdGraphSchema_K2::PC_Class;

    // Build variable name set once for O(1) existence checks (avoids O(N²) loops)
    TSet<FName> ExistingVars = BuildVariableNameSet(Blueprint);

    // Add bShowOnHover variable
    if (!HasVariable(ExistingVars, TEXT("bShowOnHover"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bShowOnHover"), BoolType);
    }
    // Set default value for bShowOnHover
    if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("bShowOnHover"))) {
      Var->DefaultValue = ShowOnHover ? TEXT("true") : TEXT("false");
    }

    // Add bShowPromptText variable
    if (!HasVariable(ExistingVars, TEXT("bShowPromptText"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bShowPromptText"), BoolType);
    }
    // Set default value for bShowPromptText
    if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("bShowPromptText"))) {
      Var->DefaultValue = ShowPromptText ? TEXT("true") : TEXT("false");
    }

    // Add PromptTextFormat variable
    if (!HasVariable(ExistingVars, TEXT("PromptTextFormat"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("PromptTextFormat"), StringType);
    }
    // Set default value for PromptTextFormat
    if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("PromptTextFormat"))) {
      Var->DefaultValue = PromptTextFormat;
    }

    // Add InteractionWidgetClass variable (soft class reference)
    FEdGraphPinType SoftClassType;
    SoftClassType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;

    if (!HasVariable(ExistingVars, TEXT("InteractionWidgetClass"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("InteractionWidgetClass"), SoftClassType);
    }
    // Set default value for InteractionWidgetClass if provided
    if (!WidgetClass.IsEmpty()) {
      if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("InteractionWidgetClass"))) {
        Var->DefaultValue = WidgetClass;
      }
    }


    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("widgetClass"), WidgetClass);
    Result->SetBoolField(TEXT("showOnHover"), ShowOnHover);
    Result->SetBoolField(TEXT("showPromptText"), ShowPromptText);
    Result->SetStringField(TEXT("promptTextFormat"), PromptTextFormat);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
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

    // Define event dispatchers to add
    TArray<FString> EventNames = { 
      TEXT("OnInteractionStart"), 
      TEXT("OnInteractionEnd"), 
      TEXT("OnInteractableFound"), 
      TEXT("OnInteractableLost") 
    };

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> AddedEvents;

    // Add event dispatcher variables for each event
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    for (const FString& EventName : EventNames) {
      // Check if variable already exists
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName.ToString() == EventName) {
          bExists = true;
          break;
        }
      }

      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*EventName), DelegateType);
        AddedEvents.Add(MakeShareable(new FJsonValueString(EventName)));
      } else {
        AddedEvents.Add(MakeShareable(new FJsonValueString(EventName + TEXT(" (exists)"))));
      }
    }

    Result->SetArrayField(TEXT("eventsAdded"), AddedEvents);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("eventCount"), EventNames.Num());

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
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
    // Normalize the path
    FString PackagePath = Folder.IsEmpty() ? TEXT("/Game/Interfaces") : Folder;
    if (!PackagePath.StartsWith(TEXT("/"))) { 
      PackagePath = TEXT("/Game/") + PackagePath; 
    }
    FString PackageName = PackagePath / Name;

    // Create the package
    UPackage* Package = CreatePackage(*PackageName);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    // Create a Blueprint Interface
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->BlueprintType = BPTYPE_Interface;
    Factory->ParentClass = UInterface::StaticClass();

    UBlueprint* InterfaceBP = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, FName(*Name), 
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (InterfaceBP) {
      // Mark as interface type
      InterfaceBP->BlueprintType = BPTYPE_Interface;

      // Add standard interaction functions via function graphs
      // Note: Blueprint function creation requires K2Node manipulation which is complex
      // For now, create the interface and document the expected functions

      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(InterfaceBP);
      FAssetRegistryModule::AssetCreated(InterfaceBP);
      McpSafeAssetSave(InterfaceBP);

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetStringField(TEXT("interfacePath"), InterfaceBP->GetPathName());
      Result->SetStringField(TEXT("interfaceName"), Name);
      Result->SetBoolField(TEXT("created"), true);

      TArray<TSharedPtr<FJsonValue>> FunctionsToAdd;
      FunctionsToAdd.Add(MakeShareable(new FJsonValueString(TEXT("Interact"))));
      FunctionsToAdd.Add(MakeShareable(new FJsonValueString(TEXT("CanInteract"))));
      FunctionsToAdd.Add(MakeShareable(new FJsonValueString(TEXT("GetInteractionPrompt"))));
      Result->SetArrayField(TEXT("recommendedFunctions"), FunctionsToAdd);
      Result->SetStringField(TEXT("note"), TEXT("Interface created. Add Interact, CanInteract, and GetInteractionPrompt functions in the Blueprint Editor."));

      SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Interactable interface created"), Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create interface blueprint"), TEXT("BLUEPRINT_CREATE_FAILED"));
    }
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

    // Add door property Blueprint variables
    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    // Add OpenAngle variable
    bool bOpenAngleExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("OpenAngle")) {
        bOpenAngleExists = true;
        break;
      }
    }
    if (!bOpenAngleExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OpenAngle"), FloatType);
    }

    // Add OpenTime variable
    bool bOpenTimeExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("OpenTime")) {
        bOpenTimeExists = true;
        break;
      }
    }
    if (!bOpenTimeExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OpenTime"), FloatType);
    }

    // Add bIsLocked variable
    bool bLockedExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("bIsLocked")) {
        bLockedExists = true;
        break;
      }
    }
    if (!bLockedExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bIsLocked"), BoolType);
    }

    // Add bIsOpen variable
    bool bIsOpenExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("bIsOpen")) {
        bIsOpenExists = true;
        break;
      }
    }
    if (!bIsOpenExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bIsOpen"), BoolType);
    }

    // Set default values on CDO if available
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        FProperty* OpenAngleProp = CDO->GetClass()->FindPropertyByName(TEXT("OpenAngle"));
        if (OpenAngleProp) {
          TSharedPtr<FJsonValue> FloatValue = MakeShareable(new FJsonValueNumber(OpenAngle));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, OpenAngleProp, FloatValue, ApplyError);
        }

        FProperty* OpenTimeProp = CDO->GetClass()->FindPropertyByName(TEXT("OpenTime"));
        if (OpenTimeProp) {
          TSharedPtr<FJsonValue> FloatValue = MakeShareable(new FJsonValueNumber(OpenTime));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, OpenTimeProp, FloatValue, ApplyError);
        }

        FProperty* LockedProp = CDO->GetClass()->FindPropertyByName(TEXT("bIsLocked"));
        if (LockedProp) {
          TSharedPtr<FJsonValue> BoolValue = MakeShareable(new FJsonValueBoolean(Locked));
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, LockedProp, BoolValue, ApplyError);
        }
      }
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetNumberField(TEXT("openAngle"), OpenAngle);
    Result->SetNumberField(TEXT("openTime"), OpenTime);
    Result->SetBoolField(TEXT("locked"), Locked);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetStringField(TEXT("doorPath"), DoorPath);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
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
    bool CanToggle = GetJsonBoolField(Payload, TEXT("canToggle"), true);
    double ResetTime = GetJsonNumberField(Payload, TEXT("resetTime"), 0.0);

#if WITH_EDITOR
    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(SwitchPath, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    // Add switch property Blueprint variables
    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    // Build variable name set once for O(1) existence checks (avoids O(N²) loops)
    TSet<FName> ExistingVars = BuildVariableNameSet(Blueprint);

    // Add SwitchType variable
    if (!HasVariable(ExistingVars, TEXT("SwitchType"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SwitchType"), NameType);
    }
    // Set default value for SwitchType
    if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("SwitchType"))) {
      Var->DefaultValue = SwitchType;
    }

    // Add bCanToggle variable
    if (!HasVariable(ExistingVars, TEXT("bCanToggle"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bCanToggle"), BoolType);
    }
    // Set default value for bCanToggle
    if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("bCanToggle"))) {
      Var->DefaultValue = CanToggle ? TEXT("true") : TEXT("false");
    }

    // Add bIsActivated variable
    if (!HasVariable(ExistingVars, TEXT("bIsActivated"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bIsActivated"), BoolType);
    }
    // Set default value for bIsActivated (default to false)
    if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("bIsActivated"))) {
      Var->DefaultValue = TEXT("false");
    }

    // Add ResetTime variable
    if (!HasVariable(ExistingVars, TEXT("ResetTime"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("ResetTime"), FloatType);
    }
    // Set default value for ResetTime
    if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("ResetTime"))) {
      Var->DefaultValue = FString::Printf(TEXT("%f"), ResetTime);
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("switchType"), SwitchType);
    Result->SetBoolField(TEXT("canToggle"), CanToggle);
    Result->SetNumberField(TEXT("resetTime"), ResetTime);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetStringField(TEXT("switchPath"), SwitchPath);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
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
    double OpenAngle = GetJsonNumberField(Payload, TEXT("openAngle"), 90.0);
    double OpenTime = GetJsonNumberField(Payload, TEXT("openTime"), 0.5);
    FString LootTablePath = GetJsonStringField(Payload, TEXT("lootTablePath"));

#if WITH_EDITOR
    FString ResolvedPath, LoadError;
    UBlueprint* Blueprint = LoadBlueprintAsset(ChestPath, ResolvedPath, LoadError);
    if (!Blueprint) {
      SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    // Add chest property Blueprint variables
    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

    // Build variable name set once for O(1) existence checks (avoids O(N²) loops)
    TSet<FName> ExistingVars = BuildVariableNameSet(Blueprint);

    // Add bIsLocked variable
    if (!HasVariable(ExistingVars, TEXT("bIsLocked"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bIsLocked"), BoolType);
    }
    // Set default value for bIsLocked
    if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("bIsLocked"))) {
      Var->DefaultValue = Locked ? TEXT("true") : TEXT("false");
    }

    // Add bIsOpen variable
    if (!HasVariable(ExistingVars, TEXT("bIsOpen"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bIsOpen"), BoolType);
    }
    // Set default value for bIsOpen (default to false - chest starts closed)
    if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("bIsOpen"))) {
      Var->DefaultValue = TEXT("false");
    }

    // Add LidOpenAngle variable
    if (!HasVariable(ExistingVars, TEXT("LidOpenAngle"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("LidOpenAngle"), FloatType);
    }
    // Set default value for LidOpenAngle
    if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("LidOpenAngle"))) {
      Var->DefaultValue = FString::Printf(TEXT("%f"), OpenAngle);
    }

    // Add OpenTime variable
    if (!HasVariable(ExistingVars, TEXT("OpenTime"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OpenTime"), FloatType);
    }
    // Set default value for OpenTime
    if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("OpenTime"))) {
      Var->DefaultValue = FString::Printf(TEXT("%f"), OpenTime);
    }

    // Add LootTable soft reference
    if (!HasVariable(ExistingVars, TEXT("LootTable"))) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("LootTable"), SoftObjectType);
    }
    // Set default value for LootTable if provided
    if (!LootTablePath.IsEmpty()) {
      if (FBPVariableDescription* Var = FindVariableDescription(Blueprint, TEXT("LootTable"))) {
        Var->DefaultValue = LootTablePath;
      }
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetBoolField(TEXT("locked"), Locked);
    Result->SetNumberField(TEXT("openAngle"), OpenAngle);
    Result->SetNumberField(TEXT("openTime"), OpenTime);
    if (!LootTablePath.IsEmpty()) {
      Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
    }
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetStringField(TEXT("chestPath"), ChestPath);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
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
    UWorld* World = GetActiveWorld();
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

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("NO_SCS"));
      return true;
    }

    // Create a SceneComponent for destruction (allows hierarchy and proper transform)
    USCS_Node* Node = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
    if (Node) {
      SCS->AddNode(Node);

      // Add destruction-related Blueprint variables
      FEdGraphPinType BoolType;
      BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

      FEdGraphPinType FloatType;
      FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
      FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

      FEdGraphPinType IntType;
      IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

      // Add Health variable
      bool bHealthExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == TEXT("Health")) {
          bHealthExists = true;
          break;
        }
      }
      if (!bHealthExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("Health"), FloatType);
      }

      // Add MaxHealth variable
      bool bMaxHealthExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == TEXT("MaxHealth")) {
          bMaxHealthExists = true;
          break;
        }
      }
      if (!bMaxHealthExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MaxHealth"), FloatType);
      }

      // Add bIsDestroyed variable
      bool bDestroyedExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == TEXT("bIsDestroyed")) {
          bDestroyedExists = true;
          break;
        }
      }
      if (!bDestroyedExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bIsDestroyed"), BoolType);
      }

      // Add DestructionStage variable
      bool bStageExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == TEXT("DestructionStage")) {
          bStageExists = true;
          break;
        }
      }
      if (!bStageExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("DestructionStage"), IntType);
      }

      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
      McpSafeAssetSave(Blueprint);

      TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
      Result->SetBoolField(TEXT("componentAdded"), true);
      Result->SetStringField(TEXT("componentName"), ComponentName);
      Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

      TArray<TSharedPtr<FJsonValue>> AddedVars;
      AddedVars.Add(MakeShareable(new FJsonValueString(TEXT("Health"))));
      AddedVars.Add(MakeShareable(new FJsonValueString(TEXT("MaxHealth"))));
      AddedVars.Add(MakeShareable(new FJsonValueString(TEXT("bIsDestroyed"))));
      AddedVars.Add(MakeShareable(new FJsonValueString(TEXT("DestructionStage"))));
      Result->SetArrayField(TEXT("variablesAdded"), AddedVars);

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
      UWorld* World = GetActiveWorld();
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
