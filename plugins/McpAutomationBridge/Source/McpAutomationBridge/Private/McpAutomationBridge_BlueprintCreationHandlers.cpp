#include "McpAutomationBridge_BlueprintCreationHandlers.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EditorAssetLibrary.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Factories/BlueprintFactory.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

// Respect build-rule's PublicDefinitions: if the build rule set
// MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1 then include the subsystem headers.
#if defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM) &&                               \
    (MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM == 1)
#if defined(__has_include)
#if __has_include("Subsystems/SubobjectDataSubsystem.h")
#include "Subsystems/SubobjectDataSubsystem.h"
#elif __has_include("SubobjectDataSubsystem.h")
#include "SubobjectDataSubsystem.h"
#elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#include "SubobjectData/SubobjectDataSubsystem.h"
#endif
#else
#include "SubobjectDataSubsystem.h"
#endif
#elif !defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM)
// If the build-rule did not define the macro, perform header probing here
// to discover whether the engine exposes SubobjectData headers.
#if defined(__has_include)
#if __has_include("Subsystems/SubobjectDataSubsystem.h")
#include "Subsystems/SubobjectDataSubsystem.h"
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#elif __has_include("SubobjectDataSubsystem.h")
#include "SubobjectDataSubsystem.h"
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#include "SubobjectData/SubobjectDataSubsystem.h"
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#else
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif
#else
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif
#else
#define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif

#endif // WITH_EDITOR

/**
 * @brief Probes subobject handles for a temporary blueprint and returns gathered handles to the requester.
 *
 * Creates a temporary probe Blueprint, attempts to gather subobject handles via the SubobjectDataSubsystem when available, and falls back to enumerating SimpleConstructionScript nodes when the subsystem is not available. In non-editor builds, sends a NOT_IMPLEMENTED response.
 *
 * @param Self Pointer to the MCP Automation Bridge subsystem handling the request.
 * @param RequestId Identifier for the incoming request; used when sending the response.
 * @param LocalPayload JSON payload for the probe request (may include "componentClass").
 * @param RequestingSocket WebSocket of the requesting client; may be null for non-socket invocations.
 * @return true if the request was handled (a response was sent). 
 */
bool FBlueprintCreationHandlers::HandleBlueprintProbeSubobjectHandle(
    UMcpAutomationBridgeSubsystem *Self, const FString &RequestId,
    const TSharedPtr<FJsonObject> &LocalPayload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  check(Self);
  // Local extraction
  FString ComponentClass;
  LocalPayload->TryGetStringField(TEXT("componentClass"), ComponentClass);
  if (ComponentClass.IsEmpty())
    ComponentClass = TEXT("StaticMeshComponent");

#if WITH_EDITOR
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("HandleBlueprintAction: blueprint_probe_subobject_handle start "
              "RequestId=%s componentClass=%s"),
         *RequestId, *ComponentClass);

  auto CleanupProbeAsset = [](UBlueprint *ProbeBP) {
#if WITH_EDITOR
    if (ProbeBP) {
      const FString AssetPath = ProbeBP->GetPathName();
      if (!UEditorAssetLibrary::DeleteLoadedAsset(ProbeBP)) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("Failed to delete loaded probe asset: %s"), *AssetPath);
      }

      if (!AssetPath.IsEmpty() &&
          UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
        if (!UEditorAssetLibrary::DeleteAsset(AssetPath)) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
                 TEXT("Failed to delete probe asset file: %s"), *AssetPath);
        }
      }
    }
#endif
  };

  const FString ProbeFolder = TEXT("/Game/Temp/MCPProbe");
  const FString ProbeName = FString::Printf(
      TEXT("MCP_Probe_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
  UBlueprint *CreatedBP = nullptr;
  {
    UBlueprintFactory *Factory = NewObject<UBlueprintFactory>();
    FAssetToolsModule &AssetToolsModule =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools"));
    UObject *NewObj = AssetToolsModule.Get().CreateAsset(
        ProbeName, ProbeFolder, UBlueprint::StaticClass(), Factory);
    if (!NewObj) {
      TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("componentClass"), ComponentClass);
      Err->SetStringField(TEXT("error"),
                          TEXT("Failed to create probe blueprint asset"));
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("blueprint_probe_subobject_handle: asset creation failed"));
      Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                   TEXT("Failed to create probe blueprint"),
                                   Err, TEXT("PROBE_CREATE_FAILED"));
      return true;
    }
    CreatedBP = Cast<UBlueprint>(NewObj);
    if (!CreatedBP) {
      TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
      Err->SetStringField(TEXT("componentClass"), ComponentClass);
      Err->SetStringField(TEXT("error"),
                          TEXT("Probe asset was not a Blueprint"));
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Warning,
          TEXT(
              "blueprint_probe_subobject_handle: created asset not blueprint"));
      Self->SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Probe asset created was not a Blueprint"), Err,
          TEXT("PROBE_CREATE_FAILED"));
      CleanupProbeAsset(CreatedBP);
      return true;
    }
    FAssetRegistryModule &Arm =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    Arm.Get().AssetCreated(CreatedBP);
  }

  TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
  ResultObj->SetStringField(TEXT("componentClass"), ComponentClass);
  ResultObj->SetBoolField(TEXT("success"), false);
  ResultObj->SetBoolField(TEXT("subsystemAvailable"), false);

#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
  if (USubobjectDataSubsystem *Subsystem =
          (GEngine ? GEngine->GetEngineSubsystem<USubobjectDataSubsystem>()
                   : nullptr)) {
    ResultObj->SetBoolField(TEXT("subsystemAvailable"), true);

    TArray<FSubobjectDataHandle> GatheredHandles;
    Subsystem->K2_GatherSubobjectDataForBlueprint(CreatedBP, GatheredHandles);

    TArray<TSharedPtr<FJsonValue>> HandleJsonArr;
    const UScriptStruct *HandleStruct = FSubobjectDataHandle::StaticStruct();
    for (int32 Index = 0; Index < GatheredHandles.Num(); ++Index) {
      const FSubobjectDataHandle &Handle = GatheredHandles[Index];
      FString Repr;
      if (HandleStruct) {
        Repr = FString::Printf(TEXT("%s@%p"), *HandleStruct->GetName(),
                               (void *)&Handle);
      } else {
        Repr = FString::Printf(TEXT("<subobject_handle_%d>"), Index);
      }
      HandleJsonArr.Add(MakeShared<FJsonValueString>(Repr));
    }
    ResultObj->SetArrayField(TEXT("gatheredHandles"), HandleJsonArr);
    ResultObj->SetBoolField(TEXT("success"), true);

    CleanupProbeAsset(CreatedBP);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                                 TEXT("Native probe completed"), ResultObj,
                                 FString());
    return true;
  }
#endif

  // Subsystem unavailable – fallback to simple SCS enumeration
  ResultObj->SetBoolField(TEXT("subsystemAvailable"), false);
  TArray<TSharedPtr<FJsonValue>> HandleJsonArr;
  if (CreatedBP && CreatedBP->SimpleConstructionScript) {
    const TArray<USCS_Node *> &Nodes =
        CreatedBP->SimpleConstructionScript->GetAllNodes();
    for (USCS_Node *Node : Nodes) {
      if (!Node || !Node->GetVariableName().IsValid())
        continue;
      HandleJsonArr.Add(MakeShared<FJsonValueString>(FString::Printf(
          TEXT("scs://%s"), *Node->GetVariableName().ToString())));
    }
  }
  if (HandleJsonArr.Num() == 0) {
    HandleJsonArr.Add(
        MakeShared<FJsonValueString>(TEXT("<probe_handle_stub>")));
  }
  ResultObj->SetArrayField(TEXT("gatheredHandles"), HandleJsonArr);
  ResultObj->SetBoolField(TEXT("success"), true);

  CleanupProbeAsset(CreatedBP);
  Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Fallback probe completed"), ResultObj,
                               FString());
  return true;
#else
  Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Blueprint probe requires editor build."),
                               nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif // WITH_EDITOR
}

/**
 * @brief Applies JSON-defined property values to a UObject, recursively handling nested object properties.
 *
 * For each entry in Properties, this function looks up a property on TargetObj by name and sets it:
 * - If the property is an object property and the JSON value is an object, the function recurses into that child object.
 * - Otherwise JSON primitives (string, number, boolean) are converted to text and applied using ImportText_Direct.
 * Unknown property names are silently ignored and no errors are thrown.
 *
 * @param TargetObj The UObject to modify. No action is performed if null.
 * @param Properties JSON object mapping property names to values; nested JSON objects map to subobjects/components.
 */
static void ApplyPropertiesToObject(UObject *TargetObj,
                                    const TSharedPtr<FJsonObject> &Properties) {
  if (!TargetObj || !Properties.IsValid()) {
    return;
  }

  for (const auto &Pair : Properties->Values) {
    FProperty *Property = TargetObj->GetClass()->FindPropertyByName(*Pair.Key);
    if (!Property) {
      continue;
    }

    // 1. Handle Object Properties (Recursion for Components/Subobjects)
    if (FObjectProperty *ObjProp = CastField<FObjectProperty>(Property)) {
      if (Pair.Value->Type == EJson::Object) {
        // This is likely a component or subobject property we want to edit
        // inline
        UObject *ChildObj =
            ObjProp->GetObjectPropertyValue_InContainer(TargetObj);
        if (ChildObj) {
          ApplyPropertiesToObject(ChildObj, Pair.Value->AsObject());
        }
        continue;
      }
    }

    // 2. Handle generic property setting via text import
    FString TextValue;

    if (Pair.Value->Type == EJson::String) {
      TextValue = Pair.Value->AsString();
    } else if (Pair.Value->Type == EJson::Number) {
      double Val = Pair.Value->AsNumber();
      // Heuristic: check if target is integer to avoid floating point syntax
      // issues if any
      if (Property->IsA<FIntProperty>() || Property->IsA<FInt64Property>() ||
          Property->IsA<FByteProperty>()) {
        TextValue = FString::Printf(TEXT("%lld"), (long long)Val);
      } else {
        TextValue = FString::SanitizeFloat(Val);
      }
    } else if (Pair.Value->Type == EJson::Boolean) {
      TextValue = Pair.Value->AsBool() ? TEXT("True") : TEXT("False");
    }

    if (!TextValue.IsEmpty()) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      Property->ImportText_Direct(
          *TextValue, Property->ContainerPtrToValuePtr<void>(TargetObj),
          TargetObj, 0);
#else
      // UE 5.0: Use ImportText with different signature
      Property->ImportText(*TextValue, Property->ContainerPtrToValuePtr<void>(TargetObj),
          PPF_None, TargetObj);
#endif
    }
  }
}

/**
 * @brief Create a new Blueprint asset from the provided payload and send a completion response to the requester (coalesces concurrent requests for the same path).
 *
 * Expected payload fields:
 * - "name" (string, required): asset name.
 * - "savePath" (string, optional, default "/Game"): destination folder.
 * - "parentClass" (string, optional): class path or name used as the Blueprint parent.
 * - "blueprintType" (string, optional): hint like "actor", "pawn", or "character" used when parent class is not resolved.
 * - "properties" (object, optional): JSON object of CDO properties to apply to the generated class default object.
 * - "waitForCompletion" (bool, optional): whether the caller intends to wait for completion (affects coalescing behavior).
 *
 * Behavior notes:
 * - Multiple concurrent requests that target the same SavePath/Name are coalesced so all waiters receive the same completion result.
 * - In editor builds, attempts to create the Blueprint (or returns an existing asset if present), applies optional CDO properties, registers the asset with the Asset Registry, and attempts to ensure asset availability (save/scan).
 * - In non-editor builds, responds with NOT_IMPLEMENTED indicating blueprint creation requires an editor build.
 *
 * @param Self Subsystem instance used to send responses and perform subsystem operations.
 * @param RequestId Identifier for the request; included in the completion response.
 * @param LocalPayload JSON payload describing the blueprint to create (see Expected payload fields above).
 * @param RequestingSocket Optional socket to which the immediate response should be sent; coalesced waiters will also be notified.
 * @return true if the request was handled and a response was sent to the requester (or coalesced waiters). 
 */
bool FBlueprintCreationHandlers::HandleBlueprintCreate(
    UMcpAutomationBridgeSubsystem *Self, const FString &RequestId,
    const TSharedPtr<FJsonObject> &LocalPayload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  check(Self);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("HandleBlueprintCreate ENTRY: RequestId=%s"), *RequestId);

  FString Name;
  LocalPayload->TryGetStringField(TEXT("name"), Name);
  if (Name.TrimStartAndEnd().IsEmpty()) {
    Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                 TEXT("blueprint_create requires a name."),
                                 nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SavePath;
  LocalPayload->TryGetStringField(TEXT("savePath"), SavePath);
  if (SavePath.TrimStartAndEnd().IsEmpty())
    SavePath = TEXT("/Game");
  FString ParentClassSpec;
  LocalPayload->TryGetStringField(TEXT("parentClass"), ParentClassSpec);
  FString BlueprintTypeSpec;
  LocalPayload->TryGetStringField(TEXT("blueprintType"), BlueprintTypeSpec);
  const double Now = FPlatformTime::Seconds();
  const FString CreateKey = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);

  // Check if client wants to wait for completion
  bool bWaitForCompletion = false;
  LocalPayload->TryGetBoolField(TEXT("waitForCompletion"), bWaitForCompletion);
  UE_LOG(
      LogMcpAutomationBridgeSubsystem, Log,
      TEXT("HandleBlueprintCreate: name=%s, savePath=%s, waitForCompletion=%s"),
      *Name, *SavePath, bWaitForCompletion ? TEXT("true") : TEXT("false"));

  // Track in-flight requests regardless so all waiters receive completion
  {
    FScopeLock Lock(&GBlueprintCreateMutex);
    if (GBlueprintCreateInflight.Contains(CreateKey)) {
      GBlueprintCreateInflight[CreateKey].Add(
          TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId,
                                                          RequestingSocket));
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("HandleBlueprintCreate: Coalescing request %s for %s"),
             *RequestId, *CreateKey);
      return true;
    }

    GBlueprintCreateInflight.Add(
        CreateKey, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>());
    GBlueprintCreateInflightTs.Add(CreateKey, Now);
    GBlueprintCreateInflight[CreateKey].Add(
        TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId,
                                                        RequestingSocket));
  }

#if WITH_EDITOR
  // Perform real creation (editor only)
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("HandleBlueprintCreate: Starting blueprint creation "
              "(WITH_EDITOR=1)"));
  UBlueprint *CreatedBlueprint = nullptr;
  FString CreatedNormalizedPath;
  FString CreationError;

  // Check if asset already exists to avoid "Overwrite" dialogs which can crash
  // the editor/driver
  FString PreExistingNormalized;
  FString PreExistingError;
  if (UBlueprint *PreExistingBP = LoadBlueprintAsset(
          CreateKey, PreExistingNormalized, PreExistingError)) {
    CreatedBlueprint = PreExistingBP;
    CreatedNormalizedPath = !PreExistingNormalized.TrimStartAndEnd().IsEmpty()
                                ? PreExistingNormalized
                                : PreExistingBP->GetPathName();
    if (CreatedNormalizedPath.Contains(TEXT("."))) {
      CreatedNormalizedPath =
          CreatedNormalizedPath.Left(CreatedNormalizedPath.Find(TEXT(".")));
    }

    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
    ResultPayload->SetStringField(TEXT("path"), CreatedNormalizedPath);
    ResultPayload->SetStringField(TEXT("assetPath"),
                                  PreExistingBP->GetPathName());
    ResultPayload->SetBoolField(TEXT("saved"), true);

    FScopeLock Lock(&GBlueprintCreateMutex);
    if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>> *Subs =
            GBlueprintCreateInflight.Find(CreateKey)) {
      for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>> &Pair :
           *Subs) {
        Self->SendAutomationResponse(Pair.Value, Pair.Key, true,
                                     TEXT("Blueprint already exists"),
                                     ResultPayload, FString());
      }
      GBlueprintCreateInflight.Remove(CreateKey);
      GBlueprintCreateInflightTs.Remove(CreateKey);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("blueprint_create RequestId=%s completed (existing blueprint "
                  "found early)."),
             *RequestId);
    } else {
      Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Blueprint already exists"),
                                   ResultPayload, FString());
    }

    return true;
  }

  UBlueprintFactory *Factory = NewObject<UBlueprintFactory>();
  UClass *ResolvedParent = nullptr;
  if (!ParentClassSpec.IsEmpty()) {
    if (ParentClassSpec.StartsWith(TEXT("/Script/"))) {
      ResolvedParent = LoadClass<UObject>(nullptr, *ParentClassSpec);
    } else {
      ResolvedParent = FindObject<UClass>(nullptr, *ParentClassSpec);
      // Avoid calling StaticLoadClass on a bare short name like "Actor" which
      // can generate engine warnings (e.g., "Class None.Actor"). For short
      // names, try common /Script prefixes instead.
      const bool bLooksPathLike = ParentClassSpec.Contains(TEXT("/")) ||
                                  ParentClassSpec.Contains(TEXT("."));
      if (!ResolvedParent && bLooksPathLike) {
        ResolvedParent =
            StaticLoadClass(UObject::StaticClass(), nullptr, *ParentClassSpec);
      }
      if (!ResolvedParent && !bLooksPathLike) {
        const TArray<FString> PrefixGuesses = {
            FString::Printf(TEXT("/Script/Engine.%s"), *ParentClassSpec),
            FString::Printf(TEXT("/Script/GameFramework.%s"), *ParentClassSpec),
            FString::Printf(TEXT("/Script/CoreUObject.%s"), *ParentClassSpec)};
        for (const FString &Guess : PrefixGuesses) {
          UClass *Loaded = FindObject<UClass>(nullptr, *Guess);
          if (!Loaded) {
            Loaded = StaticLoadClass(UObject::StaticClass(), nullptr, *Guess);
          }
          if (Loaded) {
            ResolvedParent = Loaded;
            break;
          }
        }
      }
      if (!ResolvedParent) {
        for (TObjectIterator<UClass> It; It; ++It) {
          UClass *C = *It;
          if (!C)
            continue;
          if (C->GetName().Equals(ParentClassSpec, ESearchCase::IgnoreCase)) {
            ResolvedParent = C;
            break;
          }
        }
      }
    }
  }
  if (!ResolvedParent && !BlueprintTypeSpec.IsEmpty()) {
    const FString LowerType = BlueprintTypeSpec.ToLower();
    if (LowerType == TEXT("actor"))
      ResolvedParent = AActor::StaticClass();
    else if (LowerType == TEXT("pawn"))
      ResolvedParent = APawn::StaticClass();
    else if (LowerType == TEXT("character"))
      ResolvedParent = ACharacter::StaticClass();
  }
  if (ResolvedParent)
    Factory->ParentClass = ResolvedParent;
  else
    Factory->ParentClass = AActor::StaticClass();

  FAssetToolsModule &AssetToolsModule =
      FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
  UObject *NewObj = AssetToolsModule.Get().CreateAsset(
      Name, SavePath, UBlueprint::StaticClass(), Factory);
  if (NewObj) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("CreateAsset returned object: name=%s path=%s class=%s"),
           *NewObj->GetName(), *NewObj->GetPathName(),
           *NewObj->GetClass()->GetName());
  }

  CreatedBlueprint = Cast<UBlueprint>(NewObj);

  // Apply optional CDO properties immediately if provided
  if (CreatedBlueprint && CreatedBlueprint->GeneratedClass) {
    const TSharedPtr<FJsonObject> *PropertiesPtr;
    if (LocalPayload->TryGetObjectField(TEXT("properties"), PropertiesPtr)) {
      UObject *CDO = CreatedBlueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        ApplyPropertiesToObject(CDO, *PropertiesPtr);
        // Mark dirty
        CreatedBlueprint->Modify();
      }
    }
  }

  if (!CreatedBlueprint) {
    // If creation failed, check whether a Blueprint already exists at the
    // target path. AssetTools will return nullptr when an asset with the
    // same name already exists; in that case we should treat this as an
    // idempotent success instead of a hard failure.
    FString ExistingNormalized;
    FString ExistingError;
    UBlueprint *ExistingBP =
        LoadBlueprintAsset(CreateKey, ExistingNormalized, ExistingError);
    if (ExistingBP) {
      CreatedBlueprint = ExistingBP;
      CreatedNormalizedPath = !ExistingNormalized.TrimStartAndEnd().IsEmpty()
                                  ? ExistingNormalized
                                  : ExistingBP->GetPathName();
      if (CreatedNormalizedPath.Contains(TEXT("."))) {
        CreatedNormalizedPath =
            CreatedNormalizedPath.Left(CreatedNormalizedPath.Find(TEXT(".")));
      }

      TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
      ResultPayload->SetStringField(TEXT("path"), CreatedNormalizedPath);
      ResultPayload->SetStringField(TEXT("assetPath"),
                                    ExistingBP->GetPathName());
      ResultPayload->SetBoolField(TEXT("saved"), true);

      FScopeLock Lock(&GBlueprintCreateMutex);
      if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>> *Subs =
              GBlueprintCreateInflight.Find(CreateKey)) {
        for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>> &Pair :
             *Subs) {
          Self->SendAutomationResponse(Pair.Value, Pair.Key, true,
                                       TEXT("Blueprint already exists"),
                                       ResultPayload, FString());
        }
        GBlueprintCreateInflight.Remove(CreateKey);
        GBlueprintCreateInflightTs.Remove(CreateKey);
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("blueprint_create RequestId=%s completed (existing "
                    "blueprint)."),
               *RequestId);
      } else {
        Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                                     TEXT("Blueprint already exists"),
                                     ResultPayload, FString());
      }

      return true;
    }

    CreationError =
        FString::Printf(TEXT("Created asset is not a Blueprint: %s"),
                        NewObj ? *NewObj->GetPathName() : TEXT("<null>"));

    {
      FScopeLock Lock(&GBlueprintCreateMutex);
      if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>> *Subs =
              GBlueprintCreateInflight.Find(CreateKey)) {
        for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>> &Pair :
             *Subs) {
          Self->SendAutomationResponse(Pair.Value, Pair.Key, false,
                                       CreationError, nullptr,
                                       TEXT("CREATE_FAILED"));
        }
        GBlueprintCreateInflight.Remove(CreateKey);
        GBlueprintCreateInflightTs.Remove(CreateKey);
      } else {
        Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                     CreationError, nullptr,
                                     TEXT("CREATE_FAILED"));
      }
    }
    return true;
  }

  CreatedNormalizedPath = CreatedBlueprint->GetPathName();
  if (CreatedNormalizedPath.Contains(TEXT(".")))
    CreatedNormalizedPath =
        CreatedNormalizedPath.Left(CreatedNormalizedPath.Find(TEXT(".")));
  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  AssetRegistryModule.AssetCreated(CreatedBlueprint);

  TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
  ResultPayload->SetStringField(TEXT("path"), CreatedNormalizedPath);
  ResultPayload->SetStringField(TEXT("assetPath"),
                                CreatedBlueprint->GetPathName());
  ResultPayload->SetBoolField(TEXT("saved"), true);
  FScopeLock Lock(&GBlueprintCreateMutex);
  if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>> *Subs =
          GBlueprintCreateInflight.Find(CreateKey)) {
    for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>> &Pair : *Subs) {
      Self->SendAutomationResponse(Pair.Value, Pair.Key, true,
                                   TEXT("Blueprint created"), ResultPayload,
                                   FString());
    }
    GBlueprintCreateInflight.Remove(CreateKey);
    GBlueprintCreateInflightTs.Remove(CreateKey);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("blueprint_create RequestId=%s completed (coalesced)."),
           *RequestId);
  } else {
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                                 TEXT("Blueprint created"), ResultPayload,
                                 FString());
  }

  TWeakObjectPtr<UBlueprint> WeakCreatedBp = CreatedBlueprint;
  if (WeakCreatedBp.IsValid()) {
    UBlueprint *BP = WeakCreatedBp.Get();
#if WITH_EDITOR
    // Force immediate save and registry scan to ensure availability
    SaveLoadedAssetThrottled(BP, -1.0, true);
    ScanPathSynchronous(BP->GetOutermost()->GetName());
#endif
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("HandleBlueprintCreate EXIT: RequestId=%s created successfully"),
         *RequestId);
  return true;
#else
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("HandleBlueprintCreate: WITH_EDITOR not defined - cannot create "
              "blueprints"));
  Self->SendAutomationResponse(
      RequestingSocket, RequestId, false,
      TEXT("Blueprint creation requires editor build."), nullptr,
      TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}