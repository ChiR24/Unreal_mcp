#include "McpAutomationBridge_BlueprintCreationHandlers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"

#if WITH_EDITOR
#include "Factories/BlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectIterator.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"

// Respect build-rule's PublicDefinitions: if the build rule set
// MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1 then include the subsystem headers.
#if defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM) && (MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM == 1)
#  if defined(__has_include)
#    if __has_include("Subsystems/SubobjectDataSubsystem.h")
#      include "Subsystems/SubobjectDataSubsystem.h"
#    elif __has_include("SubobjectDataSubsystem.h")
#      include "SubobjectDataSubsystem.h"
#    elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#      include "SubobjectData/SubobjectDataSubsystem.h"
#    endif
#  else
#    include "SubobjectDataSubsystem.h"
#  endif
#elif !defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM)
// If the build-rule did not define the macro, perform header probing here
// to discover whether the engine exposes SubobjectData headers.
#  if defined(__has_include)
#    if __has_include("Subsystems/SubobjectDataSubsystem.h")
#      include "Subsystems/SubobjectDataSubsystem.h"
#      define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#    elif __has_include("SubobjectDataSubsystem.h")
#      include "SubobjectDataSubsystem.h"
#      define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#    elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#      include "SubobjectData/SubobjectDataSubsystem.h"
#      define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#    else
#      define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#    endif
#  else
#    define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#  endif
#else
#  define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif

#endif // WITH_EDITOR

bool FBlueprintCreationHandlers::HandleBlueprintProbeSubobjectHandle(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& LocalPayload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    check(Self);
    // Local extraction
    FString ComponentClass; LocalPayload->TryGetStringField(TEXT("componentClass"), ComponentClass);
    if (ComponentClass.IsEmpty()) ComponentClass = TEXT("StaticMeshComponent");

#if WITH_EDITOR
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: blueprint_probe_subobject_handle start RequestId=%s componentClass=%s"), *RequestId, *ComponentClass);

    auto CleanupProbeAsset = [](UBlueprint* ProbeBP)
    {
#if WITH_EDITOR
        if (ProbeBP)
        {
            const FString AssetPath = ProbeBP->GetPathName();
            if (!UEditorAssetLibrary::DeleteLoadedAsset(ProbeBP))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Failed to delete loaded probe asset: %s"), *AssetPath);
            }
            
            if (!AssetPath.IsEmpty() && UEditorAssetLibrary::DoesAssetExist(AssetPath))
            {
                if (!UEditorAssetLibrary::DeleteAsset(AssetPath))
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Failed to delete probe asset file: %s"), *AssetPath);
                }
            }
        }
#endif
    };

    const FString ProbeFolder = TEXT("/Game/Temp/MCPProbe");
    const FString ProbeName = FString::Printf(TEXT("MCP_Probe_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    UBlueprint* CreatedBP = nullptr;
    {
        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        UObject* NewObj = AssetToolsModule.Get().CreateAsset(ProbeName, ProbeFolder, UBlueprint::StaticClass(), Factory);
        if (!NewObj)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("componentClass"), ComponentClass);
            Err->SetStringField(TEXT("error"), TEXT("Failed to create probe blueprint asset"));
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("blueprint_probe_subobject_handle: asset creation failed"));
            Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create probe blueprint"), Err, TEXT("PROBE_CREATE_FAILED"));
            return true;
        }
        CreatedBP = Cast<UBlueprint>(NewObj);
        if (!CreatedBP)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
            Err->SetStringField(TEXT("componentClass"), ComponentClass);
            Err->SetStringField(TEXT("error"), TEXT("Probe asset was not a Blueprint"));
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("blueprint_probe_subobject_handle: created asset not blueprint"));
            Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Probe asset created was not a Blueprint"), Err, TEXT("PROBE_CREATE_FAILED"));
            CleanupProbeAsset(CreatedBP);
            return true;
        }
        FAssetRegistryModule& Arm = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        Arm.Get().AssetCreated(CreatedBP);
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("componentClass"), ComponentClass);
    ResultObj->SetBoolField(TEXT("success"), false);
    ResultObj->SetBoolField(TEXT("subsystemAvailable"), false);

#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
    if (USubobjectDataSubsystem* Subsystem = (GEngine ? GEngine->GetEngineSubsystem<USubobjectDataSubsystem>() : nullptr))
    {
        ResultObj->SetBoolField(TEXT("subsystemAvailable"), true);

        TArray<FSubobjectDataHandle> GatheredHandles;
        Subsystem->K2_GatherSubobjectDataForBlueprint(CreatedBP, GatheredHandles);

        TArray<TSharedPtr<FJsonValue>> HandleJsonArr;
        const UScriptStruct* HandleStruct = FSubobjectDataHandle::StaticStruct();
        for (int32 Index = 0; Index < GatheredHandles.Num(); ++Index)
        {
            const FSubobjectDataHandle& Handle = GatheredHandles[Index];
            FString Repr;
            if (HandleStruct)
            {
                Repr = FString::Printf(TEXT("%s@%p"), *HandleStruct->GetName(), (void*)&Handle);
            }
            else
            {
                Repr = FString::Printf(TEXT("<subobject_handle_%d>"), Index);
            }
            HandleJsonArr.Add(MakeShared<FJsonValueString>(Repr));
        }
        ResultObj->SetArrayField(TEXT("gatheredHandles"), HandleJsonArr);
        ResultObj->SetBoolField(TEXT("success"), true);

        CleanupProbeAsset(CreatedBP);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Native probe completed"), ResultObj, FString());
        return true;
    }
#endif

    // Subsystem unavailable – fallback to simple SCS enumeration
    ResultObj->SetBoolField(TEXT("subsystemAvailable"), false);
    TArray<TSharedPtr<FJsonValue>> HandleJsonArr;
    if (CreatedBP && CreatedBP->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& Nodes = CreatedBP->SimpleConstructionScript->GetAllNodes();
        for (USCS_Node* Node : Nodes)
        {
            if (!Node || !Node->GetVariableName().IsValid()) continue;
            HandleJsonArr.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("scs://%s"), *Node->GetVariableName().ToString())));
        }
    }
    if (HandleJsonArr.Num() == 0)
    {
        HandleJsonArr.Add(MakeShared<FJsonValueString>(TEXT("<probe_handle_stub>")));
    }
    ResultObj->SetArrayField(TEXT("gatheredHandles"), HandleJsonArr);
    ResultObj->SetBoolField(TEXT("success"), true);

    CleanupProbeAsset(CreatedBP);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Fallback probe completed"), ResultObj, FString());
    return true;
#else
    Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint probe requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif // WITH_EDITOR
}

bool FBlueprintCreationHandlers::HandleBlueprintCreate(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& LocalPayload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    check(Self);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintCreate ENTRY: RequestId=%s"), *RequestId);
    
    FString Name; LocalPayload->TryGetStringField(TEXT("name"), Name);
    if (Name.TrimStartAndEnd().IsEmpty()) { Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_create requires a name."), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
    FString SavePath; LocalPayload->TryGetStringField(TEXT("savePath"), SavePath); if (SavePath.TrimStartAndEnd().IsEmpty()) SavePath = TEXT("/Game");
    FString ParentClassSpec; LocalPayload->TryGetStringField(TEXT("parentClass"), ParentClassSpec);
    FString BlueprintTypeSpec; LocalPayload->TryGetStringField(TEXT("blueprintType"), BlueprintTypeSpec);
    const double Now = FPlatformTime::Seconds();
    const FString CreateKey = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);

    // Check if client wants to wait for completion
    bool bWaitForCompletion = false;
    LocalPayload->TryGetBoolField(TEXT("waitForCompletion"), bWaitForCompletion);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintCreate: name=%s, savePath=%s, waitForCompletion=%s"), *Name, *SavePath, bWaitForCompletion ? TEXT("true") : TEXT("false"));

    // Track in-flight requests regardless so all waiters receive completion
    {
        FScopeLock Lock(&GBlueprintCreateMutex);
        if (GBlueprintCreateInflight.Contains(CreateKey))
        {
            GBlueprintCreateInflight[CreateKey].Add(TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId, RequestingSocket));
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintCreate: Coalescing request %s for %s"), *RequestId, *CreateKey);
            return true;
        }

        GBlueprintCreateInflight.Add(CreateKey, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>());
        GBlueprintCreateInflightTs.Add(CreateKey, Now);
        GBlueprintCreateInflight[CreateKey].Add(TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId, RequestingSocket));
    }

#if WITH_EDITOR
    // Perform real creation (editor only)
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintCreate: Starting blueprint creation (WITH_EDITOR=1)"));
    UBlueprint* CreatedBlueprint = nullptr;
    FString CreatedNormalizedPath;
    FString CreationError;

    // Check if asset already exists to avoid "Overwrite" dialogs which can crash the editor/driver
    FString PreExistingNormalized;
    FString PreExistingError;
    if (UBlueprint* PreExistingBP = LoadBlueprintAsset(CreateKey, PreExistingNormalized, PreExistingError))
    {
        CreatedBlueprint = PreExistingBP;
        CreatedNormalizedPath = !PreExistingNormalized.TrimStartAndEnd().IsEmpty() ? PreExistingNormalized : PreExistingBP->GetPathName();
        if (CreatedNormalizedPath.Contains(TEXT(".")))
        {
            CreatedNormalizedPath = CreatedNormalizedPath.Left(CreatedNormalizedPath.Find(TEXT(".")));
        }

        TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
        ResultPayload->SetStringField(TEXT("path"), CreatedNormalizedPath);
        ResultPayload->SetStringField(TEXT("assetPath"), PreExistingBP->GetPathName());
        ResultPayload->SetBoolField(TEXT("saved"), true);

        FScopeLock Lock(&GBlueprintCreateMutex);
        if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs = GBlueprintCreateInflight.Find(CreateKey))
        {
            for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>& Pair : *Subs)
            {
                Self->SendAutomationResponse(Pair.Value, Pair.Key, true, TEXT("Blueprint already exists"), ResultPayload, FString());
            }
            GBlueprintCreateInflight.Remove(CreateKey);
            GBlueprintCreateInflightTs.Remove(CreateKey);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_create RequestId=%s completed (existing blueprint found early)."), *RequestId);
        }
        else
        {
            Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint already exists"), ResultPayload, FString());
        }

        return true;
    }

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    UClass* ResolvedParent = nullptr;
    if (!ParentClassSpec.IsEmpty())
    {
        if (ParentClassSpec.StartsWith(TEXT("/Script/")))
        {
            ResolvedParent = LoadClass<UObject>(nullptr, *ParentClassSpec);
        }
        else
        {
            ResolvedParent = FindObject<UClass>(nullptr, *ParentClassSpec);
            // Avoid calling StaticLoadClass on a bare short name like "Actor" which
            // can generate engine warnings (e.g., "Class None.Actor"). For short names,
            // try common /Script prefixes instead.
            const bool bLooksPathLike = ParentClassSpec.Contains(TEXT("/")) || ParentClassSpec.Contains(TEXT("."));
            if (!ResolvedParent && bLooksPathLike)
            {
                ResolvedParent = StaticLoadClass(UObject::StaticClass(), nullptr, *ParentClassSpec);
            }
            if (!ResolvedParent && !bLooksPathLike)
            {
                const TArray<FString> PrefixGuesses = {
                    FString::Printf(TEXT("/Script/Engine.%s"), *ParentClassSpec),
                    FString::Printf(TEXT("/Script/GameFramework.%s"), *ParentClassSpec),
                    FString::Printf(TEXT("/Script/CoreUObject.%s"), *ParentClassSpec)
                };
                for (const FString& Guess : PrefixGuesses)
                {
                    UClass* Loaded = FindObject<UClass>(nullptr, *Guess);
                    if (!Loaded)
                    {
                        Loaded = StaticLoadClass(UObject::StaticClass(), nullptr, *Guess);
                    }
                    if (Loaded)
                    {
                        ResolvedParent = Loaded;
                        break;
                    }
                }
            }
            if (!ResolvedParent)
            {
                for (TObjectIterator<UClass> It; It; ++It)
                {
                    UClass* C = *It;
                    if (!C) continue;
                    if (C->GetName().Equals(ParentClassSpec, ESearchCase::IgnoreCase)) { ResolvedParent = C; break; }
                }
            }
        }
    }
    if (!ResolvedParent && !BlueprintTypeSpec.IsEmpty())
    {
        const FString LowerType = BlueprintTypeSpec.ToLower();
        if (LowerType == TEXT("actor")) ResolvedParent = AActor::StaticClass(); else if (LowerType == TEXT("pawn")) ResolvedParent = APawn::StaticClass(); else if (LowerType == TEXT("character")) ResolvedParent = ACharacter::StaticClass();
    }
    if (ResolvedParent) Factory->ParentClass = ResolvedParent; else Factory->ParentClass = AActor::StaticClass();

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    UObject* NewObj = AssetToolsModule.Get().CreateAsset(Name, SavePath, UBlueprint::StaticClass(), Factory);
    if (NewObj)
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("CreateAsset returned object: name=%s path=%s class=%s"), *NewObj->GetName(), *NewObj->GetPathName(), *NewObj->GetClass()->GetName());
    }

    CreatedBlueprint = Cast<UBlueprint>(NewObj);
    if (!CreatedBlueprint)
    {
        // If creation failed, check whether a Blueprint already exists at the
        // target path. AssetTools will return nullptr when an asset with the
        // same name already exists; in that case we should treat this as an
        // idempotent success instead of a hard failure.
        FString ExistingNormalized;
        FString ExistingError;
        UBlueprint* ExistingBP = LoadBlueprintAsset(CreateKey, ExistingNormalized, ExistingError);
        if (ExistingBP)
        {
            CreatedBlueprint = ExistingBP;
            CreatedNormalizedPath = !ExistingNormalized.TrimStartAndEnd().IsEmpty() ? ExistingNormalized : ExistingBP->GetPathName();
            if (CreatedNormalizedPath.Contains(TEXT(".")))
            {
                CreatedNormalizedPath = CreatedNormalizedPath.Left(CreatedNormalizedPath.Find(TEXT(".")));
            }

            TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
            ResultPayload->SetStringField(TEXT("path"), CreatedNormalizedPath);
            ResultPayload->SetStringField(TEXT("assetPath"), ExistingBP->GetPathName());
            ResultPayload->SetBoolField(TEXT("saved"), true);

            FScopeLock Lock(&GBlueprintCreateMutex);
            if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs = GBlueprintCreateInflight.Find(CreateKey))
            {
                for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>& Pair : *Subs)
                {
                    Self->SendAutomationResponse(Pair.Value, Pair.Key, true, TEXT("Blueprint already exists"), ResultPayload, FString());
                }
                GBlueprintCreateInflight.Remove(CreateKey);
                GBlueprintCreateInflightTs.Remove(CreateKey);
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_create RequestId=%s completed (existing blueprint)."), *RequestId);
            }
            else
            {
                Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint already exists"), ResultPayload, FString());
            }

            return true;
        }

        CreationError = FString::Printf(TEXT("Created asset is not a Blueprint: %s"), NewObj ? *NewObj->GetPathName() : TEXT("<null>"));
        
        {
            FScopeLock Lock(&GBlueprintCreateMutex);
            if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs = GBlueprintCreateInflight.Find(CreateKey))
            {
                for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>& Pair : *Subs)
                {
                    Self->SendAutomationResponse(Pair.Value, Pair.Key, false, CreationError, nullptr, TEXT("CREATE_FAILED"));
                }
                GBlueprintCreateInflight.Remove(CreateKey);
                GBlueprintCreateInflightTs.Remove(CreateKey);
            }
            else
            {
                Self->SendAutomationResponse(RequestingSocket, RequestId, false, CreationError, nullptr, TEXT("CREATE_FAILED"));
            }
        }
        return true;
    }

    CreatedNormalizedPath = CreatedBlueprint->GetPathName(); if (CreatedNormalizedPath.Contains(TEXT("."))) CreatedNormalizedPath = CreatedNormalizedPath.Left(CreatedNormalizedPath.Find(TEXT(".")));
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")); AssetRegistryModule.AssetCreated(CreatedBlueprint);

    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>(); ResultPayload->SetStringField(TEXT("path"), CreatedNormalizedPath); ResultPayload->SetStringField(TEXT("assetPath"), CreatedBlueprint->GetPathName()); ResultPayload->SetBoolField(TEXT("saved"), true);
    FScopeLock Lock(&GBlueprintCreateMutex);
    if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs = GBlueprintCreateInflight.Find(CreateKey))
    {
        for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>& Pair : *Subs) { Self->SendAutomationResponse(Pair.Value, Pair.Key, true, TEXT("Blueprint created"), ResultPayload, FString()); }
        GBlueprintCreateInflight.Remove(CreateKey); GBlueprintCreateInflightTs.Remove(CreateKey);
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_create RequestId=%s completed (coalesced)."), *RequestId);
    }
    else { Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint created"), ResultPayload, FString()); }

    TWeakObjectPtr<UBlueprint> WeakCreatedBp = CreatedBlueprint;
    if (WeakCreatedBp.IsValid())
    {
        UBlueprint* BP = WeakCreatedBp.Get();
#if WITH_EDITOR
        SaveLoadedAssetThrottled(BP);
#endif
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintCreate EXIT: RequestId=%s created successfully"), *RequestId);
    return true;
#else
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("HandleBlueprintCreate: WITH_EDITOR not defined - cannot create blueprints"));
    Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint creation requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
