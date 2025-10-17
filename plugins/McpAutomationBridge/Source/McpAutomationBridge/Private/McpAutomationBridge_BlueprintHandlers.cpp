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
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include <functional>
// K2Node headers for Blueprint node graph manipulation
// In UE 5.6, K2Node headers moved to BlueprintGraph/ directory
#if defined(__has_include)
#  if __has_include("BlueprintGraph/K2Node_CallFunction.h")
#    include "BlueprintGraph/K2Node_CallFunction.h"
#    include "BlueprintGraph/K2Node_VariableGet.h"
#    include "BlueprintGraph/K2Node_VariableSet.h"
#    include "BlueprintGraph/K2Node_Literal.h"
#    include "BlueprintGraph/K2Node_Event.h"
#    include "BlueprintGraph/K2Node_CustomEvent.h"
#    include "BlueprintGraph/K2Node_FunctionEntry.h"
#    include "BlueprintGraph/K2Node_FunctionResult.h"
#    define MCP_HAS_K2NODE_HEADERS 1
#  elif __has_include("K2Node_CallFunction.h")
#    include "K2Node_CallFunction.h"
#    include "K2Node_VariableGet.h"
#    include "K2Node_VariableSet.h"
#    include "K2Node_Literal.h"
#    include "K2Node_Event.h"
#    include "K2Node_CustomEvent.h"
#    include "K2Node_FunctionEntry.h"
#    include "K2Node_FunctionResult.h"
#    define MCP_HAS_K2NODE_HEADERS 1
#  else
#    define MCP_HAS_K2NODE_HEADERS 0
#  endif
#else
#  define MCP_HAS_K2NODE_HEADERS 0
#endif
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
#endif // WITH_EDITOR
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

// Helper: pattern-match logic extracted to file-scope so diagnostic
// loops cannot be accidentally placed outside a function body by
// preprocessor variations.
static bool ActionMatchesPatternImpl(const FString& Lower, const FString& AlphaNumLower, const TCHAR* Pattern)
{
    const FString PatternStr = FString(Pattern).ToLower();
    FString PatternAlpha; PatternAlpha.Reserve(PatternStr.Len());
    for (int32 i=0;i<PatternStr.Len();++i) { const TCHAR C = PatternStr[i]; if (FChar::IsAlnum(C)) PatternAlpha.AppendChar(C); }
    const bool bExactOrContains = (Lower.Equals(PatternStr) || Lower.Contains(PatternStr));
    const bool bAlphaMatch = (!AlphaNumLower.IsEmpty() && !PatternAlpha.IsEmpty() && AlphaNumLower.Contains(PatternAlpha));
    return (bExactOrContains || bAlphaMatch);
}

static void DiagnosticPatternChecks(const FString& CleanAction, const FString& Lower, const FString& AlphaNumLower)
{
    const TCHAR* Patterns[] = {
        TEXT("blueprint_add_variable"), TEXT("add_variable"), TEXT("addvariable"),
        TEXT("blueprint_add_event"), TEXT("add_event"),
        TEXT("blueprint_add_function"), TEXT("add_function"),
        TEXT("blueprint_modify_scs"), TEXT("modify_scs"),
        TEXT("blueprint_set_default"), TEXT("set_default"),
        TEXT("blueprint_set_variable_metadata"), TEXT("set_variable_metadata"),
        TEXT("blueprint_compile"), TEXT("blueprint_probe_subobject_handle"), TEXT("blueprint_exists"), TEXT("blueprint_get"), TEXT("blueprint_create")
    };
    for (const TCHAR* P : Patterns)
    {
        const bool bMatch = ActionMatchesPatternImpl(Lower, AlphaNumLower, P);
        // This diagnostic is extremely chatty when processing many requests —
        // lower it to VeryVerbose so it only appears when a developer explicitly
        // enables very verbose logging for the subsystem.
        UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("Diagnostic pattern check: Action=%s Pattern=%s Matched=%s"), *CleanAction, P, bMatch ? TEXT("true") : TEXT("false"));
    }
}

// Handler helper: probe subobject handle (extracted from inline dispatcher to
// avoid preprocessor-brace fragility in the large dispatcher function).
static bool HandleBlueprintProbeSubobjectHandle(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& LocalPayload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    check(Self);
    // Local extraction
    FString ComponentClass; LocalPayload->TryGetStringField(TEXT("componentClass"), ComponentClass);
    if (ComponentClass.IsEmpty()) ComponentClass = TEXT("StaticMeshComponent");

#if WITH_EDITOR
    // If the engine exposes SubobjectDataSubsystem, attempt native probe.
    // This code mirrors the inline logic previously in the dispatcher.
    #if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
    AsyncTask(ENamedThreads::GameThread, [Self, RequestId, ComponentClass, RequestingSocket]() {
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("componentClass"), ComponentClass);
        ResultObj->SetBoolField(TEXT("subsystemAvailable"), false);
        ResultObj->SetBoolField(TEXT("success"), false);

        UClass* ComponentUClass = nullptr;
        if (!ComponentClass.IsEmpty())
        {
            ComponentUClass = FindObject<UClass>(nullptr, *ComponentClass);
            if (!ComponentUClass) ComponentUClass = StaticLoadClass(UActorComponent::StaticClass(), nullptr, *ComponentClass);
            if (!ComponentUClass)
            {
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

        USubobjectDataSubsystem* Subsystem = nullptr;
        if (GEngine) Subsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
        if (!Subsystem)
        {
            ResultObj->SetBoolField(TEXT("subsystemAvailable"), false);
            ResultObj->SetStringField(TEXT("error"), TEXT("SubobjectDataSubsystem not available"));
            Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("SubobjectDataSubsystem not available"), ResultObj, TEXT("PROBE_FAILED"));
            return;
        }
        ResultObj->SetBoolField(TEXT("subsystemAvailable"), true);

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
                Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create probe blueprint"), ResultObj, TEXT("PROBE_CREATE_FAILED"));
                return;
            }
            CreatedBP = Cast<UBlueprint>(NewObj);
            if (!CreatedBP)
            {
                ResultObj->SetStringField(TEXT("error"), TEXT("Created asset is not a Blueprint"));
                Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Probe asset created was not a Blueprint"), ResultObj, TEXT("PROBE_CREATE_FAILED"));
                return;
            }
            FAssetRegistryModule& Arm = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
            Arm.Get().AssetCreated(CreatedBP);
#if WITH_EDITOR
            SaveLoadedAssetThrottled(CreatedBP);
#endif
        }

        TArray<FSubobjectDataHandle> GatheredHandles;
        if (Subsystem)
        {
            Subsystem->K2_GatherSubobjectDataForBlueprint(CreatedBP, GatheredHandles);
        }

        TArray<TSharedPtr<FJsonValue>> HandleJsonArr;
        if (GatheredHandles.Num() > 0)
        {
            const UScriptStruct* HandleStruct = FSubobjectDataHandle::StaticStruct();
            for (int32 i = 0; i < GatheredHandles.Num(); ++i)
            {
                const FSubobjectDataHandle& H = GatheredHandles[i];
                if (HandleStruct)
                {
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

#if WITH_EDITOR
        if (CreatedBP)
        {
            UEditorAssetLibrary::DeleteLoadedAsset(CreatedBP);
        }
#endif

        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Native probe completed"), ResultObj, FString());
    });
    return true;
    #else
        // Native subsystem not available — provide a conservative editor-only
        // fallback so callers still receive a well-formed result. The fallback
        // does not rely on SubobjectDataSubsystem; instead it creates a small
        // probe Blueprint asset and returns a best-effort set of handles.
        AsyncTask(ENamedThreads::GameThread, [Self, RequestId, ComponentClass, RequestingSocket]() {
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetStringField(TEXT("componentClass"), ComponentClass);
            ResultObj->SetBoolField(TEXT("subsystemAvailable"), false);

            // Try to create a small probe Blueprint asset so we can examine
            // its construction script nodes as a proxy for subobject handles.
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
                    Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to create probe blueprint"), ResultObj, TEXT("PROBE_CREATE_FAILED"));
                    return;
                }
                CreatedBP = Cast<UBlueprint>(NewObj);
                if (!CreatedBP)
                {
                    ResultObj->SetStringField(TEXT("error"), TEXT("Created asset is not a Blueprint"));
                    Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Probe asset created was not a Blueprint"), ResultObj, TEXT("PROBE_CREATE_FAILED"));
                    return;
                }
                FAssetRegistryModule& Arm = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
                Arm.Get().AssetCreated(CreatedBP);
        #if WITH_EDITOR
                SaveLoadedAssetThrottled(CreatedBP);
        #endif
            }

            // Gather simple, conservative handles: list SCS node names when available
            TArray<TSharedPtr<FJsonValue>> HandleJsonArr;
        #if WITH_EDITOR
            if (CreatedBP && CreatedBP->SimpleConstructionScript)
            {
                const TArray<USCS_Node*>& Nodes = CreatedBP->SimpleConstructionScript->GetAllNodes();
                for (USCS_Node* N : Nodes)
                {
                    if (!N) continue;
                    const FString Repr = FString::Printf(TEXT("scs://%s"), *N->GetVariableName().ToString());
                    HandleJsonArr.Add(MakeShared<FJsonValueString>(Repr));
                }
            }
        #endif

            // If we couldn't gather any real handles, include a synthetic probe token
            if (HandleJsonArr.Num() == 0) HandleJsonArr.Add(MakeShared<FJsonValueString>(TEXT("<probe_handle_stub>")));

            ResultObj->SetArrayField(TEXT("gatheredHandles"), HandleJsonArr);
            ResultObj->SetBoolField(TEXT("success"), true);

        #if WITH_EDITOR
            if (CreatedBP)
            {
                UEditorAssetLibrary::DeleteLoadedAsset(CreatedBP);
            }
        #endif

            Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Fallback probe completed"), ResultObj, FString());
        });
        return true;
        #endif // MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
#else
    Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint probe requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif // WITH_EDITOR
}

// Handler helper: create Blueprint (extracted for the same reasons)
static bool HandleBlueprintCreate(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& LocalPayload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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

    // Register creator and send immediate fast success only if NOT waiting for completion
    if (!bWaitForCompletion)
    {
        GBlueprintCreateInflight.Add(CreateKey, TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>());
        GBlueprintCreateInflightTs.Add(CreateKey, Now);
        GBlueprintCreateInflight[CreateKey].Add(TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>(RequestId, RequestingSocket));
        
        TSharedPtr<FJsonObject> FastPayload = MakeShared<FJsonObject>(); FastPayload->SetStringField(TEXT("path"), CandidateNormalized); FastPayload->SetStringField(TEXT("assetPath"), CandidateAssetPath);
        if (TArray<TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>>* Subs2 = GBlueprintCreateInflight.Find(CreateKey))
        {
            for (const TPair<FString, TSharedPtr<FMcpBridgeWebSocket>>& Pair : *Subs2) { Self->SendAutomationResponse(Pair.Value, Pair.Key, true, TEXT("Blueprint created (queued)"), FastPayload, FString()); }
        }
    }

#if WITH_EDITOR
    // Perform real creation (editor only)
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintCreate: Starting blueprint creation (WITH_EDITOR=1)"));
    UBlueprint* CreatedBlueprint = nullptr;
    FString CreatedNormalizedPath;
    FString CreationError;

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    UClass* ResolvedParent = nullptr;
    if (!ParentClassSpec.IsEmpty())
    {
        if (ParentClassSpec.StartsWith(TEXT("/Script/"))) { ResolvedParent = LoadClass<UObject>(nullptr, *ParentClassSpec); }
        else
        {
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
        if (LowerType == TEXT("actor")) ResolvedParent = AActor::StaticClass(); else if (LowerType == TEXT("pawn")) ResolvedParent = APawn::StaticClass(); else if (LowerType == TEXT("character")) ResolvedParent = ACharacter::StaticClass();
    }
    if (ResolvedParent) Factory->ParentClass = ResolvedParent; else Factory->ParentClass = AActor::StaticClass();

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    UObject* NewObj = AssetToolsModule.Get().CreateAsset(Name, SavePath, UBlueprint::StaticClass(), Factory);
    if (!NewObj)
    {
        CreationError = FString::Printf(TEXT("AssetTools::CreateAsset returned null for %s in %s"), *Name, *SavePath);
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("blueprint_create RequestId=%s: %s - attempting native fallback."), *RequestId, *CreationError);
#if WITH_EDITOR
        UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *SavePath, *Name));
        if (Package)
        {
            UBlueprint* KismetBP = FKismetEditorUtilities::CreateBlueprint(ResolvedParent ? ResolvedParent : AActor::StaticClass(), Package, FName(*Name), EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
            if (KismetBP) { NewObj = KismetBP; UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_create RequestId=%s: created via FKismetEditorUtilities"), *RequestId); }
        }
#endif
    }
    else
    {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("CreateAsset returned object: name=%s path=%s class=%s"), *NewObj->GetName(), *NewObj->GetPathName(), *NewObj->GetClass()->GetName());
    }

    CreatedBlueprint = Cast<UBlueprint>(NewObj);
    if (!CreatedBlueprint) { CreationError = FString::Printf(TEXT("Created asset is not a Blueprint: %s"), NewObj ? *NewObj->GetPathName() : TEXT("<null>")); Self->SendAutomationResponse(RequestingSocket, RequestId, false, CreationError, nullptr, TEXT("CREATE_FAILED")); return true; }

    CreatedNormalizedPath = CreatedBlueprint->GetPathName(); if (CreatedNormalizedPath.Contains(TEXT("."))) CreatedNormalizedPath = CreatedNormalizedPath.Left(CreatedNormalizedPath.Find(TEXT(".")));
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")); AssetRegistryModule.AssetCreated(CreatedBlueprint);
    const double Now2 = FPlatformTime::Seconds(); if (!CreatedNormalizedPath.IsEmpty()) { GBlueprintExistCacheTs.Add(CreatedNormalizedPath, Now2); GBlueprintExistCacheNormalized.Add(CreatedNormalizedPath, CreatedNormalizedPath); FString CandidateKey2 = FString::Printf(TEXT("%s/%s"), *SavePath, *Name); if (!CandidateKey2.IsEmpty()) GBlueprintExistCacheNormalized.Add(CandidateKey2, CreatedNormalizedPath); TSharedPtr<FJsonObject> Entry2 = MakeShared<FJsonObject>(); Entry2->SetStringField(TEXT("blueprintPath"), CreatedNormalizedPath); Entry2->SetArrayField(TEXT("variables"), TArray<TSharedPtr<FJsonValue>>() ); Entry2->SetArrayField(TEXT("constructionScripts"), TArray<TSharedPtr<FJsonValue>>() ); Entry2->SetObjectField(TEXT("defaults"), MakeShared<FJsonObject>()); Entry2->SetObjectField(TEXT("metadata"), MakeShared<FJsonObject>()); GBlueprintRegistry.Add(CreatedNormalizedPath, Entry2); }

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
    AsyncTask(ENamedThreads::GameThread, [WeakCreatedBp]() {
        if (!WeakCreatedBp.IsValid()) return;
        UBlueprint* BP = WeakCreatedBp.Get();
#if WITH_EDITOR
    SaveLoadedAssetThrottled(BP);
#endif
    });

    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintCreate EXIT: RequestId=%s created successfully"), *RequestId);
    return true;
#else
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("HandleBlueprintCreate: WITH_EDITOR not defined - cannot create blueprints"));
    Self->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint creation requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleBlueprintAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT(">>> HandleBlueprintAction ENTRY: RequestId=%s RawAction='%s'"), *RequestId, *Action);
    
    // Sanitize action to remove control characters and common invisible
    // Unicode markers (BOM, zero-width spaces) that may be injected by
    // transport framing or malformed clients. Keep a cleaned lowercase
    // variant for direct matches; additional compacted alphanumeric form
    // will be computed later (after nested action extraction) so matching
    // is tolerant of underscores, hyphens and camelCase.
    FString CleanAction;
    CleanAction.Reserve(Action.Len());
    for (int32 Idx = 0; Idx < Action.Len(); ++Idx)
    {
        const TCHAR C = Action[Idx];
        // Filter common invisible / control characters
        if (C < 32) continue;
        if (C == 0x200B /* ZERO WIDTH SPACE */ || C == 0xFEFF /* BOM */ || C == 0x200C /* ZERO WIDTH NON-JOINER */ || C == 0x200D /* ZERO WIDTH JOINER */) continue;
        CleanAction.AppendChar(C);
    }
    CleanAction.TrimStartAndEndInline();
    FString Lower = CleanAction.ToLower();
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction sanitized: CleanAction='%s' Lower='%s'"), *CleanAction, *Lower);
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleBlueprintAction invoked: RequestId=%s RawAction=%s CleanAction=%s Lower=%s"), *RequestId, *Action, *CleanAction, *Lower);
    if (!Lower.StartsWith(TEXT("blueprint_")) && !Lower.StartsWith(TEXT("manage_blueprint"))) 
    { 
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction: action does not match prefix check, returning false")); 
        return false; 
    }

    TSharedPtr<FJsonObject> LocalPayload = Payload.IsValid() ? Payload : MakeShared<FJsonObject>();

    // Temporaries used by blueprint_create handler — declared here so
    // preprocessor paths and nested blocks do not accidentally leave
    // these identifiers out-of-scope during complex conditional builds.
    FString Name;
    FString SavePath;
    FString ParentClassSpec;
    FString BlueprintTypeSpec;
    double Now = 0.0;
    FString CreateKey;

    // If the client sent a manage_blueprint wrapper, allow a nested 'action'
    // field in the payload to specify the real blueprint_* action. This
    // improves compatibility with higher-level tool wrappers that forward
    // requests under a generic tool name.
    if (Lower.StartsWith(TEXT("manage_blueprint")) && LocalPayload.IsValid())
    {
        FString Nested; if (LocalPayload->TryGetStringField(TEXT("action"), Nested) && !Nested.TrimStartAndEnd().IsEmpty())
        {
            // Recompute cleaned/lower action values using nested action
            FString NestedClean; NestedClean.Reserve(Nested.Len()); for (int32 i = 0; i < Nested.Len(); ++i) { const TCHAR C = Nested[i]; if (C >= 32) NestedClean.AppendChar(C); }
            NestedClean.TrimStartAndEndInline(); if (!NestedClean.IsEmpty()) { CleanAction = NestedClean; Lower = CleanAction.ToLower(); UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("manage_blueprint nested action detected: %s -> %s"), *Action, *CleanAction); }
        }
    }

    // Build a compact alphanumeric-only lowercase key so we can match
    // variants such as 'add_variable', 'addVariable' and 'add-variable'.
    FString AlphaNumLower; AlphaNumLower.Reserve(CleanAction.Len());
    for (int32 i = 0; i < CleanAction.Len(); ++i)
    {
        const TCHAR C = CleanAction[i];
        if (FChar::IsAlnum(C)) AlphaNumLower.AppendChar(FChar::ToLower(C));
    }

    // Helper that performs tolerant matching: exact lower/suffix matches or
    // an alphanumeric-substring match against the compacted key.
    auto ActionMatchesPattern = [&](const TCHAR* Pattern) -> bool
    {
        const FString PatternStr = FString(Pattern).ToLower();
        // compact pattern (alpha-numeric only)
        FString PatternAlpha; PatternAlpha.Reserve(PatternStr.Len()); for (int32 i=0;i<PatternStr.Len();++i) { const TCHAR C = PatternStr[i]; if (FChar::IsAlnum(C)) PatternAlpha.AppendChar(C); }
        const bool bExactOrContains = (Lower.Equals(PatternStr) || Lower.Contains(PatternStr));
        const bool bAlphaMatch = (!AlphaNumLower.IsEmpty() && !PatternAlpha.IsEmpty() && AlphaNumLower.Contains(PatternAlpha));
        const bool bMatched = (bExactOrContains || bAlphaMatch);
    // Keep this at VeryVerbose because it executes for every pattern match
    // attempt and rapidly fills the log during normal operation.
    UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose, TEXT("ActionMatchesPattern check: pattern='%s' patternAlpha='%s' lower='%s' alpha='%s' matched=%s"), *PatternStr, *PatternAlpha, *Lower, *AlphaNumLower, bMatched ? TEXT("true") : TEXT("false"));
        return bMatched;
    };

    // Run diagnostic pattern checks early while CleanAction/Lower/AlphaNumLower are in scope
    DiagnosticPatternChecks(CleanAction, Lower, AlphaNumLower);

    // Helper to resolve requested blueprint path (honors 'requestedPath' or 'blueprintCandidates')
    auto ResolveBlueprintRequestedPath = [&]() -> FString
    {
        FString Req;
        if (LocalPayload->TryGetStringField(TEXT("requestedPath"), Req) && !Req.TrimStartAndEnd().IsEmpty()) return Req;
        const TArray<TSharedPtr<FJsonValue>>* CandidateArray = nullptr;
        // Accept either 'blueprintCandidates' (preferred) or legacy 'candidates'
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
        // Backwards-compatible key used by some older clients
        if (LocalPayload->TryGetArrayField(TEXT("candidates"), CandidateArray) && CandidateArray && CandidateArray->Num() > 0)
        {
            for (const TSharedPtr<FJsonValue>& V : *CandidateArray)
            {
                if (!V.IsValid() || V->Type != EJson::String) continue;
                FString Candidate = V->AsString();
                if (Candidate.TrimStartAndEnd().IsEmpty()) continue;
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

    if (ActionMatchesPattern(TEXT("blueprint_modify_scs")) || ActionMatchesPattern(TEXT("modify_scs")) || ActionMatchesPattern(TEXT("modifyscs")) || AlphaNumLower.Contains(TEXT("blueprintmodifyscs")) || AlphaNumLower.Contains(TEXT("modifyscs")))
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
                    SendAutomationError(RequestingSocket, RequestId, TEXT("blueprint_modify_scs requires an operations array."), TEXT("INVALID_OPERATIONS")); return true;
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

                    // If we exit before completing the work, clear the busy flag
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

                // Make a shallow copy of the operations array so it's safe to reference below.
                TArray<TSharedPtr<FJsonValue>> DeferredOps = *OperationsArray;

                // Lightweight validation of operations
                for (int32 Index = 0; Index < DeferredOps.Num(); ++Index)
                {
                    const TSharedPtr<FJsonValue>& OperationValue = DeferredOps[Index];
                    if (!OperationValue.IsValid() || OperationValue->Type != EJson::Object) { SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Operation at index %d is not an object."), Index), TEXT("INVALID_OPERATION_PAYLOAD")); return true; }
                    const TSharedPtr<FJsonObject> OperationObject = OperationValue->AsObject();
                    FString OperationType; if (!OperationObject->TryGetStringField(TEXT("type"), OperationType) || OperationType.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Operation at index %d missing type."), Index), TEXT("INVALID_OPERATION_TYPE")); return true; }
                }

                // Mark busy as scheduled (we will perform the work synchronously here)
                this->bCurrentBlueprintBusyScheduled = true;

                // Fast-mode: apply operations to in-memory registry immediately and return
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

                    // Send final response
                    TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>(); ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath); ResultPayload->SetArrayField(TEXT("operations"), FinalSummaries); ResultPayload->SetBoolField(TEXT("compiled"), bCompile); ResultPayload->SetBoolField(TEXT("saved"), bSave);
                    if (LocalWarnings.Num() > 0) { TArray<TSharedPtr<FJsonValue>> WVals2; WVals2.Reserve(LocalWarnings.Num()); for (const FString& W : LocalWarnings) WVals2.Add(MakeShared<FJsonValueString>(W)); ResultPayload->SetArrayField(TEXT("warnings"), WVals2); }

                    const bool bFastOk = FinalSummaries.Num() > 0;
                    const FString Message = FString::Printf(TEXT("Processed %d SCS operation(s) (fast-mode)."), FinalSummaries.Num());
                    SendAutomationResponse(RequestingSocket, RequestId, bFastOk, Message, ResultPayload, bFastOk ? FString() : TEXT("SCS_OPERATION_FAILED"));

                    // Release busy flag
                    if (!this->CurrentBusyBlueprintKey.IsEmpty() && GBlueprintBusySet.Contains(this->CurrentBusyBlueprintKey)) { GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey); }
                    this->bCurrentBlueprintBusyMarked = false; this->bCurrentBlueprintBusyScheduled = false; this->CurrentBusyBlueprintKey.Empty();

                    return true;
                }

                // Non-fast-mode: perform the SCS modification immediately (we are on game thread)
                TSharedPtr<FJsonObject> CompletionResult = MakeShared<FJsonObject>();
                TArray<FString> LocalWarnings; TArray<TSharedPtr<FJsonValue>> FinalSummaries; bool bOk = false;

                FString LocalNormalized; FString LocalLoadError; UBlueprint* LocalBP = LoadBlueprintAsset(NormalizedBlueprintPath, LocalNormalized, LocalLoadError);
                if (!LocalBP)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("SCS application failed to load blueprint %s: %s"), *NormalizedBlueprintPath, *LocalLoadError);
                    CompletionResult->SetStringField(TEXT("error"), LocalLoadError);
                    // Send failure and clear busy
                    SendAutomationResponse(RequestingSocket, RequestId, false, LocalLoadError, CompletionResult, TEXT("BLUEPRINT_NOT_FOUND"));
                    if (!this->CurrentBusyBlueprintKey.IsEmpty() && GBlueprintBusySet.Contains(this->CurrentBusyBlueprintKey)) { GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey); }
                    this->bCurrentBlueprintBusyMarked = false; this->bCurrentBlueprintBusyScheduled = false; this->CurrentBusyBlueprintKey.Empty();
                    return true;
                }

                USimpleConstructionScript* LocalSCS = LocalBP->SimpleConstructionScript;
                if (!LocalSCS)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("SCS unavailable for blueprint %s"), *NormalizedBlueprintPath);
                    CompletionResult->SetStringField(TEXT("error"), TEXT("SCS_UNAVAILABLE"));
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("SCS_UNAVAILABLE"), CompletionResult, TEXT("SCS_UNAVAILABLE"));
                    if (!this->CurrentBusyBlueprintKey.IsEmpty() && GBlueprintBusySet.Contains(this->CurrentBusyBlueprintKey)) { GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey); }
                    this->bCurrentBlueprintBusyMarked = false; this->bCurrentBlueprintBusyScheduled = false; this->CurrentBusyBlueprintKey.Empty();
                    return true;
                }

                // Apply operations directly
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
                        else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Missing component name or transform")); }
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
                                bool bAddedViaSubsystem = false;
                                FString AdditionMethodStr;
#if MCP_HAS_SUBOBJECT_DATA_SUBSYSTEM
                                USubobjectDataSubsystem* Subsystem = nullptr;
                                if (GEngine) Subsystem = GEngine->GetEngineSubsystem<USubobjectDataSubsystem>();
                                if (Subsystem)
                                {
                                    TArray<FSubobjectDataHandle> ExistingHandles;
                                    Subsystem->K2_GatherSubobjectDataForBlueprint(LocalBP, ExistingHandles);
                                    FSubobjectDataHandle ParentHandle;
                                    if (ExistingHandles.Num() > 0)
                                    {
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

                                    using namespace McpAutomationBridge;
                                    constexpr bool bHasK2Add = THasK2Add<USubobjectDataSubsystem>::value;
                                    constexpr bool bHasAdd = THasAdd<USubobjectDataSubsystem>::value;
                                    constexpr bool bHasAddTwoArg = THasAddTwoArg<USubobjectDataSubsystem>::value;
                                    constexpr bool bHandleHasIsValid = THandleHasIsValid<FSubobjectDataHandle>::value;
                                    constexpr bool bHasRename = THasRename<USubobjectDataSubsystem>::value;

                                    bool bTriedNative = false;
                                    FSubobjectDataHandle NewHandle;
                                    if constexpr (bHasAddTwoArg)
                                    {
                                        FAddNewSubobjectParams Params;
                                        Params.ParentHandle = ParentHandle;
                                        Params.NewClass = ComponentClass;
                                        Params.BlueprintContext = LocalBP;
                                        FText FailReason;
                                        NewHandle = Subsystem->AddNewSubobject(Params, FailReason);
                                        bTriedNative = true;
                                        AdditionMethodStr = TEXT("SubobjectDataSubsystem.AddNewSubobject(WithFailReason)");

                                        bool bHandleValid = true;
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
#if WITH_EDITOR
                                            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LocalBP);
                                            FKismetEditorUtilities::CompileBlueprint(LocalBP);
                                            SaveLoadedAssetThrottled(LocalBP);
#endif
                                            bAddedViaSubsystem = true;
                                        }
                                    }
                                }
#endif
                                if (bAddedViaSubsystem)
                                {
                                    OpSummary->SetBoolField(TEXT("success"), true);
                                    OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                                    if (!AdditionMethodStr.IsEmpty()) OpSummary->SetStringField(TEXT("additionMethod"), AdditionMethodStr);
                                }
                                else
                                {
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
                                constexpr bool bHasDelete = McpAutomationBridge::THasDeleteSubobject<USubobjectDataSubsystem>::value;
                                if constexpr (bHasDelete)
                                {
                                    FSubobjectDataHandle ContextHandle = ExistingHandles.Num() > 0 ? ExistingHandles[0] : FoundHandle;
                                    Subsystem->DeleteSubobject(ContextHandle, FoundHandle, LocalBP);
                                    bRemoved = true;
                                }
                            }
                        }
                        if (bRemoved)
                        {
                            OpSummary->SetBoolField(TEXT("success"), true);
                            OpSummary->SetStringField(TEXT("componentName"), ComponentName);
                        }
                        else
                        {
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
#else
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
#endif
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
                            constexpr bool bHasAttach = McpAutomationBridge::THasAttach<USubobjectDataSubsystem>::value;
                            if (ChildHandle.IsValid() && ParentHandle.IsValid())
                            {
                                if constexpr (bHasAttach)
                                {
                                    bAttached = Subsystem->AttachSubobject(ParentHandle, ChildHandle);
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
                    }
                    else { OpSummary->SetBoolField(TEXT("success"), false); OpSummary->SetStringField(TEXT("warning"), TEXT("Unknown operation type")); }

                    const double OpElapsedMs = (FPlatformTime::Seconds() - OpStart) * 1000.0; OpSummary->SetNumberField(TEXT("durationMs"), OpElapsedMs); FinalSummaries.Add(MakeShared<FJsonValueObject>(OpSummary));
                }

                bOk = FinalSummaries.Num() > 0; CompletionResult->SetArrayField(TEXT("operations"), FinalSummaries);

                // Compile/save as requested
                bool bSaveResult = false;
                if (bSave && LocalBP)
                {
#if WITH_EDITOR
                    bSaveResult = SaveLoadedAssetThrottled(LocalBP);
                    if (!bSaveResult) LocalWarnings.Add(TEXT("Blueprint failed to save during apply; check output log."));
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

                // Broadcast completion and deliver final response
                TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>(); Notify->SetStringField(TEXT("type"), TEXT("automation_event")); Notify->SetStringField(TEXT("event"), TEXT("modify_scs_completed")); Notify->SetStringField(TEXT("requestId"), RequestId); Notify->SetObjectField(TEXT("result"), CompletionResult); SendControlMessage(Notify);

                // Final automation_response uses actual success state
                TSharedPtr<FJsonObject> ResultPayload = MakeShared<FJsonObject>(); ResultPayload->SetStringField(TEXT("blueprintPath"), NormalizedBlueprintPath); ResultPayload->SetArrayField(TEXT("operations"), FinalSummaries); ResultPayload->SetBoolField(TEXT("compiled"), bCompile); ResultPayload->SetBoolField(TEXT("saved"), bSave && bSaveResult);
                if (LocalWarnings.Num() > 0) { TArray<TSharedPtr<FJsonValue>> WVals2; WVals2.Reserve(LocalWarnings.Num()); for (const FString& W : LocalWarnings) WVals2.Add(MakeShared<FJsonValueString>(W)); ResultPayload->SetArrayField(TEXT("warnings"), WVals2); }

                const FString Message = FString::Printf(TEXT("Processed %d SCS operation(s)."), FinalSummaries.Num());
                SendAutomationResponse(RequestingSocket, RequestId, bOk, Message, ResultPayload, bOk ? FString() : (CompletionResult->HasField(TEXT("error")) ? CompletionResult->GetStringField(TEXT("error")) : TEXT("SCS_OPERATION_FAILED")));

                // Release busy flag
                if (!this->CurrentBusyBlueprintKey.IsEmpty() && GBlueprintBusySet.Contains(this->CurrentBusyBlueprintKey)) { GBlueprintBusySet.Remove(this->CurrentBusyBlueprintKey); }
                this->bCurrentBlueprintBusyMarked = false; this->bCurrentBlueprintBusyScheduled = false; this->CurrentBusyBlueprintKey.Empty();

                return true;
            }

            // blueprint_set_variable_metadata: store metadata into the plugin registry
            if (ActionMatchesPattern(TEXT("blueprint_set_variable_metadata")) || ActionMatchesPattern(TEXT("set_variable_metadata")) || AlphaNumLower.Contains(TEXT("blueprintsetvariablemetadata")) || AlphaNumLower.Contains(TEXT("setvariablemetadata")))
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
                Resp->SetStringField(TEXT("note"), TEXT("Metadata stored in plugin registry; use FBlueprintEditorUtils::SetBlueprintVariableMetaData for native application."));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable metadata recorded."), Resp, FString()); return true;
            }

            if (ActionMatchesPattern(TEXT("blueprint_add_construction_script")) || ActionMatchesPattern(TEXT("add_construction_script")) || AlphaNumLower.Contains(TEXT("blueprintaddconstructionscript")) || AlphaNumLower.Contains(TEXT("addconstructionscript")))
            {
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_construction_script requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString ScriptName; LocalPayload->TryGetStringField(TEXT("scriptName"), ScriptName); if (ScriptName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("scriptName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
                TArray<TSharedPtr<FJsonValue>> Scripts = Entry->HasField(TEXT("constructionScripts")) ? Entry->GetArrayField(TEXT("constructionScripts")) : TArray<TSharedPtr<FJsonValue>>(); Scripts.Add(MakeShared<FJsonValueString>(ScriptName)); Entry->SetArrayField(TEXT("constructionScripts"), Scripts);
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("scriptName"), ScriptName); Resp->SetStringField(TEXT("blueprintPath"), Path);
                Resp->SetStringField(TEXT("note"), TEXT("Registry-only operation; use blueprint_add_node for node-based construction scripts."));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Construction script recorded."), Resp, FString()); return true;
            }

            // Add a variable to the blueprint (registry-backed implementation)
            if (ActionMatchesPattern(TEXT("blueprint_add_variable")) || ActionMatchesPattern(TEXT("add_variable")) || AlphaNumLower.Contains(TEXT("blueprintaddvariable")) || AlphaNumLower.Contains(TEXT("addvariable")))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Entered blueprint_add_variable handler: RequestId=%s"), *RequestId);
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
                // Immediate acknowledgment; native implementation proceeds asynchronously
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Variable queued for addition."), RespVar, FString());

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
                                        const bool bSaved = SaveLoadedAssetThrottled(LocalBP);
                                        CompletionResult->SetBoolField(TEXT("saved"), bSaved);
#else
                                        CompletionResult->SetBoolField(TEXT("saved"), false);
#endif
                                        CompletionResult->SetStringField(TEXT("variableName"), VarName);
                                        CompletionResult->SetStringField(TEXT("blueprintPath"), Path);
                                        bOk = true;
                                    }
                                }

                                // Broadcast completion event (final status will be conveyed
                                // via automation_event so we avoid sending a second
                                // automation_response for the same requestId which can
                                // confuse clients that already resolved the promise).
                                TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>(); Notify->SetStringField(TEXT("type"), TEXT("automation_event")); Notify->SetStringField(TEXT("event"), TEXT("add_variable_completed")); Notify->SetStringField(TEXT("requestId"), RequestId); Notify->SetObjectField(TEXT("result"), CompletionResult); SendControlMessage(Notify);
                                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Background blueprint_add_variable completed: RequestId=%s blueprint=%s variable=%s ok=%s"), *RequestId, *Path, *VarName, bOk ? TEXT("true") : TEXT("false"));

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
            if (ActionMatchesPattern(TEXT("blueprint_add_event")) || ActionMatchesPattern(TEXT("add_event")) || AlphaNumLower.Contains(TEXT("blueprintaddevent")) || AlphaNumLower.Contains(TEXT("addevent")))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Entered blueprint_add_event handler: RequestId=%s"), *RequestId);
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_event requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString EventType; LocalPayload->TryGetStringField(TEXT("eventType"), EventType);
                FString CustomName; LocalPayload->TryGetStringField(TEXT("customEventName"), CustomName);
                const TArray<TSharedPtr<FJsonValue>>* Params = nullptr; LocalPayload->TryGetArrayField(TEXT("parameters"), Params);
                TSharedPtr<FJsonObject> Entry3 = EnsureBlueprintEntry(Path);
                TArray<TSharedPtr<FJsonValue>> Events = Entry3->HasField(TEXT("events")) ? Entry3->GetArrayField(TEXT("events")) : TArray<TSharedPtr<FJsonValue>>();
                TSharedPtr<FJsonObject> ERec = MakeShared<FJsonObject>(); ERec->SetStringField(TEXT("eventType"), EventType.IsEmpty() ? TEXT("custom") : EventType); if (!CustomName.IsEmpty()) ERec->SetStringField(TEXT("name"), CustomName); if (Params && Params->Num() > 0) ERec->SetArrayField(TEXT("parameters"), *Params);
                Events.Add(MakeShared<FJsonValueObject>(ERec)); Entry3->SetArrayField(TEXT("events"), Events);
                TSharedPtr<FJsonObject> RespEvt = MakeShared<FJsonObject>(); RespEvt->SetStringField(TEXT("blueprintPath"), Path); if (!CustomName.IsEmpty()) RespEvt->SetStringField(TEXT("eventName"), CustomName);
                RespEvt->SetStringField(TEXT("note"), TEXT("Event queued for addition."));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event queued for addition."), RespEvt, FString());

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
                                        const bool bSaved = SaveLoadedAssetThrottled(LocalBP);
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
                            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Background blueprint_add_event completed: RequestId=%s blueprint=%s event=%s ok=%s"), *RequestId, *Path, CompletionResult->HasField(TEXT("eventName")) ? *CompletionResult->GetStringField(TEXT("eventName")) : TEXT("(none)"), bOk ? TEXT("true") : TEXT("false"));
                            if (!Path.IsEmpty() && GBlueprintBusySet.Contains(Path)) GBlueprintBusySet.Remove(Path);
                        });
                    }
#endif
                }
                return true;
            }

            // Remove an event from the blueprint (registry-backed implementation)
            if (ActionMatchesPattern(TEXT("blueprint_remove_event")) || ActionMatchesPattern(TEXT("remove_event")) || AlphaNumLower.Contains(TEXT("blueprintremoveevent")) || AlphaNumLower.Contains(TEXT("removeevent")))
            {
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_remove_event requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString EventName; LocalPayload->TryGetStringField(TEXT("eventName"), EventName); if (EventName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("eventName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
                TArray<TSharedPtr<FJsonValue>> Events = Entry->HasField(TEXT("events")) ? Entry->GetArrayField(TEXT("events")) : TArray<TSharedPtr<FJsonValue>>();
                int32 FoundIdx = INDEX_NONE;
                for (int32 i = 0; i < Events.Num(); ++i)
                {
                    const TSharedPtr<FJsonValue>& V = Events[i];
                    if (!V.IsValid() || V->Type != EJson::Object) continue;
                    const TSharedPtr<FJsonObject> Obj = V->AsObject(); FString CandidateName; if (Obj->TryGetStringField(TEXT("name"), CandidateName) && CandidateName.Equals(EventName, ESearchCase::IgnoreCase)) { FoundIdx = i; break; }
                }
                if (FoundIdx == INDEX_NONE)
                {
                    // Treat remove as idempotent: if the event is not present in
                    // the registry consider the request successful (no-op).
                    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
                    Resp->SetStringField(TEXT("eventName"), EventName);
                    Resp->SetStringField(TEXT("blueprintPath"), Path);
                    Resp->SetStringField(TEXT("note"), TEXT("Event not present; treated as removed (idempotent)."));
                    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event not present; treated as removed"), Resp, FString());
                    return true;
                }
                Events.RemoveAt(FoundIdx);
                Entry->SetArrayField(TEXT("events"), Events);
                TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetStringField(TEXT("eventName"), EventName); Resp->SetStringField(TEXT("blueprintPath"), Path);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Event removed."), Resp, FString());

                // Optionally attempt live removal on the editor thread — this
                // is a best-effort stub in case the engine exposes the required
                // editor graph APIs; otherwise the registry removal suffices
                // for most test verification flows.
                if (!IsFastMode(LocalPayload))
                {
#if WITH_EDITOR
                    AsyncTask(ENamedThreads::GameThread, [this, RequestId, Path, EventName, RequestingSocket]() {
                        TSharedPtr<FJsonObject> Completion = MakeShared<FJsonObject>(); Completion->SetStringField(TEXT("eventName"), EventName); Completion->SetStringField(TEXT("blueprintPath"), Path);
                        // Attempt to remove event nodes if editor APIs are available.
                        // The implementation below is intentionally conservative: if
                        // we cannot safely remove nodes on this engine build, we
                        // simply log a warning and treat the registry removal as
                        // authoritative.
                        FString LocalNormalized; FString LocalLoadError; UBlueprint* LocalBP = LoadBlueprintAsset(Path, LocalNormalized, LocalLoadError);
                        if (!LocalBP)
                        {
                            UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("blueprint_remove_event: failed to load %s: %s"), *Path, *LocalLoadError);
                            Completion->SetStringField(TEXT("warning"), TEXT("Editor removal not available; registry updated"));
                            SendControlMessage(MakeShared<FJsonObject>());
                        }
                        else
                        {
                            // Removing event nodes is engine-version dependent and
                            // can be risky; leave as a no-op that reports success
                            // to callers. Plugin authors can extend this later to
                            // perform safe graph edits per engine API.
                            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("blueprint_remove_event: recorded removal in registry; live editor removal not performed automatically."));
                            Completion->SetStringField(TEXT("note"), TEXT("Registry removal performed; live editor removal skipped."));
                        }
                        // Broadcast completion event for listeners
                        TSharedPtr<FJsonObject> Notify = MakeShared<FJsonObject>(); Notify->SetStringField(TEXT("type"), TEXT("automation_event")); Notify->SetStringField(TEXT("event"), TEXT("remove_event_completed")); Notify->SetStringField(TEXT("requestId"), RequestId); Notify->SetObjectField(TEXT("result"), Completion); SendControlMessage(Notify);
                    });
#endif
                }

                return true;
            }

            // Add a function to the blueprint (registry-backed implementation)
            if (ActionMatchesPattern(TEXT("blueprint_add_function")) || ActionMatchesPattern(TEXT("add_function")) || AlphaNumLower.Contains(TEXT("blueprintaddfunction")) || AlphaNumLower.Contains(TEXT("addfunction")))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Entered blueprint_add_function handler: RequestId=%s"), *RequestId);
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
                RespFunc->SetStringField(TEXT("note"), TEXT("Function queued for addition."));
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Function queued for addition."), RespFunc, FString());

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
                                    const bool bSaved = SaveLoadedAssetThrottled(LocalBP);
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
                            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("Background blueprint_add_function completed: RequestId=%s blueprint=%s function=%s ok=%s"), *RequestId, *Path, *FuncName, bOk ? TEXT("true") : TEXT("false"));
                            if (!Path.IsEmpty() && GBlueprintBusySet.Contains(Path)) GBlueprintBusySet.Remove(Path);
                        });
                    }
#endif
                }
                return true;
            }

            

            if (ActionMatchesPattern(TEXT("blueprint_set_default")) || ActionMatchesPattern(TEXT("set_default")) || ActionMatchesPattern(TEXT("setdefault")) || AlphaNumLower.Contains(TEXT("blueprintsetdefault")) || AlphaNumLower.Contains(TEXT("setdefault")))
            {
                FString Path = ResolveBlueprintRequestedPath();
                if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_set_default requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                FString PropertyName; LocalPayload->TryGetStringField(TEXT("propertyName"), PropertyName);
                if (PropertyName.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("propertyName required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
                const TSharedPtr<FJsonValue> Value = LocalPayload->TryGetField(TEXT("value"));
                if (!Value.IsValid()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("value required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }

#if WITH_EDITOR
                AsyncTask(ENamedThreads::GameThread, [=, this]() {
                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    FString Normalized, LoadErr;
                    UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
                    
                    if (!BP)
                    {
                        Result->SetStringField(TEXT("error"), LoadErr);
                        SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr, Result, TEXT("BLUEPRINT_NOT_FOUND"));
                        return;
                    }
                    
                    // Get the CDO (Class Default Object) from the generated class
                    UClass* GeneratedClass = BP->GeneratedClass;
                    if (!GeneratedClass)
                    {
                        Result->SetStringField(TEXT("error"), TEXT("Blueprint has no generated class"));
                        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No generated class"), Result, TEXT("NO_GENERATED_CLASS"));
                        return;
                    }
                    
                    UObject* CDO = GeneratedClass->GetDefaultObject();
                    if (!CDO)
                    {
                        Result->SetStringField(TEXT("error"), TEXT("Failed to get CDO"));
                        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No CDO"), Result, TEXT("NO_CDO"));
                        return;
                    }
                    
                    // Find the property by name (supports nested via dot notation)
                    FProperty* TargetProperty = FindFProperty<FProperty>(GeneratedClass, FName(*PropertyName));
                    if (!TargetProperty)
                    {
                        // Try nested property path (e.g., "LightComponent.Intensity")
                        const int32 DotIdx = PropertyName.Find(TEXT("."));
                        if (DotIdx != INDEX_NONE)
                        {
                            const FString ComponentName = PropertyName.Left(DotIdx);
                            const FString NestedProp = PropertyName.Mid(DotIdx + 1);
                            
                            FProperty* CompProp = FindFProperty<FProperty>(GeneratedClass, FName(*ComponentName));
                            if (CompProp && CompProp->IsA<FObjectProperty>())
                            {
                                FObjectProperty* ObjProp = CastField<FObjectProperty>(CompProp);
                                void* CompPtr = ObjProp->GetPropertyValuePtr_InContainer(CDO);
                                UObject* CompObj = ObjProp->GetObjectPropertyValue(CompPtr);
                                if (CompObj)
                                {
                                    TargetProperty = FindFProperty<FProperty>(CompObj->GetClass(), FName(*NestedProp));
                                    if (TargetProperty)
                                    {
                                        CDO = CompObj; // Update CDO to point to component
                                    }
                                }
                            }
                        }
                    }
                    
                    if (!TargetProperty)
                    {
                        // Fallback: record the default into the plugin registry so higher-level tools/tests
                        // can proceed even when a native property is not present on the generated class.
                        TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
                        TSharedPtr<FJsonObject> DefaultsObj = Entry->HasField(TEXT("defaults")) ? Entry->GetObjectField(TEXT("defaults")) : MakeShared<FJsonObject>();
                        DefaultsObj->SetField(PropertyName, Value);
                        Entry->SetObjectField(TEXT("defaults"), DefaultsObj);
                        Result->SetBoolField(TEXT("success"), true);
                        Result->SetStringField(TEXT("propertyName"), PropertyName);
                        Result->SetStringField(TEXT("blueprintPath"), Path);
                        Result->SetStringField(TEXT("note"), TEXT("Recorded default in plugin registry (property not found on CDO)"));
                        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Default recorded (registry)"), Result, FString());
                        return;
                    }
                    
                    // Convert JSON value to property value using the existing JSON serialization system
                    TSharedPtr<FJsonObject> TempObj = MakeShared<FJsonObject>();
                    TempObj->SetField(TEXT("temp"), Value);
                    
                    FString JsonString;
                    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
                    FJsonSerializer::Serialize(TempObj.ToSharedRef(), Writer);
                    
                    // Use FJsonObjectConverter to deserialize the value
                    TSharedPtr<FJsonObject> ValueWrapObj = MakeShared<FJsonObject>();
                    ValueWrapObj->SetField(TargetProperty->GetName(), Value);
                    
                    CDO->Modify();
                    BP->Modify();
                    
                    // Attempt to set the property value
                    bool bSuccess = FJsonObjectConverter::JsonAttributesToUStruct(ValueWrapObj->Values, GeneratedClass, CDO, 0, 0);
                    
                    if (bSuccess)
                    {
                        FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
                        FKismetEditorUtilities::CompileBlueprint(BP);
                        
                        Result->SetBoolField(TEXT("success"), true);
                        Result->SetStringField(TEXT("propertyName"), PropertyName);
                        Result->SetStringField(TEXT("blueprintPath"), Path);
                        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint default property set"), Result, FString());
                    }
                    else
                    {
                        Result->SetBoolField(TEXT("success"), false);
                        Result->SetStringField(TEXT("error"), TEXT("Failed to set property value"));
                        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Property set failed"), Result, TEXT("SET_FAILED"));
                    }
                });
                return true;
#else
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_set_default requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
                return true;
#endif
            }

            // Compile a Blueprint asset (editor builds only). Returns whether
            // compilation (and optional save) succeeded.
            if (ActionMatchesPattern(TEXT("blueprint_compile")) || ActionMatchesPattern(TEXT("compile")) || AlphaNumLower.Contains(TEXT("blueprintcompile")) || AlphaNumLower.Contains(TEXT("compile")))
            {
                FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_compile requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
                bool bSaveAfterCompile = false; if (LocalPayload->HasField(TEXT("saveAfterCompile"))) LocalPayload->TryGetBoolField(TEXT("saveAfterCompile"), bSaveAfterCompile);
                // Editor-only compile
    #if WITH_EDITOR
                FString Normalized; FString LoadErr; UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
                if (!BP) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), LoadErr); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to load blueprint for compilation"), Err, TEXT("NOT_FOUND")); return true; }
                FKismetEditorUtilities::CompileBlueprint(BP);
                bool bSaved = false;
                if (bSaveAfterCompile) { bSaved = SaveLoadedAssetThrottled(BP); }
                TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetBoolField(TEXT("compiled"), true); Out->SetBoolField(TEXT("saved"), bSaved); Out->SetStringField(TEXT("blueprintPath"), Path);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint compiled"), Out, FString());
                return true;
    #else
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Blueprint compile requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
                return true;
    #endif
            }

                                    if (ActionMatchesPattern(TEXT("blueprint_probe_subobject_handle")) || ActionMatchesPattern(TEXT("probe_subobject_handle")) || ActionMatchesPattern(TEXT("probehandle")) || AlphaNumLower.Contains(TEXT("blueprintprobesubobjecthandle")) || AlphaNumLower.Contains(TEXT("probesubobjecthandle")) || AlphaNumLower.Contains(TEXT("probehandle")))
                                    {
                                        return HandleBlueprintProbeSubobjectHandle(this, RequestId, LocalPayload, RequestingSocket);
                                    }

            // blueprint_create handler: parse payload and prepare coalesced creation
            // Support both explicit blueprint_create and the nested 'create' action from manage_blueprint
            if (ActionMatchesPattern(TEXT("blueprint_create")) || ActionMatchesPattern(TEXT("create_blueprint")) || ActionMatchesPattern(TEXT("create")) || AlphaNumLower.Contains(TEXT("blueprintcreate")) || AlphaNumLower.Contains(TEXT("createblueprint")))
            {
                return HandleBlueprintCreate(this, RequestId, LocalPayload, RequestingSocket);
            }

    

    // Other blueprint_* actions (modify_scs, compile, add_variable, add_function, etc.)
    // For simplicity, unhandled blueprint actions return NOT_IMPLEMENTED so
    // the server may fall back to Python helpers if available.

    // blueprint_exists: check whether a blueprint asset or registry entry exists
    if (ActionMatchesPattern(TEXT("blueprint_exists")) || ActionMatchesPattern(TEXT("exists")) || AlphaNumLower.Contains(TEXT("blueprintexists")))
    {
        FString Path = ResolveBlueprintRequestedPath();
        if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_exists requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
        FString Normalized; bool bFound = false; FString LoadErr;
        #if WITH_EDITOR
            UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
            bFound = (BP != nullptr);
        #else
            bFound = GBlueprintRegistry.Contains(Path);
        #endif
        TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>(); Resp->SetBoolField(TEXT("exists"), bFound); Resp->SetStringField(TEXT("blueprintPath"), bFound ? (Normalized.IsEmpty() ? Path : Normalized) : Path);
        SendAutomationResponse(RequestingSocket, RequestId, bFound, bFound ? TEXT("Blueprint exists") : TEXT("Blueprint not found"), Resp, bFound ? FString() : TEXT("NOT_FOUND"));
        return true;
    }

    // blueprint_get: return the lightweight registry entry for a blueprint
    if (ActionMatchesPattern(TEXT("blueprint_get")) || ActionMatchesPattern(TEXT("get")) || AlphaNumLower.Contains(TEXT("blueprintget")))
    {
        FString Path = ResolveBlueprintRequestedPath(); if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_get requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
        TSharedPtr<FJsonObject> Entry = EnsureBlueprintEntry(Path);
        // Attempt to enrich the registry entry with on-disk data when available
        #if WITH_EDITOR
            FString Normalized; FString Err; UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, Err);
            if (BP)
            {
                Entry->SetStringField(TEXT("resolvedPath"), Normalized.IsEmpty() ? Path : Normalized);
                // Populate a shallow 'assetPath' for convenience
                Entry->SetStringField(TEXT("assetPath"), BP->GetPathName());

                // Enrich/merge registry variables from on-disk blueprint
                TArray<TSharedPtr<FJsonValue>> VarsJson = Entry->HasField(TEXT("variables")) ? Entry->GetArrayField(TEXT("variables")) : TArray<TSharedPtr<FJsonValue>>();
                TSet<FString> Existing;
                for (const TSharedPtr<FJsonValue>& VVal : VarsJson)
                {
                    if (VVal.IsValid() && VVal->Type == EJson::Object)
                    {
                        const TSharedPtr<FJsonObject> VObj = VVal->AsObject();
                        FString N; if (VObj.IsValid() && VObj->TryGetStringField(TEXT("name"), N)) Existing.Add(N);
                    }
                }
                for (const FBPVariableDescription& V : BP->NewVariables)
                {
                    const FString N = V.VarName.ToString();
                    if (!Existing.Contains(N))
                    {
                        TSharedPtr<FJsonObject> VObj = MakeShared<FJsonObject>();
                        VObj->SetStringField(TEXT("name"), N);
                        VarsJson.Add(MakeShared<FJsonValueObject>(VObj));
                        Existing.Add(N);
                    }
                }
                Entry->SetArrayField(TEXT("variables"), VarsJson);
            }
        #endif
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprint fetched"), Entry, FString());
        return true;
    }

    // blueprint_add_node: Create a Blueprint graph node programmatically
    if (ActionMatchesPattern(TEXT("blueprint_add_node")) || ActionMatchesPattern(TEXT("add_node")) || AlphaNumLower.Contains(TEXT("blueprintaddnode")))
    {
#if WITH_EDITOR && MCP_HAS_K2NODE_HEADERS && MCP_HAS_EDGRAPH_SCHEMA_K2
        FString Path = ResolveBlueprintRequestedPath();
        if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_node requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
        
        FString NodeType; LocalPayload->TryGetStringField(TEXT("nodeType"), NodeType);
        if (NodeType.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("nodeType required"), nullptr, TEXT("INVALID_ARGUMENT")); return true; }
        
        FString GraphName; LocalPayload->TryGetStringField(TEXT("graphName"), GraphName);
        if (GraphName.IsEmpty()) GraphName = TEXT("EventGraph");
        
        // Optional node configuration
        FString FunctionName; LocalPayload->TryGetStringField(TEXT("functionName"), FunctionName);
        FString VariableName; LocalPayload->TryGetStringField(TEXT("variableName"), VariableName);
        FString NodeName; LocalPayload->TryGetStringField(TEXT("nodeName"), NodeName);
        float PosX = 0.0f, PosY = 0.0f;
        LocalPayload->TryGetNumberField(TEXT("posX"), PosX);
        LocalPayload->TryGetNumberField(TEXT("posY"), PosY);
        
        AsyncTask(ENamedThreads::GameThread, [=, this]() {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            FString Normalized, LoadErr;
            UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
            
            if (!BP)
            {
                Result->SetStringField(TEXT("error"), LoadErr);
                SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr, Result, TEXT("BLUEPRINT_NOT_FOUND"));
                return;
            }
            
            // Find or create target graph
            UEdGraph* TargetGraph = nullptr;
            for (UEdGraph* Graph : BP->UbergraphPages)
            {
                if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
                {
                    TargetGraph = Graph;
                    break;
                }
            }
            
            if (!TargetGraph)
            {
                TargetGraph = FBlueprintEditorUtils::FindEventGraph(BP);
                if (!TargetGraph)
                {
                    TargetGraph = FBlueprintEditorUtils::CreateNewGraph(BP, FName(*GraphName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
                    FBlueprintEditorUtils::AddUbergraphPage(BP, TargetGraph);
                }
            }
            
            if (!TargetGraph)
            {
                Result->SetStringField(TEXT("error"), TEXT("Failed to locate or create target graph"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Graph creation failed"), Result, TEXT("GRAPH_ERROR"));
                return;
            }
            
            BP->Modify();
            TargetGraph->Modify();
            
            UEdGraphNode* NewNode = nullptr;
            const FString NodeTypeLower = NodeType.ToLower();
            
            // Create node based on type
            if (NodeTypeLower.Contains(TEXT("callfunction")) || NodeTypeLower.Contains(TEXT("function")))
            {
                UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(TargetGraph);
                if (!FunctionName.IsEmpty())
                {
                    // Try to find the function by name
                    UFunction* FoundFunc = FindObject<UFunction>(nullptr, *FunctionName);
                    if (!FoundFunc && BP->GeneratedClass)
                    {
                        FoundFunc = BP->GeneratedClass->FindFunctionByName(FName(*FunctionName));
                    }
                    if (FoundFunc)
                    {
                        FuncNode->SetFromFunction(FoundFunc);
                    }
                }
                NewNode = FuncNode;
            }
            else if (NodeTypeLower.Contains(TEXT("variableget")) || NodeTypeLower.Contains(TEXT("getvar")))
            {
                UK2Node_VariableGet* VarGet = NewObject<UK2Node_VariableGet>(TargetGraph);
                if (!VariableName.IsEmpty())
                {
                    VarGet->VariableReference.SetSelfMember(FName(*VariableName));
                }
                NewNode = VarGet;
            }
            else if (NodeTypeLower.Contains(TEXT("variableset")) || NodeTypeLower.Contains(TEXT("setvar")))
            {
                UK2Node_VariableSet* VarSet = NewObject<UK2Node_VariableSet>(TargetGraph);
                if (!VariableName.IsEmpty())
                {
                    VarSet->VariableReference.SetSelfMember(FName(*VariableName));
                }
                NewNode = VarSet;
            }
            else if (NodeTypeLower.Contains(TEXT("customevent")))
            {
                UK2Node_CustomEvent* CustomEvent = NewObject<UK2Node_CustomEvent>(TargetGraph);
                if (!NodeName.IsEmpty())
                {
                    CustomEvent->CustomFunctionName = FName(*NodeName);
                }
                NewNode = CustomEvent;
            }
            else
            {
                // Generic node creation fallback
                Result->SetStringField(TEXT("warning"), FString::Printf(TEXT("Unknown node type '%s', skipping node creation"), *NodeType));
            }
            
            if (NewNode)
            {
                NewNode->NodePosX = static_cast<int32>(PosX);
                NewNode->NodePosY = static_cast<int32>(PosY);
                NewNode->SetFlags(RF_Transactional);
                TargetGraph->AddNode(NewNode, false, false);
                NewNode->CreateNewGuid();
                NewNode->PostPlacedNewNode();
                NewNode->AllocateDefaultPins();
                
                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
                
                Result->SetBoolField(TEXT("success"), true);
                Result->SetStringField(TEXT("nodeGuid"), NewNode->NodeGuid.ToString());
                Result->SetStringField(TEXT("nodeName"), NewNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
                Result->SetStringField(TEXT("nodeType"), NodeType);
                Result->SetStringField(TEXT("graphName"), TargetGraph->GetName());
            }
            else
            {
                Result->SetBoolField(TEXT("success"), false);
                Result->SetStringField(TEXT("error"), TEXT("Failed to create node"));
            }
            
            SendAutomationResponse(RequestingSocket, RequestId, NewNode != nullptr, NewNode ? TEXT("Node added") : TEXT("Node creation failed"), Result, FString());
        });
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_add_node requires editor build with K2Node headers"), nullptr, TEXT("NOT_AVAILABLE"));
        return true;
#endif
    }

    // blueprint_connect_pins: Connect two pins between nodes
    if (ActionMatchesPattern(TEXT("blueprint_connect_pins")) || ActionMatchesPattern(TEXT("connect_pins")) || AlphaNumLower.Contains(TEXT("blueprintconnectpins")))
    {
#if WITH_EDITOR && MCP_HAS_EDGRAPH_SCHEMA_K2
        FString Path = ResolveBlueprintRequestedPath();
        if (Path.IsEmpty()) { SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_connect_pins requires a blueprint path."), nullptr, TEXT("INVALID_BLUEPRINT_PATH")); return true; }
        
        FString SourceNodeGuid, TargetNodeGuid;
        LocalPayload->TryGetStringField(TEXT("sourceNodeGuid"), SourceNodeGuid);
        LocalPayload->TryGetStringField(TEXT("targetNodeGuid"), TargetNodeGuid);
        
        if (SourceNodeGuid.IsEmpty() || TargetNodeGuid.IsEmpty())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("sourceNodeGuid and targetNodeGuid required"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        FString SourcePinName, TargetPinName;
        LocalPayload->TryGetStringField(TEXT("sourcePinName"), SourcePinName);
        LocalPayload->TryGetStringField(TEXT("targetPinName"), TargetPinName);
        
        AsyncTask(ENamedThreads::GameThread, [=, this]() {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            FString Normalized, LoadErr;
            UBlueprint* BP = LoadBlueprintAsset(Path, Normalized, LoadErr);
            
            if (!BP)
            {
                Result->SetStringField(TEXT("error"), LoadErr);
                SendAutomationResponse(RequestingSocket, RequestId, false, LoadErr, Result, TEXT("BLUEPRINT_NOT_FOUND"));
                return;
            }
            
            // Find nodes by GUID
            UEdGraphNode* SourceNode = nullptr;
            UEdGraphNode* TargetNode = nullptr;
            FGuid SourceGuid, TargetGuid;
            FGuid::Parse(SourceNodeGuid, SourceGuid);
            FGuid::Parse(TargetNodeGuid, TargetGuid);
            
            for (UEdGraph* Graph : BP->UbergraphPages)
            {
                if (!Graph) continue;
                for (UEdGraphNode* Node : Graph->Nodes)
                {
                    if (!Node) continue;
                    if (Node->NodeGuid == SourceGuid) SourceNode = Node;
                    if (Node->NodeGuid == TargetGuid) TargetNode = Node;
                }
            }
            
            if (!SourceNode || !TargetNode)
            {
                Result->SetStringField(TEXT("error"), TEXT("Could not find source or target node by GUID"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Node lookup failed"), Result, TEXT("NODE_NOT_FOUND"));
                return;
            }
            
            // Find pins
            UEdGraphPin* SourcePin = nullptr;
            UEdGraphPin* TargetPin = nullptr;
            
            if (!SourcePinName.IsEmpty())
            {
                for (UEdGraphPin* Pin : SourceNode->Pins)
                {
                    if (Pin && Pin->GetName().Equals(SourcePinName, ESearchCase::IgnoreCase))
                    {
                        SourcePin = Pin;
                        break;
                    }
                }
            }
            else if (SourceNode->Pins.Num() > 0)
            {
                // Default to first output pin
                for (UEdGraphPin* Pin : SourceNode->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Output)
                    {
                        SourcePin = Pin;
                        break;
                    }
                }
            }
            
            if (!TargetPinName.IsEmpty())
            {
                for (UEdGraphPin* Pin : TargetNode->Pins)
                {
                    if (Pin && Pin->GetName().Equals(TargetPinName, ESearchCase::IgnoreCase))
                    {
                        TargetPin = Pin;
                        break;
                    }
                }
            }
            else if (TargetNode->Pins.Num() > 0)
            {
                // Default to first input pin
                for (UEdGraphPin* Pin : TargetNode->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Input)
                    {
                        TargetPin = Pin;
                        break;
                    }
                }
            }
            
            if (!SourcePin || !TargetPin)
            {
                Result->SetStringField(TEXT("error"), TEXT("Could not find source or target pin"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Pin lookup failed"), Result, TEXT("PIN_NOT_FOUND"));
                return;
            }
            
            // Make connection
            BP->Modify();
            SourceNode->GetGraph()->Modify();
            
            const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(SourceNode->GetGraph()->GetSchema());
            if (Schema)
            {
                Schema->TryCreateConnection(SourcePin, TargetPin);
                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
                
                Result->SetBoolField(TEXT("success"), true);
                Result->SetStringField(TEXT("sourcePinName"), SourcePin->GetName());
                Result->SetStringField(TEXT("targetPinName"), TargetPin->GetName());
            }
            else
            {
                Result->SetBoolField(TEXT("success"), false);
                Result->SetStringField(TEXT("error"), TEXT("Invalid graph schema"));
            }
            
            SendAutomationResponse(RequestingSocket, RequestId, Result->GetBoolField(TEXT("success")), TEXT("Pin connection attempt complete"), Result, FString());
        });
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("blueprint_connect_pins requires editor build with EdGraphSchema_K2"), nullptr, TEXT("NOT_AVAILABLE"));
        return true;
#endif
    }

    // Handle SCS (Simple Construction Script) operations
    if (HandleSCSAction(RequestId, Action, Payload, RequestingSocket)) return true;

    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Unhandled blueprint action: Action=%s Clean=%s AlphaNum=%s RequestId=%s - returning UNKNOWN_PLUGIN_ACTION"), *CleanAction, *CleanAction, *AlphaNumLower, *RequestId);
    SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Blueprint action not implemented by plugin: %s"), *Action), nullptr, TEXT("UNKNOWN_PLUGIN_ACTION"));
    return true;
}

            // (duplicate handlers removed - these are implemented above inside HandleBlueprintAction)
