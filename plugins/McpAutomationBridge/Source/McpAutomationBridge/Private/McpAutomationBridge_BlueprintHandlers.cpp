#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "Async/Async.h"
#if WITH_EDITOR
#include "Factories/BlueprintFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UObjectIterator.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
// Editor-only engine includes used by blueprint creation helpers
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "IPythonScriptPlugin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include <functional>
#if defined(__has_include)
#  if __has_include("EdGraph/EdGraphSchema_K2.h")
#    include "EdGraph/EdGraphSchema_K2.h"
#    define MCP_HAS_EDGRAPH_SCHEMA_K2 1
#  else
#    define MCP_HAS_EDGRAPH_SCHEMA_K2 0
#  endif
#else
#  include "EdGraph/EdGraphSchema_K2.h"
#  define MCP_HAS_EDGRAPH_SCHEMA_K2 1
#endif
// Respect build-rule's PublicDefinitions: if the build rule set
// MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM=1 then include the subsystem headers.
#if defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM) && (MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM == 1)
#if defined(__has_include)
#  if __has_include("Subsystems/SubobjectDataSubsystem.h")
#    include "Subsystems/SubobjectDataSubsystem.h"
#  elif __has_include("SubobjectDataSubsystem.h")
#    include "SubobjectDataSubsystem.h"
#  elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#    include "SubobjectData/SubobjectDataSubsystem.h"
#  endif
#else
#  include "SubobjectDataSubsystem.h"
#endif
#elif !defined(MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM)
// If the build-rule did not define the macro, perform header probing here
// to discover whether the engine exposes SubobjectData headers.
#if defined(__has_include)
#  if __has_include("Subsystems/SubobjectDataSubsystem.h")
#    include "Subsystems/SubobjectDataSubsystem.h"
#    define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#  elif __has_include("SubobjectDataSubsystem.h")
#    include "SubobjectDataSubsystem.h"
#    define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#  elif __has_include("SubobjectData/SubobjectDataSubsystem.h")
#    include "SubobjectData/SubobjectDataSubsystem.h"
#    define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 1
#  else
#    define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#  endif
#else
#  define MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM 0
#endif
#endif
#if MCP_HAS_EDGRAPH_SCHEMA_K2
#define MCP_PC_Float UEdGraphSchema_K2::PC_Float
#define MCP_PC_Int UEdGraphSchema_K2::PC_Int
#define MCP_PC_Boolean UEdGraphSchema_K2::PC_Boolean
#define MCP_PC_String UEdGraphSchema_K2::PC_String
#define MCP_PC_Name UEdGraphSchema_K2::PC_Name
#define MCP_PC_Object UEdGraphSchema_K2::PC_Object
#define MCP_PC_Wildcard UEdGraphSchema_K2::PC_Wildcard
#else
static const FName MCP_PC_Float(TEXT("float"));
static const FName MCP_PC_Int(TEXT("int"));
static const FName MCP_PC_Boolean(TEXT("bool"));
static const FName MCP_PC_String(TEXT("string"));
static const FName MCP_PC_Name(TEXT("name"));
static const FName MCP_PC_Object(TEXT("object"));
static const FName MCP_PC_Wildcard(TEXT("wildcard"));
#endif
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
namespace McpAutomationBridge
{
    template<typename, typename = void>
    struct THasK2Add : std::false_type {};

    template<typename T>
    struct THasK2Add<T, std::void_t<decltype(std::declval<T>().K2_AddNewSubobject(std::declval<FAddNewSubobjectParams>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasAdd : std::false_type {};

    template<typename T>
    struct THasAdd<T, std::void_t<decltype(std::declval<T>().AddNewSubobject(std::declval<FAddNewSubobjectParams>()))>> : std::true_type {};

    // Newer engine builds expose an AddNewSubobject overload that takes a
    // failure reason out-parameter. Detect that signature as well so we can
    // call the correct overload depending on the engine version.
    template<typename, typename = void>
    struct THasAddTwoArg : std::false_type {};

    template<typename T>
    struct THasAddTwoArg<T, std::void_t<decltype(std::declval<T>().AddNewSubobject(std::declval<FAddNewSubobjectParams>(), std::declval<FText&>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THandleHasIsValid : std::false_type {};

    template<typename T>
    struct THandleHasIsValid<T, std::void_t<decltype(std::declval<T>().IsValid())>> : std::true_type {};

    template<typename, typename = void>
    struct THasRename : std::false_type {};

    template<typename T>
    struct THasRename<T, std::void_t<decltype(std::declval<T>().RenameSubobjectMemberVariable(std::declval<UBlueprint*>(), std::declval<FSubobjectDataHandle>(), std::declval<FName>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasK2Remove : std::false_type {};

    template<typename T>
    struct THasK2Remove<T, std::void_t<decltype(std::declval<T>().K2_RemoveSubobject(std::declval<UBlueprint*>(), std::declval<FSubobjectDataHandle>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasRemove : std::false_type {};

    template<typename T>
    struct THasRemove<T, std::void_t<decltype(std::declval<T>().RemoveSubobject(std::declval<UBlueprint*>(), std::declval<FSubobjectDataHandle>()))>> : std::true_type {};

    // Some engine versions expose a DeleteSubobject API instead of RemoveSubobject;
    // detect it so we can call the correct symbol if present.
    template<typename, typename = void>
    struct THasDeleteSubobject : std::false_type {};

    // Engine variations: DeleteSubobject commonly has the signature
    // DeleteSubobject(const FSubobjectDataHandle& ContextHandle,
    //                const FSubobjectDataHandle& SubobjectToDelete,
    //                UBlueprint* BPContext = nullptr)
    // Detect that signature so we can call it correctly when present.
    template<typename T>
    struct THasDeleteSubobject<T, std::void_t<decltype(std::declval<T>().DeleteSubobject(std::declval<const FSubobjectDataHandle&>(), std::declval<const FSubobjectDataHandle&>(), std::declval<UBlueprint*>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasK2Attach : std::false_type {};

    template<typename T>
    struct THasK2Attach<T, std::void_t<decltype(std::declval<T>().K2_AttachSubobject(std::declval<UBlueprint*>(), std::declval<FSubobjectDataHandle>(), std::declval<FSubobjectDataHandle>()))>> : std::true_type {};

    template<typename, typename = void>
    struct THasAttach : std::false_type {};

    template<typename T>
    struct THasAttach<T, std::void_t<decltype(std::declval<T>().AttachSubobject(std::declval<FSubobjectDataHandle>(), std::declval<FSubobjectDataHandle>()))>> : std::true_type {};
}
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleBlueprintAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.StartsWith(TEXT("blueprint_")) && !Lower.StartsWith(TEXT("manage_blueprint"))) return false;

    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();

    // Helper to resolve requested blueprint path (honors 'requestedPath' or 'blueprintCandidates')
    auto ResolveBlueprintRequestedPath = [&]() -> FString
    {
        FString Req;
        if (LocalPayload->TryGetStringField(TEXT("requestedPath"), Req) && !Req.TrimStartAndEnd().IsEmpty()) return Req;
        const TArray<TSharedPtr<FJsonValue>>* CandidateArray = nullptr;
        if (LocalPayload->TryGetArrayField(TEXT("blueprintCandidates"), CandidateArray) && CandidateArray && CandidateArray->Num() > 0)
        {
            for (const TSharedPtr<FJsonValue>& V : *CandidateArray)
            {
                if (!V.IsValid() || V->Type != EJson::String) continue;
                FString Candidate = V->AsString();
                if (Candidate.TrimStartAndEnd().IsEmpty()) continue;
                // Return the first existing candidate
                FString Norm;
                if (FindBlueprintNormalizedPath(Candidate, Norm)) return Candidate;
            }
        }
        return FString();
    };

    // Ensure registry entry helper used by multiple blueprint_* handlers
    auto EnsureBlueprintEntry = [&](const FString& Path) -> TSharedPtr<FJsonObject>
    {
        if (Path.IsEmpty()) return MakeShared<FJsonObject>();
        if (TSharedPtr<FJsonObject>* Found = GBlueprintRegistry.Find(Path)) return *Found;
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("blueprintPath"), Path);
        Entry->SetArrayField(TEXT("variables"), TArray<TSharedPtr<FJsonValue>>() );
        Entry->SetArrayField(TEXT("constructionScripts"), TArray<TSharedPtr<FJsonValue>>() );
        Entry->SetObjectField(TEXT("defaults"), MakeShared<FJsonObject>());
        Entry->SetObjectField(TEXT("metadata"), MakeShared<FJsonObject>());
        GBlueprintRegistry.Add(Path, Entry);
        return Entry;
    };

    // Blueprint existence probe
    if (Lower.Equals(TEXT("blueprint_exists")))
    {
        const double EntryTime = FPlatformTime::Seconds();
        if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_exists payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

        TArray<FString> CandidatePaths;
        const TArray<TSharedPtr<FJsonValue>>* CandidateArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("candidates"), CandidateArray) && CandidateArray != nullptr && CandidateArray->Num() > 0)
        {
            for (const TSharedPtr<FJsonValue>& V : *CandidateArray)
            {
                if (!V.IsValid() || V->Type != EJson::String) continue;
                const FString Candidate = V->AsString();
                if (!Candidate.TrimStartAndEnd().IsEmpty()) CandidatePaths.Add(Candidate);
            }
        }
        else
        {
            FString Single;
            if (Payload->TryGetStringField(TEXT("requestedPath"), Single) && !Single.TrimStartAndEnd().IsEmpty()) CandidatePaths.Add(Single);
            else { SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_exists requires candidates or requestedPath."), TEXT("INVALID_PAYLOAD")); return true; }
        }

        if (CandidatePaths.Num() == 0) { SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_exists requires candidates or requestedPath."), TEXT("INVALID_PAYLOAD")); return true; }

        TArray<TSharedPtr<FJsonValue>> TriedValues; TriedValues.Reserve(CandidatePaths.Num());
        const double Now = FPlatformTime::Seconds();
        FString CanonKey = FString::Join(CandidatePaths, TEXT("|"));
        if (!CanonKey.IsEmpty())
        {
            if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Inflight = GBlueprintExistsInflight.Find(CanonKey))
            {
                Inflight->Add(TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId, RequestingSocket));
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Coalesced blueprint_exists for key=%s (subscribers=%d)"), *CanonKey, Inflight->Num());
                return true;
            }
        }

        // Register in-flight
    if (!CanonKey.IsEmpty()) { GBlueprintExistsInflight.Add(CanonKey, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>() ); GBlueprintExistsInflight[CanonKey].Add(TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId, RequestingSocket)); }

        FString FoundNormalized; bool bAnyFound = false;
        for (const FString& Candidate : CandidatePaths)
        {
            TriedValues.Add(MakeShared<FJsonValueString>(Candidate));
            if (FString* Cached = GBlueprintExistCacheNormalized.Find(Candidate)) { FoundNormalized = *Cached; bAnyFound = true; break; }
            FString FastNorm;
            if (FindBlueprintNormalizedPath(Candidate, FastNorm)) { FoundNormalized = FastNorm; bAnyFound = true; break; }
            // Try expensive load
            FString Norm; FString Err; UBlueprint* BP = LoadBlueprintAsset(Candidate, Norm, Err);
            if (BP) { FoundNormalized = Norm; bAnyFound = true; break; }
        }

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
        if (bAnyFound) { Resp->SetBoolField(TEXT("exists"), true); Resp->SetStringField(TEXT("found"), FoundNormalized); }
        else { Resp->SetBoolField(TEXT("exists"), false); Resp->SetArrayField(TEXT("triedCandidates"), TriedValues); }

        // Send to all subscribers
        if (!CanonKey.IsEmpty())
        {
            if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs = GBlueprintExistsInflight.Find(CanonKey))
            {
                for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>& Pair : *Subs) { SendAutomationResponse(Pair.Value, Pair.Key, bAnyFound, bAnyFound ? TEXT("Blueprint exists") : TEXT("Blueprint does not exist"), Resp, bAnyFound ? FString() : TEXT("NOT_FOUND")); }
                GBlueprintExistsInflight.Remove(CanonKey);
            }
            return true;
        }

        // Fallback: send directly to requester
        SendAutomationResponse(RequestingSocket, RequestId, bAnyFound, bAnyFound ? TEXT("Blueprint exists") : TEXT("Blueprint does not exist"), Resp, bAnyFound ? FString() : TEXT("NOT_FOUND"));
        return true;
    }

    // Blueprint get: return registry entry or load live blueprint when available
    if (Lower.Equals(TEXT("blueprint_get")))
    {
        FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_get requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
        // Prefer registry entry when present
        if (TSharedPtr<FJsonObject>* Found = GBlueprintRegistry.Find(Path))
        {
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint fetched from registry"), *Found, FString());
            return true;
        }

        // Try to load live blueprint in editor builds
#if WITH_EDITOR
        FString Normalized; FString LoadError; UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadError);
        if (!BP)
        {
            TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), LoadError);
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to load blueprint"), Err, TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("blueprintPath"), Normalized);
        // Variables
        TArray<TSharedPtr<FJsonValue>> Vars;
        for (const FBPVariableDescription& V : BP->NewVariables)
        {
            TSharedPtr<FJsonObject> Vobj = MakeShared<FJsonObject>();
            Vobj->SetStringField(TEXT("name"), ConvertToString(V.VarName));
            Vobj->SetStringField(TEXT("friendlyName"), ConvertToString(V.FriendlyName));
            Vobj->SetStringField(TEXT("category"), ConvertToString(V.Category)); // basic
            // attempt to record pin category
            FString PinCat = V.VarType.PinCategory.IsNone() ? TEXT("unknown") : V.VarType.PinCategory.ToString();
            Vobj->SetStringField(TEXT("typeCategory"), PinCat);
            Vars.Add(MakeShared<FJsonValueObject>(Vobj));
        }
        Resp->SetArrayField(TEXT("variables"), Vars);

        // Functions: list function graphs names
    TArray<TSharedPtr<FJsonValue>> Funcs;
    // Best-effort: some editor utility helpers may differ across engine versions.
    TArray<UEdGraph*> FuncGraphs;
    // Attempt to collect function graphs through known entry points
#if MCP_HAS_EDGRAPH_SCHEMA_K2
    FBlueprintEditorUtils::GetAllGraphs(BP, FuncGraphs);
#else
    // Fallback: inspect blueprint's function graphs containers when utilities are not present
    for (UEdGraph* G : BP->FunctionGraphs) { FuncGraphs.Add(G); }
#endif
    for (UEdGraph* G : FuncGraphs) { if (!G) continue; const FString GName = G->GetName(); if (GName.StartsWith(TEXT("FunctionGraph")) || GName.StartsWith(TEXT("Function"))) { TSharedPtr<FJsonObject> Fobj = MakeShared<FJsonObject>(); Fobj->SetStringField(TEXT("name"), G->GetName()); Funcs.Add(MakeShared<FJsonValueObject>(Fobj)); } }
        Resp->SetArrayField(TEXT("functions"), Funcs);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint loaded"), Resp, FString());
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint query requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // For other blueprint_* actions, defer to the older in-file implementations
    // (create, modify_scs, compile, add_variable, add_event, etc.). For
    // readability these are intentionally moved here as a consolidated block.

    if (Lower.Equals(TEXT("blueprint_create")))
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Entered blueprint_create handler: RequestId=%s PayloadExists=%d"), *RequestId, Payload.IsValid());
        if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_create payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

        FString Name; if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_create requires a non-empty name."), TEXT("INVALID_NAME")); return true; }
        FString SavePath = TEXT("/Game/Blueprints"); Payload->TryGetStringField(TEXT("savePath"), SavePath);
        // Normalize path (same logic as prior)
        SavePath = SavePath.Replace(TEXT("\\"), TEXT("/")); SavePath = SavePath.Replace(TEXT("//"), TEXT("/")); if (SavePath.EndsWith(TEXT("/"))) SavePath = SavePath.LeftChop(1);
        if (SavePath.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) SavePath = FString::Printf(TEXT("/Game%s"), *SavePath.RightChop(8));
        if (!SavePath.StartsWith(TEXT("/Game"))) SavePath = FString::Printf(TEXT("/Game/%s"), *SavePath.Replace(TEXT("/"), TEXT("")));

        FString ParentClassSpec; Payload->TryGetStringField(TEXT("parentClass"), ParentClassSpec);
        FString BlueprintTypeSpec; Payload->TryGetStringField(TEXT("blueprintType"), BlueprintTypeSpec);

        const FString CreateKey = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
        {
            FScopeLock Lock(&GBlueprintCreateMutex);
            // Purge stale entries
            double Now = FPlatformTime::Seconds(); TArray<FString> ToPurge;
            for (const auto& Pair : GBlueprintCreateInflightTs) if (Now - Pair.Value > GBlueprintCreateStaleTimeoutSec) ToPurge.Add(Pair.Key);
            for (const FString& K : ToPurge) { GBlueprintCreateInflight.Remove(K); GBlueprintCreateInflightTs.Remove(K); }

            if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs = GBlueprintCreateInflight.Find(CreateKey))
            {
                Subs->Add(TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId, RequestingSocket));
                TSharedPtr<FJsonObject> FastResp = MakeShared<FJsonObject>(); FastResp->SetStringField(TEXT("path"), FString::Printf(TEXT("%s/%s"), *SavePath, *Name)); FastResp->SetStringField(TEXT("assetPath"), FString::Printf(TEXT("%s.%s"), *SavePath, *Name));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint created (queued)"), FastResp, FString());
                return true;
            }

            // blueprint_modify_scs: schedule SCS ops with fast-mode support and deferred
            // game-thread application when necessary. Mirrors the original logic but
            // kept here to reduce the size of the main subsystem file.
            if (Lower.Equals(TEXT("blueprint_modify_scs")))
            {
                const double HandlerStartTimeSec = FPlatformTime::Seconds();
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_modify_scs handler start (RequestId=%s)"), *RequestId);

                if (!LocalPayload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

                // Resolve blueprint path or candidate list
                FString BlueprintPath;
                TArray<FString> CandidatePaths;
                if (!LocalPayload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) || BlueprintPath.TrimStartAndEnd().IsEmpty())
                {
                    const TArray<TSharedPtr<FJsonValue>>* CandidateArray = nullptr;
                    if (!LocalPayload->TryGetArrayField(TEXT("blueprintCandidates"), CandidateArray) || CandidateArray == nullptr || CandidateArray->Num() == 0)
                    {
                        SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs requires a non-empty blueprintPath or blueprintCandidates."), TEXT("INVALID_BLUEPRINT"));
                        return true;
                    }
                    for (const TSharedPtr<FJsonValue>& Val : *CandidateArray)
                    {
                        if (!Val.IsValid()) continue;
                        const FString Candidate = Val->AsString();
                        if (!Candidate.TrimStartAndEnd().IsEmpty()) CandidatePaths.Add(Candidate);
                    }
                    if (CandidatePaths.Num() == 0)
                    {
                        SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs blueprintCandidates array provided but contains no valid strings."), TEXT("INVALID_BLUEPRINT_CANDIDATES"));
                        return true;
                    }
                }

                // Operations are required
                const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
                if (!LocalPayload->TryGetArrayField(TEXT("operations"), OperationsArray) || OperationsArray == nullptr)
                {
                    SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs requires an operations array."), TEXT("INVALID_OPERATIONS"));
                    return true;
                }

                // Flags
                bool bCompile = false; if (LocalPayload->HasField(TEXT("compile")) && !LocalPayload->TryGetBoolField(TEXT("compile"), bCompile)) { SendAutomationError(RequestingSocket, RequestId, TEXT("compile must be a boolean."), TEXT("INVALID_COMPILE_FLAG")); return true; }
                bool bSave = false; if (LocalPayload->HasField(TEXT("save")) && !LocalPayload->TryGetBoolField(TEXT("save"), bSave)) { SendAutomationError(RequestingSocket, RequestId, TEXT("save must be a boolean."), TEXT("INVALID_SAVE_FLAG")); return true; }

                // Resolve the blueprint asset (explicit path preferred, then candidates)
                FString NormalizedBlueprintPath;
                FString LoadError;
                TArray<FString> TriedCandidates;

                if (!BlueprintPath.IsEmpty())
                {
                    TriedCandidates.Add(BlueprintPath);
                    if (FindBlueprintNormalizedPath(BlueprintPath, NormalizedBlueprintPath))
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_modify_scs: resolved explicit path %s -> %s"), *BlueprintPath, *NormalizedBlueprintPath);
                    }
                    else
                    {
                        LoadError = FString::Printf(TEXT("Blueprint not found for path %s"), *BlueprintPath);
                    }
                }

                if (NormalizedBlueprintPath.IsEmpty() && CandidatePaths.Num() > 0)
                {
                    for (const FString& Candidate : CandidatePaths)
                    {
                        TriedCandidates.Add(Candidate);
                        FString CandidateNormalized;
                        if (FindBlueprintNormalizedPath(Candidate, CandidateNormalized))
                        {
                            NormalizedBlueprintPath = CandidateNormalized;
                            LoadError.Empty();
                            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_modify_scs: resolved candidate %s -> %s"), *Candidate, *CandidateNormalized);
                            break;
                        }
                        LoadError = FString::Printf(TEXT("Candidate not found: %s"), *Candidate);
                    }
                }

                if (NormalizedBlueprintPath.IsEmpty())
                {
                    TSharedPtr<FJsonObject> ErrPayload = MakeShared<FJsonObject>();
                    if (TriedCandidates.Num() > 0)
                    {
                        TArray<TSharedPtr<FJsonValue>> TriedValues;
                        for (const FString& C : TriedCandidates) TriedValues.Add(MakeShared<FJsonValueString>(C));
                        ErrPayload->SetArrayField(TEXT("triedCandidates"), TriedValues);
                    }
                    SendAutomationResponse(RequestingSocket, RequestId, false, LoadError.IsEmpty() ? TEXT("Blueprint not found") : LoadError, ErrPayload, TEXT("BLUEPRINT_NOT_FOUND"));
                    return true;
                }

                if (OperationsArray->Num() == 0)
                {
                    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>();
                    ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
                    ResultPayload->SetArrayField(TEXT("operations"), TArray<TSharedPtr<FJsonValue>>());
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("No SCS operations supplied."), ResultPayload, FString());
                    return true;
                }

                // Prevent concurrent SCS modifications against the same blueprint.
                const FString BusyKey = NormalizedBlueprintPath;
                if (!BusyKey.IsEmpty())
                {
                    if (GBlueprintBusySet.Contains(BusyKey)) { SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Blueprint %s is busy with another modification."), *BusyKey), nullptr, TEXT("BLUEPRINT_BUSY")); return true; }

                    GBlueprintBusySet.Add(BusyKey);
                    this->CurrentBusyBlueprintKey = BusyKey;
                    this->bCurrentBlueprintBusyMarked = true;
                    this->bCurrentBlueprintBusyScheduled = false;

                    // If we exit before scheduling the deferred work, clear the busy flag
                    ON_SCOPE_EXIT
                    {
                        if (this->bCurrentBlueprintBusyMarked && !this->bCurrentBlueprintBusyScheduled)
                        {
                            GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey);
                            this->bCurrentBlueprintBusyMarked = false;
                            this->CurrentBusyBlueprintKey.Empty();
                        }
                    };
                }

                // Make a shallow copy of the operations array so the deferred lambda
                // can safely reference them after this function returns.
                TArray<TSharedPtr<FJsonValue>> DeferredOps = *OperationsArray;

                // Lightweight validation of operations
                for (int32 Index = 0; Index < DeferredOps.Num(); ++Index)
                {
                    const TSharedPtr<FJsonValue>& OperationValue = DeferredOps[Index];
                    if (!OperationValue.IsValid() || OperationValue->Type != EJson::Object) { SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Operation at index %d is not an object."), Index), TEXT("INVALID_OPERATION_PAYLOAD")); return true; }
                    const TSharedPtr<FJsonObject> OperationObject = OperationValue->AsObject();
                    FString OperationType; if (!OperationObject->TryGetStringField(TEXT("type"), OperationType) || OperationType.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Operation at index %d missing type."), Index), TEXT("INVALID_OPERATION_TYPE")); return true; }
                }

                // Mark busy as scheduled (the deferred worker will clear it)
                this->bCurrentBlueprintBusyScheduled = true;

                // Build immediate acknowledgement payload summarizing scheduled ops
                TArray<TSharedPtr<FJsonValue>> ImmediateSummaries; ImmediateSummaries.Reserve(DeferredOps.Num());
                for (int32 Index = 0; Index < DeferredOps.Num(); ++Index)
                {
                    const TSharedPtr<FJsonObject> OpObj = DeferredOps[Index]->AsObject();
                    TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
                    FString Type; OpObj->TryGetStringField(TEXT("type"), Type);
                    Summary->SetNumberField(TEXT("index"), Index);
                    Summary->SetStringField(TEXT("type"), Type.IsEmpty() ? TEXT("unknown") : Type);
                    Summary->SetBoolField(TEXT("scheduled"), true);
                    ImmediateSummaries.Add(MakeShared<FJsonValueObject>(Summary));
                }

                TSharedPtr<FJsonObject> AckPayload = MakeShared<FJsonObject>();
                AckPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
                AckPayload->SetStringField(TEXT("matchedCandidate"), NormalizedBlueprintPath);
                AckPayload->SetArrayField(TEXT("operations"), ImmediateSummaries);
                AckPayload->SetBoolField(TEXT("scheduled"), true);
                AckPayload->SetBoolField(TEXT("compiled"), false);
                AckPayload->SetBoolField(TEXT("saved"), false);

                const FString AckMessage = FString::Printf(TEXT("Scheduled %d SCS operation(s) for application."), DeferredOps.Num());
                SendAutomationResponse(RequestingSocket, RequestId, true, AckMessage, AckPayload, FString());
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_modify_scs: RequestId=%s scheduled %d ops and returned ack."), *RequestId, DeferredOps.Num());

                // Fast-mode: apply operations to in-memory registry immediately
                if (IsFastMode(LocalPayload))
                {
                    TArray<TSharedPtr<FJsonValue>> FinalSummaries; TArray<FString> LocalWarnings;
                    ApplySCSOperationsToRegistry(NormalizedBlueprintPath, DeferredOps, FinalSummaries, LocalWarnings);

                    TSharedPtr<FJsonObject> CompletionResult = MakeShared<FJsonObject>();
                    CompletionResult->SetArrayField(TEXT("operations"), FinalSummaries);
                    CompletionResult->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
                    CompletionResult->SetBoolField(TEXT("compiled"), bCompile);
                    CompletionResult->SetBoolField(TEXT("saved"), bSave);
                    if (LocalWarnings.Num() > 0)
                    {
                        TArray<TSharedPtr<FJsonValue>> WVals; for (const FString& W : LocalWarnings) WVals.Add(MakeShared<FJsonValueString>(W)); CompletionResult->SetArrayField(TEXT("warnings"), WVals);
                    }

                    // Broadcast completion event
                    TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>(); Notify->SetStringField(TEXT("type"), TEXT("automation_event")); Notify->SetStringField(TEXT("event"), TEXT("modify_scs_completed")); Notify->SetStringField(TEXT("requestId"), RequestId); Notify->SetObjectField(TEXT("result"), CompletionResult); SendControlMessage(Notify);

                    // Try to send final automation_response to the original requester
                    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>(); ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath); ResultPayload->SetArrayField(TEXT("operations"), FinalSummaries); ResultPayload->SetBoolField(TEXT("compiled"), bCompile); ResultPayload->SetBoolField(TEXT("saved"), bSave);
                    if (LocalWarnings.Num() > 0)
                    {
                        TArray<TSharedPtr<FJsonValue>> WVals2; WVals2.Reserve(LocalWarnings.Num()); for (const FString& W : LocalWarnings) WVals2.Add(MakeShared<FJsonValueString>(W)); ResultPayload->SetArrayField(TEXT("warnings"), WVals2);
                    }

                    const FString Message = FString::Printf(TEXT("Processed %d SCS operation(s) (fast-mode)."), FinalSummaries.Num());
                    SendAutomationResponse(RequestingSocket, RequestId, true, Message, ResultPayload, FString());

                    // Release busy flag
                    if (!this->CurrentBusyBlueprintKey.IsEmpty() && GBlueprintBusySet.Contains(this->CurrentBusyBlueprintKey)) { GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey); }
                    this->bCurrentBlueprintBusyMarked = false; this->bCurrentBlueprintBusyScheduled = false; this->CurrentBusyBlueprintKey.Empty();

                    return true;
                }

                // Defer actual SCS application to the game thread
                AsyncTask(ENamedThreads::GameThread, [this, RequestId, DeferredOps, NormalizedBlueprintPath, bCompile, bSave, TriedCandidates, HandlerStartTimeSec, RequestingSocket]() mutable {
                    TSharedPtr<FJsonObject> CompletionResult = MakeShared<FJsonObject>();
                    TArray<FString> LocalWarnings; TArray<TSharedPtr<FJsonValue>> FinalSummaries; bool bOk = false;

                    // (Re)load the blueprint on the game thread
                    FString LocalNormalized; FString LocalLoadError; UBlueprint* LocalBP = LoadBlueprintAsset(NormalizedBlueprintPath, LocalNormalized, LocalLoadError);

                    if (!LocalBP)
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Deferred SCS application failed to load blueprint %s: %s"), *NormalizedBlueprintPath, *LocalLoadError);
                        CompletionResult->SetStringField(TEXT("error"), LocalLoadError);
                    }
                    else
                    {
                        USimpleConstructionScript* LocalSCS = LocalBP->SimpleConstructionScript;
                        if (!LocalSCS) { UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Deferred SCS application: SCS unavailable for %s"), *NormalizedBlueprintPath); CompletionResult->SetStringField(TEXT("error"), TEXT("SCS_UNAVAILABLE")); }
                        else
                        {
                            LocalBP->Modify(); LocalSCS->Modify();

                            for (int32 Index = 0; Index < DeferredOps.Num(); ++Index)
                            {
                                const double OpStart = FPlatformTime::Seconds();
                                const TSharedPtr<FJsonValue>& V = DeferredOps[Index];
                                if (!V.IsValid() || V->Type != EJson::Object) continue;
                                const TSharedPtr<FJsonObject> Op = V->AsObject(); FString OpType; Op->TryGetStringField(TEXT("type"), OpType); const FString NormalizedType = OpType.ToLower();
                                TSharedPtr<FJsonObject> OpSummary = MakeShared<FJsonObject>(); OpSummary->SetNumberField(TEXT("index"), Index); OpSummary->SetStringField(TEXT("type"), NormalizedType);

                                if (NormalizedType == TEXT("modify_component"))
                                {
                                    FString ComponentName; Op->TryGetStringField(TEXT("componentName"), ComponentName);
                                    const TSharedPtr<FJsonValue> TransformVal = Op->TryGetField(TEXT("transform"));
                                    const TSharedPtr<FJsonObject> TransformObj = TransformVal.IsValid() && TransformVal->Type == EJson::Object ? TransformVal->AsObject() : nullptr;
                                    if (!ComponentName.IsEmpty() && TransformObj.IsValid())
                                    {
                                        USCS_Node* Node = FindScsNodeByName(LocalSCS, ComponentName);
                                        if (Node && Node->ComponentTemplate && Node->ComponentTemplate->IsA<USceneComponent>())
                                        {
                                            USceneComponent* SceneTemplate = Cast<USceneComponent>(Node->ComponentTemplate);
                                            FVector Location = SceneTemplate->GetRelativeLocation(); FRotator Rotation = SceneTemplate->GetRelativeRotation(); FVector Scale = SceneTemplate->GetRelativeScale3D();
                                            ReadVectorField(TransformObj, TEXT("location"), Location, Location); ReadRotatorField(TransformObj, TEXT("rotation"), Rotation, Rotation); ReadVectorField(TransformObj, TEXT("scale"), Scale, Scale);
                                            SceneTemplate->SetRelativeLocation(Location); SceneTemplate->SetRelativeRotation(Rotation); SceneTemplate->SetRelativeScale3D(Scale);
                                            OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                        }
                                        else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Component not found or template missing")); }
                                    }
                                }
                                else if (NormalizedType == TEXT("add_component"))
                                {
                                    FString ComponentName; Op->TryGetStringField(TEXT("componentName"), ComponentName);
                                    FString ComponentClassPath; Op->TryGetStringField(TEXT("componentClass"), ComponentClassPath);
                                    FString AttachToName; Op->TryGetStringField(TEXT("attachTo"), AttachToName);
                                    FSoftClassPath ComponentClassSoftPath(ComponentClassPath); UClass* ComponentClass = ComponentClassSoftPath.TryLoadClass<UActorComponent>();
                                    if (!ComponentClass) ComponentClass = FindObject<UClass>(nullptr, *ComponentClassPath);
                                    if (!ComponentClass)
                                    {
                                        const TArray<FString> Prefixes = { TEXT("/Script/Engine."), TEXT("/Script/UMG."), TEXT("/Script/Paper2D.") };
                                        for (const FString& Prefix : Prefixes)
                                        {
                                            const FString Guess = Prefix + ComponentClassPath; UClass* TryClass = FindObject<UClass>(nullptr, *Guess); if (!TryClass) TryClass = StaticLoadClass(UActorComponent::StaticClass(), nullptr, *Guess); if (TryClass) { ComponentClass = TryClass; break; }
                                        }
                                    }
                                    if (!ComponentClass)
                                    {
                                        OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Component class not found"));
                                    }
                                    else
                                    {
                                        USCS_Node* ExistingNode = FindScsNodeByName(LocalSCS, ComponentName);
                                        if (ExistingNode) { OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), ComponentName); OpSummary->SetStringField(TEXT("warning"), TEXT("Component already exists")); }
                                        else
                                        {
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
                                            bool bAddedViaSubsystem = false;
                                            FString AdditionMethodStr;
                                            USubobjectDataSubsystem* Subsystem = nullptr;
                                            if (GEngine) Subsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
                                            if (Subsystem)
                                            {
                                                // Gather existing handles for blueprint context
                                                TArray<FSubobjectDataHandle> ExistingHandles;
                                                Subsystem->K2_GatherSubobjectDataForBlueprint(LocalBP, ExistingHandles);
                                                FSubobjectDataHandle ParentHandle;
                                                if (ExistingHandles.Num() > 0)
                                                {
                                                    // Prefer a handle that matches the requested AttachToName when provided
                                                    bool bFoundParentByName = false;
                                                    if (!AttachToName.TrimStartAndEnd().IsEmpty())
                                                    {
                                                        const UScriptStruct* HandleStruct = FSubobjectDataHandle::StaticStruct();
                                                        for (const FSubobjectDataHandle& H : ExistingHandles)
                                                        {
                                                            if (!HandleStruct) continue;
                                                            FString HText;
                                                            HandleStruct->ExportText(HText, &H, nullptr, nullptr, PPF_None, nullptr);
                                                            if (HText.Contains(AttachToName, ESearchCase::IgnoreCase))
                                                            {
                                                                ParentHandle = H;
                                                                bFoundParentByName = true;
                                                                break;
                                                            }
                                                        }
                                                    }
                                                    if (!bFoundParentByName) ParentHandle = ExistingHandles[0];
                                                }

                                                // Attempt to use the native FAddNewSubobjectParams + AddNewSubobject API
                                                {
                                                    using namespace McpAutomationBridge;
                                                    constexpr bool bHasK2Add = THasK2Add<USubobjectDataSubsystem>::value;
                                                    constexpr bool bHasAdd = THasAdd<USubobjectDataSubsystem>::value;
                                                    constexpr bool bHasAddTwoArg = THasAddTwoArg<USubobjectDataSubsystem>::value;
                                                    constexpr bool bHandleHasIsValid = THandleHasIsValid<FSubobjectDataHandle>::value;
                                                    constexpr bool bHasRename = THasRename<USubobjectDataSubsystem>::value;

                                                    bool bTriedNative = false;
                                                    FSubobjectDataHandle NewHandle;
                                                    if constexpr (bHasK2Add || bHasAdd || bHasAddTwoArg)
                                                    {
                                                        FAddNewSubobjectParams Params;
                                                        Params.ParentHandle = ParentHandle;
                                                        Params.NewClass = ComponentClass;
                                                        Params.BlueprintContext = LocalBP;

                                                        if constexpr (bHasK2Add)
                                                        {
                                                            NewHandle = Subsystem->K2_AddNewSubobject(Params);
                                                            bTriedNative = true;
                                                            AdditionMethodStr = TEXT("SubobjectDataSubsystem.K2_AddNewSubobject");
                                                        }
                                                        else if constexpr (bHasAdd)
                                                        {
                                                            NewHandle = Subsystem->AddNewSubobject(Params);
                                                            bTriedNative = true;
                                                            AdditionMethodStr = TEXT("SubobjectDataSubsystem.AddNewSubobject");
                                                        }
                                                        else if constexpr (bHasAddTwoArg)
                                                        {
                                                            FText FailReason;
                                                            NewHandle = Subsystem->AddNewSubobject(Params, FailReason);
                                                            bTriedNative = true;
                                                            AdditionMethodStr = TEXT("SubobjectDataSubsystem.AddNewSubobject(WithFailReason)");
                                                        }

                                                        bool bHandleValid = true;
                                                        if (bTriedNative)
                                                        {
                                                            if constexpr (bHandleHasIsValid)
                                                            {
                                                                bHandleValid = NewHandle.IsValid();
                                                            }
                                                            if (bHandleValid)
                                                            {
                                                                if constexpr (bHasRename)
                                                                {
                                                                    Subsystem->RenameSubobjectMemberVariable(LocalBP, NewHandle, FName(*ComponentName));
                                                                }
                                                                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LocalBP);
                                                                FKismetEditorUtilities::CompileBlueprint(LocalBP);
#if WITH_EDITOR
                                                                UEditorAssetLibrary::SaveLoadedAsset(LocalBP);
#endif
                                                                bAddedViaSubsystem = true;
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            if (bAddedViaSubsystem)
                                            {
                                                OpSummary->SetBoolField(TEXT("success"), true);
                                                OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                                if (!AdditionMethodStr.IsEmpty()) OpSummary->SetStringField(TEXT("additionMethod"), AdditionMethodStr);
                                            }
                                            else
                                            {
                                                // Fallback to legacy SCS creation if the subsystem path is unavailable or failed
                                                USCS_Node* NewNode = LocalSCS->CreateNode(ComponentClass, *ComponentName);
                                                if (NewNode)
                                                {
                                                    if (!AttachToName.TrimStartAndEnd().IsEmpty())
                                                    {
                                                        if (USCS_Node* ParentNode = FindScsNodeByName(LocalSCS, AttachToName)) { ParentNode->AddChildNode(NewNode); }
                                                        else { LocalSCS->AddNode(NewNode); }
                                                    }
                                                    else { LocalSCS->AddNode(NewNode); }
                                                    OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                                }
                                                else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Failed to create SCS node")); }
                                            }
#else
                                            // SubobjectDataSubsystem unavailable - keep legacy SCS behavior
                                            USCS_Node* NewNode = LocalSCS->CreateNode(ComponentClass, *ComponentName);
                                            if (NewNode)
                                            {
                                                if (!AttachToName.TrimStartAndEnd().IsEmpty())
                                                {
                                                    if (USCS_Node* ParentNode = FindScsNodeByName(LocalSCS, AttachToName)) { ParentNode->AddChildNode(NewNode); }
                                                    else { LocalSCS->AddNode(NewNode); }
                                                }
                                                else { LocalSCS->AddNode(NewNode); }
                                                OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                            }
                                            else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Failed to create SCS node")); }
#endif
                                        }
                                    }
                                }
                                else if (NormalizedType == TEXT("remove_component"))
                                {
                                    FString ComponentName; Op->TryGetStringField(TEXT("componentName"), ComponentName);
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
                                    bool bRemoved = false;
                                    USubobjectDataSubsystem* Subsystem = nullptr;
                                    if (GEngine) Subsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
                                    if (Subsystem)
                                    {
                                        // Gather handles and find matching handle by textual inspection
                                        TArray<FSubobjectDataHandle> ExistingHandles;
                                        Subsystem->K2_GatherSubobjectDataForBlueprint(LocalBP, ExistingHandles);
                                        FSubobjectDataHandle FoundHandle;
                                        bool bFound = false;
                                        const UScriptStruct* HandleStruct = FSubobjectDataHandle::StaticStruct();
                                        for (const FSubobjectDataHandle& H : ExistingHandles)
                                        {
                                            if (!HandleStruct) continue;
                                            FString HText; HandleStruct->ExportText(HText, &H, nullptr, nullptr, PPF_None, nullptr);
                                            if (HText.Contains(ComponentName, ESearchCase::IgnoreCase)) { FoundHandle = H; bFound = true; break; }
                                        }

                                        if (bFound)
                                        {
                                            constexpr bool bHasK2Remove = McpAutomationBridge::THasK2Remove<USubobjectDataSubsystem>::value;
                                            constexpr bool bHasRemove = McpAutomationBridge::THasRemove<USubobjectDataSubsystem>::value;
                                            constexpr bool bHasDelete = McpAutomationBridge::THasDeleteSubobject<USubobjectDataSubsystem>::value;

                                            if constexpr (bHasK2Remove)
                                            {
                                                Subsystem->K2_RemoveSubobject(LocalBP, FoundHandle);
                                                bRemoved = true;
                                            }
                                            else if constexpr (bHasRemove)
                                            {
                                                Subsystem->RemoveSubobject(LocalBP, FoundHandle);
                                                bRemoved = true;
                                            }
                                            else if constexpr (bHasDelete)
                                            {
                                                // Newer API expects (ContextHandle, SubobjectToDelete, Blueprint*)
                                                FSubobjectDataHandle ContextHandle = ExistingHandles.Num() > 0 ? ExistingHandles[0] : FoundHandle;
                                                Subsystem->DeleteSubobject(ContextHandle, FoundHandle, LocalBP);
                                                bRemoved = true;
                                            }
                                        }
                                    }
                                    // If the subsystem path did not remove the component, fall
                                    // back to the legacy SCS node removal behavior so older
                                    // engine flows and edge cases still work.
                                    if (bRemoved)
                                    {
                                        OpSummary->SetBoolField(TEXT("success"), true);
                                        OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                    }
                                    else
                                    {
                                        // Fallback: try legacy SCS node removal
                                        if (USCS_Node* TargetNode = FindScsNodeByName(LocalSCS, ComponentName))
                                        {
                                            LocalSCS->RemoveNode(TargetNode);
                                            OpSummary->SetBoolField(TEXT("success"), true);
                                            OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                        }
                                        else
                                        {
                                            OpSummary->SetBoolField(TEXT("success"), false);
                                            OpSummary->SetStringField(TEXT("warning"), TEXT("Component not found; remove skipped"));
                                        }
                                    }
                                }
                                else if (NormalizedType == TEXT("attach_component"))
                                {
                                    FString AttachComponentName; Op->TryGetStringField(TEXT("componentName"), AttachComponentName);
                                    FString ParentName; Op->TryGetStringField(TEXT("parentComponent"), ParentName); if (ParentName.IsEmpty()) Op->TryGetStringField(TEXT("attachTo"), ParentName);
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
                                    bool bAttached = false;
                                    USubobjectDataSubsystem* Subsystem = nullptr;
                                    if (GEngine) Subsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
                                    if (Subsystem)
                                    {
                                        TArray<FSubobjectDataHandle> Handles;
                                        Subsystem->K2_GatherSubobjectDataForBlueprint(LocalBP, Handles);
                                        FSubobjectDataHandle ChildHandle, ParentHandle;
                                        const UScriptStruct* HandleStruct = FSubobjectDataHandle::StaticStruct();
                                        for (const FSubobjectDataHandle& H : Handles)
                                        {
                                            if (!HandleStruct) continue;
                                            FString HText; HandleStruct->ExportText(HText, &H, nullptr, nullptr, PPF_None, nullptr);
                                            if (!AttachComponentName.IsEmpty() && HText.Contains(AttachComponentName, ESearchCase::IgnoreCase)) ChildHandle = H;
                                            if (!ParentName.IsEmpty() && HText.Contains(ParentName, ESearchCase::IgnoreCase)) ParentHandle = H;
                                        }
                                        constexpr bool bHasK2Attach = McpAutomationBridge::THasK2Attach<USubobjectDataSubsystem>::value;
                                        constexpr bool bHasAttach = McpAutomationBridge::THasAttach<USubobjectDataSubsystem>::value;
                                        if (ChildHandle.IsValid() && ParentHandle.IsValid())
                                        {
                                            if constexpr (bHasK2Attach)
                                            {
                                                Subsystem->K2_AttachSubobject(LocalBP, ChildHandle, ParentHandle);
                                                bAttached = true;
                                            }
                                            else if constexpr (bHasAttach)
                                            {
                                                bAttached = Subsystem->AttachSubobject(ParentHandle, ChildHandle);
                                            }
                                            else
                                            {
                                                // No native attach API available; will fall back to legacy SCS attach below
                                            }
                                        }
                                    }
                                    if (bAttached)
                                    {
                                        OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), AttachComponentName); OpSummary->SetStringField(TEXT("attachedTo"), ParentName);
                                    }
                                    else
                                    {
                                        USCS_Node* ChildNode = FindScsNodeByName(LocalSCS, AttachComponentName); USCS_Node* ParentNode = FindScsNodeByName(LocalSCS, ParentName);
                                        if (ChildNode && ParentNode) { ParentNode->AddChildNode(ChildNode); OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), AttachComponentName); OpSummary->SetStringField(TEXT("attachedTo"), ParentName); }
                                        else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Attach failed: child or parent not found")); }
                                    }
#else
                                    USCS_Node* ChildNode = FindScsNodeByName(LocalSCS, AttachComponentName); USCS_Node* ParentNode = FindScsNodeByName(LocalSCS, ParentName);
                                    if (ChildNode && ParentNode) { ParentNode->AddChildNode(ChildNode); OpSummary->SetBoolField(TEXT("success"), true); OpSummary->SetStringField(TEXT("componentName"), AttachComponentName); OpSummary->SetStringField(TEXT("attachedTo"), ParentName); }
                                    else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Attach failed: child or parent not found")); }
#endif
#endif
                                }
                                else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Unknown operation type")); }

                                const double OpElapsedMs = (FPlatformTime::Seconds() - OpStart) * 1000.0; OpSummary->SetNumberField(TEXT("durationMs"), OpElapsedMs); FinalSummaries.Add(MakeShared<FJsonValueObject>(OpSummary));
                            }

                            bOk = FinalSummaries.Num() > 0; CompletionResult->SetArrayField(TEXT("operations"), FinalSummaries);
                        }
                    }

                    // Compile/save as requested
                    bool bSaveResult = false;
                    if (bSave && LocalBP)
                    {
        #if WITH_EDITOR
                        bSaveResult = UEditorAssetLibrary::SaveLoadedAsset(LocalBP);
                        if (!bSaveResult) LocalWarnings.Add(TEXT("Blueprint failed to save during deferred apply; check output log."));
        #endif
                    }
                    if (bCompile && LocalBP)
                    {
        #if WITH_EDITOR
                        FKismetEditorUtilities::CompileBlueprint(LocalBP);
        #endif
                    }

                    CompletionResult->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath);
                    CompletionResult->SetBoolField(TEXT("compiled"), bCompile);
                    CompletionResult->SetBoolField(TEXT("saved"), bSave && bSaveResult);
                    if (LocalWarnings.Num() > 0)
                    {
                        TArray<TSharedPtr<FJsonValue>> WVals; for (const FString& W : LocalWarnings) WVals.Add(MakeShared<FJsonValueString>(W)); CompletionResult->SetArrayField(TEXT("warnings"), WVals);
                    }

                    // Broadcast completion and attempt to deliver final response
                    TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>(); Notify->SetStringField(TEXT("type"), TEXT("automation_event")); Notify->SetStringField(TEXT("event"), TEXT("modify_scs_completed")); Notify->SetStringField(TEXT("requestId"), RequestId); Notify->SetObjectField(TEXT("result"), CompletionResult); SendControlMessage(Notify);

                    // Try to send final automation_response to the original requester
                    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>(); ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath); ResultPayload->SetArrayField(TEXT("operations"), FinalSummaries); ResultPayload->SetBoolField(TEXT("compiled"), bCompile); ResultPayload->SetBoolField(TEXT("saved"), bSave && bSaveResult);
                    if (LocalWarnings.Num() > 0) { TArray<TSharedPtr<FJsonValue>> WVals2; WVals2.Reserve(LocalWarnings.Num()); for (const FString& W : LocalWarnings) WVals2.Add(MakeShared<FJsonValueString>(W)); ResultPayload->SetArrayField(TEXT("warnings"), WVals2); }

                    const FString Message = FString::Printf(TEXT("Processed %d SCS operation(s)."), FinalSummaries.Num()); SendAutomationResponse(RequestingSocket, RequestId, true, Message, ResultPayload, FString());

                    // Release busy flag
                    if (!this->CurrentBusyBlueprintKey.IsEmpty() && GBlueprintBusySet.Contains(this->CurrentBusyBlueprintKey)) { GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey); }
                    this->bCurrentBlueprintBusyMarked = false; this->bCurrentBlueprintBusyScheduled = false; this->CurrentBusyBlueprintKey.Empty();
                });

                return true;
            }

            // blueprint_set_variable_metadata: store metadata into the plugin registry
            if (Lower.Equals(TEXT("blueprint_set_variable_metadata")))
            {
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_set_variable_metadata requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString VarName; LocalPayload->TryGetStringField(TEXT("variableName"), VarName); if (VarName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("variableName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                const TSharedPtr<FJsonValue> MetaVal = LocalPayload->TryGetField(TEXT("metadata"));
                const TSharedPtr<FJsonObject> MetaObjPtr = MetaVal.IsValid() && MetaVal->Type == EJson::Object ? MetaVal->AsObject() : nullptr;
                if (!MetaObjPtr.IsValid()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("metadata object required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
                // naive metadata storage at blueprint->metadata.variableName
                TSharedPtr<FJsonObject> MetadataRoot = Entry->GetObjectField(TEXT("metadata")); MetadataRoot->SetObjectField(VarName, MetaObjPtr);
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("variableName"), VarName); Resp->SetStringField(TEXT("blueprintPath"), Path);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable metadata stored in plugin registry (stub)."), Resp, FString()); return true;
            }

            if (Lower.Equals(TEXT("blueprint_add_construction_script")))
            {
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_construction_script requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString ScriptName; LocalPayload->TryGetStringField(TEXT("scriptName"), ScriptName); if (ScriptName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("scriptName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
                TArray<TSharedPtr<FJsonValue>> Scripts = Entry->HasField(TEXT("constructionScripts")) ? Entry->GetArrayField(TEXT("constructionScripts")) : TArray<TSharedPtr<FJsonValue>>(); Scripts.Add(MakeShared<FJsonValueString>(ScriptName)); Entry->SetArrayField(TEXT("constructionScripts"), Scripts);
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("scriptName"), ScriptName); Resp->SetStringField(TEXT("blueprintPath"), Path);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Construction script recorded in plugin registry (stub)."), Resp, FString()); return true;
            }

            // Add a variable to the blueprint (registry-backed implementation)
            if (Lower.Equals(TEXT("blueprint_add_variable")))
            {
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_variable requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString VarName; LocalPayload->TryGetStringField(TEXT("variableName"), VarName); if (VarName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("variableName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                FString VarType; LocalPayload->TryGetStringField(TEXT("variableType"), VarType);
                const TSharedPtr<FJsonValue> DefaultVal = LocalPayload->TryGetField(TEXT("defaultValue"));
                FString Category; LocalPayload->TryGetStringField(TEXT("category"), Category);
                const bool bReplicated = LocalPayload->HasField(TEXT("isReplicated")) ? LocalPayload->GetBoolField(TEXT("isReplicated")) : false;
                const bool bPublic = LocalPayload->HasField(TEXT("isPublic")) ? LocalPayload->GetBoolField(TEXT("isPublic")) : false;
                TSharedPtr<FJsonObject> Entry2 = EnsureBlueprintEntry(Path);
                TArray<TSharedPtr<FJsonValue>> Vars = Entry2->HasField(TEXT("variables")) ? Entry2->GetArrayField(TEXT("variables")) : TArray<TSharedPtr<FJsonValue>>();
                TSharedPtr<FJsonObject> VarRec = MakeShared<FJsonObject>(); VarRec->SetStringField(TEXT("name"), VarName); if (!VarType.IsEmpty()) VarRec->SetStringField(TEXT("type"), VarType); VarRec->SetBoolField(TEXT("replicated"), bReplicated); VarRec->SetBoolField(TEXT("public"), bPublic); if (!Category.IsEmpty()) VarRec->SetStringField(TEXT("category"), Category); if (DefaultVal.IsValid()) VarRec->SetField(TEXT("defaultValue"), DefaultVal);
                Vars.Add(MakeShared<FJsonValueObject>(VarRec)); Entry2->SetArrayField(TEXT("variables"), Vars);
                TSharedPtr<FJsonObject> RespVar = MakeShared<FJsonObject>(); RespVar->SetStringField(TEXT("variableName"), VarName); RespVar->SetStringField(TEXT("blueprintPath"), Path);
                // Immediate registry ack
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable recorded in plugin registry (stub)."), RespVar, FString());

                // If not fast-mode, attempt to apply the change to the live Blueprint asset
                if (!IsFastMode(LocalPayload))
                {
#if WITH_EDITOR
                    // Prevent concurrent modifications against the same blueprint
                    const FString BusyKeyVar = Path;
                    if (!BusyKeyVar.IsEmpty())
                    {
                        if (GBlueprintBusySet.Contains(BusyKeyVar))
                        {
                            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("blueprint_add_variable: Blueprint %s is busy; skipping live modification."), *BusyKeyVar);
                        }
                        else
                        {
                            GBlueprintBusySet.Add(BusyKeyVar);
                            AsyncTask(ENamedThreads::GameThread, [this, RequestId, Path, VarName, VarType, Category, DefaultVal = DefaultVal, bReplicated, bPublic, RequestingSocket]() mutable {
                                TSharedPtr<FJsonObject> CompletionResult = MakeShared<FJsonObject>();
                                bool bOk = false; TArray<FString> LocalWarnings;
                                FString LocalNormalized; FString LocalLoadError;
                                UBlueprint* LocalBP = LoadBlueprintAsset(Path, LocalNormalized, LocalLoadError);
                                if (!LocalBP)
                                {
                                    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("blueprint_add_variable: failed to load %s: %s"), *Path, *LocalLoadError);
                                    CompletionResult->SetStringField(TEXT("error"), LocalLoadError);
                                    bOk = false;
                                }
                                else
                                {
                                    // Build a simple pin type for common primitives
                                    FEdGraphPinType PinType;
                                    const FString LowerType = VarType.ToLower();
                                    if (LowerType == TEXT("float") || LowerType == TEXT("double")) { PinType.PinCategory = MCP_PC_Float; }
                                    else if (LowerType == TEXT("int") || LowerType == TEXT("integer")) { PinType.PinCategory = MCP_PC_Int; }
                                    else if (LowerType == TEXT("bool") || LowerType == TEXT("boolean")) { PinType.PinCategory = MCP_PC_Boolean; }
                                    else if (LowerType == TEXT("string")) { PinType.PinCategory = MCP_PC_String; }
                                    else if (LowerType == TEXT("name")) { PinType.PinCategory = MCP_PC_Name; }
                                    else
                                    {
                                        // Fallback: treat as object/class name
                                        PinType.PinCategory = MCP_PC_Object;
                                        UClass* FoundClass = nullptr;
                                        FString Trimmed = VarType;
                                        Trimmed.TrimStartAndEndInline();
                                        if (!Trimmed.IsEmpty())
                                        {
                                            // Try direct find/load
                                            FoundClass = FindObject<UClass>(nullptr, *Trimmed);
                                            if (!FoundClass) FoundClass = LoadObject<UClass>(nullptr, *Trimmed);
                                            if (!FoundClass)
                                            {
                                                // Try script prefix guesses
                                                const TArray<FString> Prefixes = { TEXT("/Script/Engine."), TEXT("/Script/CoreUObject.") };
                                                for (const FString& P : Prefixes)
                                                {
                                                    const FString Guess = P + Trimmed;
                                                    FoundClass = FindObject<UClass>(nullptr, *Guess);
                                                    if (!FoundClass) FoundClass = LoadObject<UClass>(nullptr, *Guess);
                                                    if (FoundClass) break;
                                                }
                                            }
                                        }
                                        if (FoundClass) PinType.PinSubCategoryObject = FoundClass;
                                    }

                                    // Modify blueprint and add variable description
                                    LocalBP->Modify();
                                    FBPVariableDescription NewVar;
                                    NewVar.VarName = FName(*VarName);
                                    NewVar.VarGuid = FGuid::NewGuid();
                                    // FriendlyName and Category types differ across engine versions.
                                    // Assign each field based on its actual type to avoid mismatched
                                    // FText/FString/FName assignment errors on different engine builds.
                                    if constexpr (std::is_same_v<decltype(NewVar.FriendlyName), FText>)
                                    {
                                        NewVar.FriendlyName = FText::FromString(VarName);
                                    }
                                    else if constexpr (std::is_same_v<decltype(NewVar.FriendlyName), FName>)
                                    {
                                        NewVar.FriendlyName = FName(*VarName);
                                    }
                                    else
                                    {
                                        // Fall back to string assignment when FriendlyName is an FString
                                        NewVar.FriendlyName = VarName;
                                    }

                                    if constexpr (std::is_same_v<decltype(NewVar.Category), FText>)
                                    {
                                        NewVar.Category = FText::FromString(Category);
                                    }
                                    else if constexpr (std::is_same_v<decltype(NewVar.Category), FName>)
                                    {
                                        NewVar.Category = FName(*Category);
                                    }
                                    else
                                    {
                                        NewVar.Category = Category;
                                    }
                                    NewVar.VarType = PinType;
                                    // Basic flags
                                    NewVar.RepNotifyFunc = NAME_None;

                                    // Check for existing variable with same name
                                    bool bAlready = false;
                                    for (const FBPVariableDescription& V : LocalBP->NewVariables)
                                    {
                                        if (V.VarName == NewVar.VarName) { bAlready = true; break; }
                                    }
                                    if (bAlready)
                                    {
                                        CompletionResult->SetStringField(TEXT("warning"), TEXT("Variable already exists"));
                                        CompletionResult->SetStringField(TEXT("variableName"), VarName);
                                        bOk = true;
                                    }
                                    else
                                    {
                                        LocalBP->NewVariables.Add(NewVar);
                                        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LocalBP);

                                        // Compile and save
                                        FKismetEditorUtilities::CompileBlueprint(LocalBP);
#if WITH_EDITOR
                                        const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(LocalBP);
                                        CompletionResult->SetBoolField(TEXT("saved"), bSaved);
#else
                                        CompletionResult->SetBoolField(TEXT("saved"), false);
#endif
                                        CompletionResult->SetStringField(TEXT("variableName"), VarName);
                                        CompletionResult->SetStringField(TEXT("blueprintPath"), Path);
                                        bOk = true;
                                    }
                                }

                                // Broadcast completion event
                                TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>(); Notify->SetStringField(TEXT("type"), TEXT("automation_event")); Notify->SetStringField(TEXT("event"), TEXT("add_variable_completed")); Notify->SetStringField(TEXT("requestId"), RequestId); Notify->SetObjectField(TEXT("result"), CompletionResult); SendControlMessage(Notify);

                                // Try to send final automation_response to the original requester
                                TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>(); ResultPayload->SetStringField(TEXT("variableName"), VarName); ResultPayload->SetStringField(TEXT("blueprintPath"), Path);
                                SendAutomationResponse(RequestingSocket, RequestId, bOk, bOk ? TEXT("Variable added (editor)") : TEXT("Failed to add variable (editor)"), ResultPayload, bOk ? FString() : TEXT("ADD_VARIABLE_FAILED"));

                                // Release busy flag
                                if (!Path.IsEmpty() && GBlueprintBusySet.Contains(Path)) { GBlueprintBusySet.Remove(Path); }
                            });
                        }
                    }
#endif
                }
                return true;
            }

            // Add an event to the blueprint (registry-backed implementation)
            if (Lower.Equals(TEXT("blueprint_add_event")))
            {
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_event requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString EventType; LocalPayload->TryGetStringField(TEXT("eventType"), EventType);
                FString CustomName; LocalPayload->TryGetStringField(TEXT("customEventName"), CustomName);
                const TArray<TSharedPtr<FJsonValue>>* Params = nullptr; LocalPayload->TryGetArrayField(TEXT("parameters"), Params);
                TSharedPtr<FJsonObject> Entry3 = EnsureBlueprintEntry(Path);
                TArray<TSharedPtr<FJsonValue>> Events = Entry3->HasField(TEXT("events")) ? Entry3->GetArrayField(TEXT("events")) : TArray<TSharedPtr<FJsonValue>>();
                TSharedPtr<FJsonObject> ERec = MakeShared<FJsonObject>(); ERec->SetStringField(TEXT("eventType"), EventType.IsEmpty() ? TEXT("custom") : EventType); if (!CustomName.IsEmpty()) ERec->SetStringField(TEXT("name"), CustomName); if (Params && Params->Num() > 0) ERec->SetArrayField(TEXT("parameters"), *Params);
                Events.Add(MakeShared<FJsonValueObject>(ERec)); Entry3->SetArrayField(TEXT("events"), Events);
                TSharedPtr<FJsonObject> RespEvt = MakeShared<FJsonObject>(); RespEvt->SetStringField(TEXT("blueprintPath"), Path); if (!CustomName.IsEmpty()) RespEvt->SetStringField(TEXT("eventName"), CustomName);
                RespEvt->SetStringField(TEXT("note"), TEXT("Event recorded in plugin registry (stub)."));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event recorded in plugin registry (stub)."), RespEvt, FString());

                if (!IsFastMode(LocalPayload))
                {
#if WITH_EDITOR
                    if (GBlueprintBusySet.Contains(Path)) { UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("blueprint_add_event: Blueprint %s is busy; skipping live modification."), *Path); }
                    else
                    {
                        GBlueprintBusySet.Add(Path);
                        AsyncTask(ENamedThreads::GameThread, [this, RequestId, Path, EventType, CustomName, Params, RequestingSocket]() mutable {
                            TSharedPtr<FJsonObject> CompletionResult = MakeShared<FJsonObject>(); bool bOk = false;
                            FString LocalNormalized; FString LocalLoadError; UBlueprint* LocalBP = LoadBlueprintAsset(Path, LocalNormalized, LocalLoadError);
                            if (!LocalBP) { CompletionResult->SetStringField(TEXT("error"), LocalLoadError); bOk = false; }
                            else
                            {
                                // Add a custom event node to the EventGraph or create one
                                UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(LocalBP);
                                if (!EventGraph) EventGraph = FBlueprintEditorUtils::CreateNewGraph(LocalBP, TEXT("EventGraph"), UEdGraph::StaticClass(), MCP_HAS_EDGRAPH_SCHEMA_K2 ? UEdGraphSchema_K2::StaticClass() : nullptr);
                                    if (EventGraph)
                                    {
#if MCP_HAS_EDGRAPH_SCHEMA_K2
                                        FName NewEventName = CustomName.IsEmpty() ? FName(*FString::Printf(TEXT("Event_%s"), *FGuid::NewGuid().ToString())) : FName(*CustomName);
                                        FBlueprintEditorUtils::AddNewEvent(LocalBP, EventGraph, NewEventName);
                                        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LocalBP);
                                        FKismetEditorUtilities::CompileBlueprint(LocalBP);
#if WITH_EDITOR
                                        const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(LocalBP);
                                        CompletionResult->SetBoolField(TEXT("saved"), bSaved);
#endif
                                        CompletionResult->SetStringField(TEXT("eventName"), NewEventName.ToString());
                                        CompletionResult->SetStringField(TEXT("blueprintPath"), Path);
                                        bOk = true;
#else
                                        // Editor API for adding events unavailable on this build.
                                        CompletionResult->SetStringField(TEXT("warning"), TEXT("AddNewEvent is not supported in this engine build; event recorded in registry only."));
                                        CompletionResult->SetStringField(TEXT("blueprintPath"), Path);
                                        bOk = true;
#endif
                                    }
                                    else
                                    {
                                        CompletionResult->SetStringField(TEXT("error"), TEXT("Failed to locate or create event graph")); bOk = false;
                                    }
                            }

                            TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>(); Notify->SetStringField(TEXT("type"), TEXT("automation_event")); Notify->SetStringField(TEXT("event"), TEXT("add_event_completed")); Notify->SetStringField(TEXT("requestId"), RequestId); Notify->SetObjectField(TEXT("result"), CompletionResult); SendControlMessage(Notify);
                            TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>(); ResultPayload->SetStringField(TEXT("blueprintPath"), Path); if (CompletionResult->HasField(TEXT("eventName"))) ResultPayload->SetStringField(TEXT("eventName"), CompletionResult->GetStringField(TEXT("eventName")));
                            SendAutomationResponse(RequestingSocket, RequestId, bOk, bOk ? TEXT("Event added (editor)") : TEXT("Failed to add event (editor)"), ResultPayload, bOk ? FString() : TEXT("ADD_EVENT_FAILED"));
                            if (!Path.IsEmpty() && GBlueprintBusySet.Contains(Path)) GBlueprintBusySet.Remove(Path);
                        });
                    }
#endif
                }
                return true;
            }

            // Add a function to the blueprint (registry-backed implementation)
            if (Lower.Equals(TEXT("blueprint_add_function")))
            {
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_function requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString FuncName; LocalPayload->TryGetStringField(TEXT("functionName"), FuncName); if (FuncName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("functionName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr; LocalPayload->TryGetArrayField(TEXT("inputs"), Inputs);
                const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr; LocalPayload->TryGetArrayField(TEXT("outputs"), Outputs);
                const bool bIsPublic = LocalPayload->HasField(TEXT("isPublic")) ? LocalPayload->GetBoolField(TEXT("isPublic")) : false;
                TSharedPtr<FJsonObject> Entry4 = EnsureBlueprintEntry(Path);
                TArray<TSharedPtr<FJsonValue>> Funcs = Entry4->HasField(TEXT("functions")) ? Entry4->GetArrayField(TEXT("functions")) : TArray<TSharedPtr<FJsonValue>>();
                TSharedPtr<FJsonObject> FRec = MakeShared<FJsonObject>(); FRec->SetStringField(TEXT("name"), FuncName); FRec->SetBoolField(TEXT("public"), bIsPublic); if (Inputs && Inputs->Num() > 0) FRec->SetArrayField(TEXT("inputs"), *Inputs); if (Outputs && Outputs->Num() > 0) FRec->SetArrayField(TEXT("outputs"), *Outputs);
                Funcs.Add(MakeShared<FJsonValueObject>(FRec)); Entry4->SetArrayField(TEXT("functions"), Funcs);
                TSharedPtr<FJsonObject> RespFunc = MakeShared<FJsonObject>(); RespFunc->SetStringField(TEXT("functionName"), FuncName); RespFunc->SetStringField(TEXT("blueprintPath"), Path);
                RespFunc->SetStringField(TEXT("note"), TEXT("Function recorded in plugin registry (stub)."));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Function recorded in plugin registry (stub)."), RespFunc, FString());

                if (!IsFastMode(LocalPayload))
                {
#if WITH_EDITOR
                    if (GBlueprintBusySet.Contains(Path)) { UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("blueprint_add_function: Blueprint %s is busy; skipping live modification."), *Path); }
                    else
                    {
                        GBlueprintBusySet.Add(Path);
                        AsyncTask(ENamedThreads::GameThread, [this, RequestId, Path, FuncName, Inputs = Inputs ? *Inputs : TArray<TSharedPtr<FJsonValue>>(), Outputs = Outputs ? *Outputs : TArray<TSharedPtr<FJsonValue>>(), bIsPublic, RequestingSocket]() mutable {
                            TSharedPtr<FJsonObject> CompletionResult = MakeShared<FJsonObject>(); bool bOk = false;
                            FString LocalNormalized; FString LocalLoadError; UBlueprint* LocalBP = LoadBlueprintAsset(Path, LocalNormalized, LocalLoadError);
                            if (!LocalBP) { CompletionResult->SetStringField(TEXT("error"), LocalLoadError); bOk = false; }
                            else
                            {
                                // Create UEdGraph for the function and add inputs/outputs
                                UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(LocalBP, FName(*FuncName), UEdGraph::StaticClass(), MCP_HAS_EDGRAPH_SCHEMA_K2 ? UEdGraphSchema_K2::StaticClass() : nullptr);
                                if (NewGraph)
                                {
                                    // Define a callable ApplyPins in the outer scope. If the engine
                                    // provides the function graph editors, we'll populate this
                                    // with a real implementation; otherwise it remains a no-op
                                    // that returns a warning but safely exists so callers can
                                    // unconditionally invoke it.
                                    std::function<void(const TArray<TSharedPtr<FJsonValue>>&, bool)> ApplyPins =
                                        [&](const TArray<TSharedPtr<FJsonValue>>& /*Arr*/, bool /*bIsInput*/)
                                    {
#if MCP_HAS_EDGRAPH_SCHEMA_K2
                                        // No-op default here; real implementation below will replace it
#else
                                        // Editor function graph APIs are not available in this build.
                                        CompletionResult->SetStringField(TEXT("warning"), TEXT("Function creation helpers are not available in this engine build; function recorded in registry only."));
#endif
                                    };

#if MCP_HAS_EDGRAPH_SCHEMA_K2
                                    // Use nullptr for SignatureType when the engine expects a pointer type
                                    FBlueprintEditorUtils::AddFunctionGraph(LocalBP, NewGraph, /*bIsCosmetic=*/false, static_cast<UEdGraph*>(nullptr));

                                    // Replace ApplyPins with a working implementation when editor helpers are available
                                    ApplyPins = [&](const TArray<TSharedPtr<FJsonValue>>& Arr, bool bIsInput) {
                                        for (const TSharedPtr<FJsonValue>& P : Arr)
                                        {
                                            if (!P.IsValid() || P->Type != EJson::Object) continue;
                                            const TSharedPtr<FJsonObject> Obj = P->AsObject(); FString ParamName; Obj->TryGetStringField(TEXT("name"), ParamName); FString ParamType; Obj->TryGetStringField(TEXT("type"), ParamType);
                                            if (ParamName.IsEmpty()) continue;
                                            FEdGraphPinType PinType;
                                            const FString LowerType = ParamType.ToLower();
                                            if (LowerType == TEXT("float") || LowerType == TEXT("double")) PinType.PinCategory = MCP_PC_Float;
                                            else if (LowerType == TEXT("int") || LowerType == TEXT("integer")) PinType.PinCategory = MCP_PC_Int;
                                            else if (LowerType == TEXT("bool")) PinType.PinCategory = MCP_PC_Boolean;
                                            else if (LowerType == TEXT("string")) PinType.PinCategory = MCP_PC_String;
                                            else PinType.PinCategory = MCP_PC_Wildcard;
                                            FBlueprintEditorUtils::AddFunctionParameter(NewGraph, FName(*ParamName), PinType, bIsInput);
                                        }
                                    };
#endif

                                    ApplyPins(Inputs, true);
                                    ApplyPins(Outputs, false);

                                    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LocalBP);
                                    FKismetEditorUtilities::CompileBlueprint(LocalBP);
#if WITH_EDITOR
                                    const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(LocalBP);
                                    CompletionResult->SetBoolField(TEXT("saved"), bSaved);
#endif
                                    CompletionResult->SetStringField(TEXT("functionName"), FuncName);
                                    CompletionResult->SetStringField(TEXT("blueprintPath"), Path);
                                    bOk = true;
                                }
                                else { CompletionResult->SetStringField(TEXT("error"), TEXT("Failed to create function graph")); bOk = false; }
                            }

                            TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>(); Notify->SetStringField(TEXT("type"), TEXT("automation_event")); Notify->SetStringField(TEXT("event"), TEXT("add_function_completed")); Notify->SetStringField(TEXT("requestId"), RequestId); Notify->SetObjectField(TEXT("result"), CompletionResult); SendControlMessage(Notify);
                            TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>(); ResultPayload->SetStringField(TEXT("functionName"), FuncName); ResultPayload->SetStringField(TEXT("blueprintPath"), Path);
                            SendAutomationResponse(RequestingSocket, RequestId, bOk, bOk ? TEXT("Function added (editor)") : TEXT("Failed to add function (editor)"), ResultPayload, bOk ? FString() : TEXT("ADD_FUNCTION_FAILED"));
                            if (!Path.IsEmpty() && GBlueprintBusySet.Contains(Path)) GBlueprintBusySet.Remove(Path);
                        });
                    }
#endif
                }
                return true;
            }

            if (Lower.Equals(TEXT("blueprint_set_default")))
            {
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_set_default requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString PropertyName; LocalPayload->TryGetStringField(TEXT("propertyName"), PropertyName); if (PropertyName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("propertyName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                const TSharedPtr<FJsonValue> Value = LocalPayload->TryGetField(TEXT("value")); if (!Value.IsValid()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("value required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path); TSharedPtr<FJsonObject> Defaults = Entry->GetObjectField(TEXT("defaults")); Defaults->SetField(PropertyName, Value);
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("blueprintPath"), Path); Resp->SetStringField(TEXT("propertyName"), PropertyName);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint default recorded in plugin registry (stub)."), Resp, FString()); return true;
            }

                                    if (Lower.Equals(TEXT("blueprint_probe_subobject_handle")))
                                    {
            #if WITH_EDITOR
                                            FString ComponentClass; LocalPayload->TryGetStringField(TEXT("componentClass"), ComponentClass);
                                            if (ComponentClass.IsEmpty()) ComponentClass = TEXT("StaticMeshComponent");

            #if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
                                            // Native probe using USubobjectDataSubsystem
                                            AsyncTask(ENamedThreads::GameThread, [this, RequestId, ComponentClass, RequestingSocket]() {
                                                    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                                                    ResultObj->SetStringField(TEXT("componentClass"), ComponentClass);
                                                    ResultObj->SetBoolField(TEXT("subsystemAvailable"), false);
                                                    ResultObj->SetBoolField(TEXT("success"), false);

                                                    // Obtain subclass UClass for the requested component
                                                    UClass* ComponentUClass = nullptr;
                                                    if (!ComponentClass.IsEmpty())
                                                    {
                                                            ComponentUClass = FindObject<UClass>(nullptr, *ComponentClass);
                                                            if (!ComponentUClass) ComponentUClass = StaticLoadClass(UActorComponent::StaticClass(), nullptr, *ComponentClass);
                                                            if (!ComponentUClass)
                                                            {
                                                                    // Try common script prefixes
                                                                    const TArray<FString> Prefixes = { TEXT("/Script/Engine."), TEXT("/Script/CoreUObject.") };
                                                                    for (const FString& P : Prefixes)
                                                                    {
                                                                            const FString Guess = P + ComponentClass;
                                                                            ComponentUClass = FindObject<UClass>(nullptr, *Guess);
                                                                            if (!ComponentUClass) ComponentUClass = StaticLoadClass(UActorComponent::StaticClass(), nullptr, *Guess);
                                                                            if (ComponentUClass) break;
                                                                    }
                                                            }
                                                    }

                                                    // Try to get the subsystem
                                                    USubobjectDataSubsystem* Subsystem = nullptr;
                                                    if (GEngine) Subsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
                                                    if (!Subsystem)
                                                    {
                                                            ResultObj->SetBoolField(TEXT("subsystemAvailable"), false);
                                                            ResultObj->SetStringField(TEXT("error"), TEXT("SubobjectDataSubsystem not available"));
                                                            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("SubobjectDataSubsystem not available"), ResultObj, TEXT("PROBE_FAILED"));
                                                            return;
                                                    }
                                                    ResultObj->SetBoolField(TEXT("subsystemAvailable"), true);

                            // Create a transient blueprint asset for the probe and gather handles
                            const FString ProbePath = TEXT("/Game/Temp/MCPProbe");
                            const FString ProbeName = FString::Printf(TEXT("MCP_Probe_BP_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
                            UBlueprint* CreatedBP = nullptr;
                            {
                                UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
                                FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
                                UObject* NewObj = AssetToolsModule.Get().CreateAsset(ProbeName, ProbePath, UBlueprint::StaticClass(), Factory);
                                if (!NewObj)
                                {
                                    ResultObj->SetStringField(TEXT("error"), TEXT("Failed to create probe blueprint asset"));
                                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create probe blueprint"), ResultObj, TEXT("PROBE_CREATE_FAILED"));
                                    return;
                                }
                                CreatedBP = Cast<UBlueprint>(NewObj);
                                if (!CreatedBP)
                                {
                                    ResultObj->SetStringField(TEXT("error"), TEXT("Created asset is not a Blueprint"));
                                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Probe asset created was not a Blueprint"), ResultObj, TEXT("PROBE_CREATE_FAILED"));
                                    return;
                                }
                                // Register asset with the registry and attempt to save
                                FAssetRegistryModule& Arm = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
                                Arm.Get().AssetCreated(CreatedBP);
#if WITH_EDITOR
                                UEditorAssetLibrary::SaveLoadedAsset(CreatedBP);
#endif
                            }

                            // Attempt to gather handles using the subsystem's C++ API
                            TArray<FSubobjectDataHandle> GatheredHandles;
                            if (Subsystem)
                            {
                                // Most UE 5.6+ builds expose this helper as K2_GatherSubobjectDataForBlueprint
                                Subsystem->K2_GatherSubobjectDataForBlueprint(CreatedBP, GatheredHandles);
                            }

                            // Convert handle set into a JSON-friendly summary
                            TArray<TSharedPtr<FJsonValue>> HandleJsonArr;
                            if (GatheredHandles.Num() > 0)
                            {
                                const UScriptStruct* HandleStruct = FSubobjectDataHandle::StaticStruct();
                                for (int32 i = 0; i < GatheredHandles.Num(); ++i)
                                {
                                    const FSubobjectDataHandle& H = GatheredHandles[i];
                                    if (HandleStruct)
                                    {
                                        // Provide a concise textual summary: struct type + address
                                        const FString Repr = FString::Printf(TEXT("%s@%p"), *HandleStruct->GetName(), (void*)&H);
                                        HandleJsonArr.Add(MakeShared<FJsonValueString>(Repr));
                                    }
                                    else
                                    {
                                        HandleJsonArr.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("<subobject_handle_%d>"), i)));
                                    }
                                }
                            }
                            ResultObj->SetArrayField(TEXT("gatheredHandles"), HandleJsonArr);
                            ResultObj->SetBoolField(TEXT("subsystemAvailable"), Subsystem != nullptr);
                            ResultObj->SetBoolField(TEXT("success"), true);

                            // Cleanup the transient probe asset
#if WITH_EDITOR
                            if (CreatedBP)
                            {
                                // DeleteLoadedAsset expects a UObject*; pass the created blueprint directly
                                UEditorAssetLibrary::DeleteLoadedAsset(CreatedBP);
                            }
#endif

                            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Native probe completed"), ResultObj, FString());
                                            });
                                            return true;
            #else
                                            // Native subsystem not available — fall back to earlier Python probe
                                            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("SubobjectDataSubsystem not available in this engine build; native probe not possible."), nullptr, TEXT("NOT_IMPLEMENTED"));
                                            return true;
            #endif // MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
            #else
                                            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint probe requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
                                            return true;
            #endif // WITH_EDITOR
                                    }

            // Register primary creator
            GBlueprintCreateInflight.Add(CreateKey, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>());
            GBlueprintCreateInflightTs.Add(CreateKey, Now);
            GBlueprintCreateInflight[CreateKey].Add(TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId, RequestingSocket));

            // Quick cache and registry entry
            const double CacheNow = FPlatformTime::Seconds();
            const FString CandidateNormalized = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
            const FString CandidateAssetPath = FString::Printf(TEXT("%s.%s"), *CandidateNormalized, *Name);
            GBlueprintExistCacheTs.Add(CandidateNormalized, CacheNow);
            GBlueprintExistCacheNormalized.Add(CandidateNormalized, CandidateNormalized);
            FString CandidateKey = FString::Printf(TEXT("%s/%s"), *SavePath, *Name);
            GBlueprintExistCacheTs.Add(CandidateKey, CacheNow);
            GBlueprintExistCacheNormalized.Add(CandidateKey, CandidateNormalized);
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("blueprintPath"), CandidateNormalized); Entry->SetArrayField(TEXT("variables"), TArray<TSharedPtr<FJsonValue>>() ); Entry->SetArrayField(TEXT("constructionScripts"), TArray<TSharedPtr<FJsonValue>>() ); Entry->SetObjectField(TEXT("defaults"), MakeShared<FJsonObject>()); Entry->SetObjectField(TEXT("metadata"), MakeShared<FJsonObject>());
            GBlueprintRegistry.Add(CandidateNormalized, Entry);

            // Send immediate fast success to subscribers
            TSharedPtr<FJsonObject> FastPayload = MakeShared<FJsonObject>(); FastPayload->SetStringField(TEXT("path"), CandidateNormalized); FastPayload->SetStringField(TEXT("assetPath"), CandidateAssetPath);
            if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs2 = GBlueprintCreateInflight.Find(CreateKey))
            {
                for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>& Pair : *Subs2) { SendAutomationResponse(Pair.Value, Pair.Key, true, TEXT("Blueprint created (queued)"), FastPayload, FString()); }
            }
        }

#if WITH_EDITOR
        // Perform the real creation (editor only)
        UBlueprint* CreatedBlueprint = nullptr;
        FString CreatedNormalizedPath;
        FString CreationError;

        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        // Resolve parent and set Factory->ParentClass similar to previous logic
        UClass* ResolvedParent = nullptr;
        if (!ParentClassSpec.IsEmpty())
        {
            if (ParentClassSpec.StartsWith(TEXT("/Script/"))) { ResolvedParent = LoadClass<UObject>(nullptr, *ParentClassSpec); }
            else
            {
                // Prefer non-deprecated lookup patterns: try FindObject with null
                // outer, then attempt to load the class path, and finally fall
                // back to scanning loaded classes by name.
                ResolvedParent = FindObject<UClass>(nullptr, *ParentClassSpec);
                if (!ResolvedParent)
                {
                    ResolvedParent = StaticLoadClass(UObject::StaticClass(), nullptr, *ParentClassSpec);
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
            if (LowerType == TEXT("actor")) ResolvedParent = AActor::StaticClass();
            else if (LowerType == TEXT("pawn")) ResolvedParent = APawn::StaticClass();
            else if (LowerType == TEXT("character")) ResolvedParent = ACharacter::StaticClass();
            // else leave null
        }
        if (ResolvedParent) Factory->ParentClass = ResolvedParent; else Factory->ParentClass = AActor::StaticClass();

        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        UObject* NewObj = AssetToolsModule.Get().CreateAsset(Name, SavePath, UBlueprint::StaticClass(), Factory);
        if (!NewObj)
        {
            CreationError = FString::Printf(TEXT("AssetTools::CreateAsset returned null for %s in %s"), *Name, *SavePath);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("blueprint_create RequestId=%s: %s - attempting native fallback."), *RequestId, *CreationError);

#if WITH_EDITOR
            // Try a direct native creation path via FKismetEditorUtilities as a fallback
            UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *SavePath, *Name));
            if (Package)
            {
                // Use Kismet utilities to create a blueprint asset in the package
                UBlueprint* KismetBP = FKismetEditorUtilities::CreateBlueprint(ResolvedParent ? ResolvedParent : AActor::StaticClass(), Package, FName(*Name), EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
                if (KismetBP)
                {
                    NewObj = KismetBP;
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_create RequestId=%s: created via FKismetEditorUtilities"), *RequestId);
                }
            }
#endif
        }
        else
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("CreateAsset returned object: name=%s path=%s class=%s"), *NewObj->GetName(), *NewObj->GetPathName(), *NewObj->GetClass()->GetName());
        }

        CreatedBlueprint = Cast<UBlueprint>(NewObj);
        if (!CreatedBlueprint) { CreationError = FString::Printf(TEXT("Created asset is not a Blueprint: %s"), NewObj ? *NewObj->GetPathName() : TEXT("<null>")); SendAutomationResponse(RequestingSocket, RequestId, false, CreationError, nullptr, TEXT("CREATE_FAILED")); return true; }

        CreatedNormalizedPath = CreatedBlueprint->GetPathName(); if (CreatedNormalizedPath.Contains(TEXT("."))) CreatedNormalizedPath = CreatedNormalizedPath.Left(CreatedNormalizedPath.Find(TEXT(".")));
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")); AssetRegistryModule.AssetCreated(CreatedBlueprint);
        const double Now = FPlatformTime::Seconds(); if (!CreatedNormalizedPath.IsEmpty()) { GBlueprintExistCacheTs.Add(CreatedNormalizedPath, Now); GBlueprintExistCacheNormalized.Add(CreatedNormalizedPath, CreatedNormalizedPath); FString CandidateKey = FString::Printf(TEXT("%s/%s"), *SavePath, *Name); if (!CandidateKey.IsEmpty()) GBlueprintExistCacheNormalized.Add(CandidateKey, CreatedNormalizedPath); TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>(); Entry->SetStringField(TEXT("blueprintPath"), CreatedNormalizedPath); Entry->SetArrayField(TEXT("variables"), TArray<TSharedPtr<FJsonValue>>() ); Entry->SetArrayField(TEXT("constructionScripts"), TArray<TSharedPtr<FJsonValue>>() ); Entry->SetObjectField(TEXT("defaults"), MakeShared<FJsonObject>()); Entry->SetObjectField(TEXT("metadata"), MakeShared<FJsonObject>()); GBlueprintRegistry.Add(CreatedNormalizedPath, Entry); }

        // Notify subscribers
        TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>(); ResultPayload->SetStringField(TEXT("path"), CreatedNormalizedPath); ResultPayload->SetStringField(TEXT("assetPath"), CreatedBlueprint->GetPathName());
        FScopeLock Lock(&GBlueprintCreateMutex);
        if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs = GBlueprintCreateInflight.Find(CreateKey))
        {
            for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>& Pair : *Subs) { SendAutomationResponse(Pair.Value, Pair.Key, true, TEXT("Blueprint created"), ResultPayload, FString()); }
            GBlueprintCreateInflight.Remove(CreateKey); GBlueprintCreateInflightTs.Remove(CreateKey);
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_create RequestId=%s completed (coalesced)."), *RequestId);
        }
        else { SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint created"), ResultPayload, FString()); }

        // Defer save tasks
        TWeakObjectPtr<UBlueprint> WeakCreatedBp = CreatedBlueprint;
        AsyncTask(ENamedThreads::GameThread, [WeakCreatedBp]() {
            if (!WeakCreatedBp.IsValid()) return;
            UBlueprint* BP = WeakCreatedBp.Get();
#if WITH_EDITOR
            UEditorAssetLibrary::SaveLoadedAsset(BP);
#endif
        });

        return true;
#else
        // Not an editor build: respond with not implemented
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint creation requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // Other blueprint_* actions (modify_scs, compile, add_variable, add_function, etc.)
    // For simplicity, unhandled blueprint actions return NOT_IMPLEMENTED so
    // the server may fall back to Python helpers if available.
    SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Blueprint action not implemented by plugin: %s"), *Action), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
}
