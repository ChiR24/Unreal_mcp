// The recipe's input section: one Enhanced Input action node, the mapping-context
// registration a Pawn or PlayerController needs before any key fires (made through
// the same hook code as every behaviour), and the key mapping, done last because
// the mapping context is another asset.
#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h"

#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersActionRouting.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersAssetResolution.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersKeyResolution.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/PackageName.h"

namespace McpBlueprintBehaviour::Detail
{
namespace
{
FString InputObjectPath(const FString& Path)
{
    return Path.Contains(TEXT(".")) ? Path : Path + TEXT(".") + FPackageName::GetShortName(Path);
}

// Which Actor classes can host input, and whether this one can register a context.
bool CheckInputHost(UBlueprint* Blueprint, bool bRegister, FString& OutError, FString& OutCode)
{
    UClass* Parent = Blueprint->ParentClass;
    OutCode = TEXT("INPUT_NOT_SUPPORTED");
    OutError = TEXT("Enhanced Input events fire only on an Actor-based Blueprint.");
    if (!Parent || !Parent->IsChildOf(AActor::StaticClass()))
    {
        return false;
    }
    OutError = FString::Printf(TEXT("%s is not a Pawn or PlayerController, so it cannot register a mapping context. An "
                                    "Actor gets Enhanced Input only after EnableInput(actor, player controller) or with "
                                    "Auto Receive Input set: pass input.registerContext false to author the input node "
                                    "alone, and register the context on the possessed Pawn."), *Parent->GetName());
    return !bRegister || Parent->IsChildOf(APawn::StaticClass()) || Parent->IsChildOf(APlayerController::StaticClass());
}
} // namespace

bool PlanInput(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Recipe, FPlan& Plan,
               const TSharedPtr<FJsonObject>& Report, FString& OutError, FString& OutCode)
{
    const TSharedPtr<FJsonObject>* Section = nullptr;
    if (!Recipe->HasField(TEXT("input")))
    {
        return true;
    }
    OutCode = TEXT("INVALID_RECIPE");
    OutError = TEXT("input must be an object.");
    if (!Recipe->TryGetObjectField(TEXT("input"), Section) ||
        !CheckKeys(*Section, {TEXT("id"), TEXT("inputActionPath"), TEXT("mappingContextPath"), TEXT("key"),
                              TEXT("registerContext")}, TEXT("input"), OutError))
    {
        return false;
    }
    const TSharedPtr<FJsonObject> Input = *Section;
    const bool bRegister = GetJsonBoolField(Input, TEXT("registerContext"), true);
    OutCode = TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED");
    OutError = TEXT("The Enhanced Input plugin is not enabled in this project; enable it to bind input.");
    Plan.Input = MakeShared<FJsonObject>();
    Report->SetObjectField(TEXT("input"), Plan.Input);
    if (!EnhancedInputLoaded() || !CheckDefaultInputClasses(Plan.Input, Report, OutError, OutCode) ||
        !CheckInputHost(Blueprint, bRegister, OutError, OutCode))
    {
        return false;
    }
    Plan.Key = GetJsonStringField(Input, TEXT("key"));
    OutCode = TEXT("INVALID_ARGUMENT");
    OutError = FString::Printf(TEXT("input.key '%s' is not a key name (LeftShift, SpaceBar, Gamepad_FaceButton_Bottom)."),
                               *Plan.Key);
    if (!Plan.Key.IsEmpty() && !FKey(FName(*Plan.Key)).IsValid())
    {
        return false;
    }
    FString Normalized;
    const FString ActionArg = GetJsonStringField(Input, TEXT("inputActionPath"));
    const FString ContextArg = GetJsonStringField(Input, TEXT("mappingContextPath"));
    UInputAction* Action = ActionArg.IsEmpty() ? nullptr : McpInputHandlers::LoadInputActionAsset(ActionArg, Normalized);
    UInputMappingContext* Context =
        ContextArg.IsEmpty() ? nullptr : McpInputHandlers::LoadInputMappingContextAsset(ContextArg, Normalized);
    OutCode = TEXT("ASSET_NOT_FOUND");
    OutError = FString::Printf(TEXT("input: '%s' is not an Input Action or '%s' is not an Input Mapping Context."),
                               *ActionArg, *ContextArg);
    if ((!ActionArg.IsEmpty() && !Action) || (!ContextArg.IsEmpty() && !Context))
    {
        return false;
    }
    // One resolver for every caller: the caller's context, else the one this Blueprint
    // already registers, else the shared project context.
    bool bRegistered = false;
    UInputMappingContext* Registered = Cast<UInputMappingContext>(RegisteredContext(Blueprint, Context, bRegistered));
    bRegistered = bRegistered || (!Context && Registered);
    Context = Context ? Context : Registered;
    Context = Context ? Context : McpInputHandlers::LoadInputMappingContextAsset(DefaultInputContext, Normalized);
    Plan.bCreateContext = Context == nullptr;
    Plan.ContextPath = Context ? Context->GetPathName() : InputObjectPath(DefaultInputContext);
    if (!Action && !Plan.Key.IsEmpty())
    {
        const FString Default = DefaultActionPath(Blueprint, Plan.Tag);
        Action = McpInputHandlers::LoadInputActionAsset(Default, Normalized);
        Plan.bCreateAction = Action == nullptr;
        Plan.ActionPath = InputObjectPath(Default);
    }
    Plan.ActionPath = Action ? Action->GetPathName() : Plan.ActionPath;
    OutCode = TEXT("INVALID_RECIPE");
    OutError = TEXT("input names no action, no key and registerContext false: it would author nothing.");
    if (Plan.ActionPath.IsEmpty() && !bRegister)
    {
        return false;
    }
    Plan.InputId = GetJsonStringField(Input, TEXT("id"), TEXT("input"));
    OutCode = TEXT("INPUT_NOT_SUPPORTED");
    OutError = TEXT("The Enhanced Input Blueprint nodes (module InputBlueprintNodes) are not loaded in this editor.");
    if (!Plan.ActionPath.IsEmpty())
    {
        if (!McpBlueprintGraphHandlers::FindNodeClassByName(TEXT("K2Node_EnhancedInputAction")))
        {
            return false;
        }
        TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
        Node->SetStringField(TEXT("edit"), TEXT("create_node"));
        Node->SetStringField(TEXT("id"), Plan.InputId);
        Node->SetStringField(TEXT("nodeType"), TEXT("K2Node_EnhancedInputAction"));
        Node->SetStringField(TEXT("inputActionPath"), Plan.ActionPath);
        AddOp(Plan, Node, TEXT("input"));
    }
    OutCode = TEXT("INVALID_RECIPE");
    const bool bPawn = Blueprint->ParentClass->IsChildOf(APawn::StaticClass());
    if (bRegister && !bRegistered && !AppendRegistration(Plan, bPawn, OutError))
    {
        return false;
    }
    Plan.Input->SetStringField(TEXT("inputActionPath"), Plan.ActionPath);
    Plan.Input->SetStringField(TEXT("mappingContextPath"), Plan.ContextPath);
    Plan.Input->SetStringField(TEXT("key"), Plan.Key);
    Plan.Input->SetBoolField(TEXT("contextRegistered"), bRegister);
    Plan.Input->SetBoolField(TEXT("registrationAdded"), bRegister && !bRegistered);
    if (!bRegister)
    {
        Plan.Input->SetStringField(TEXT("note"), TEXT("This Actor fires the input only after EnableInput(actor, player "
                                                      "controller) or with Auto Receive Input set, and only while the "
                                                      "possessed Pawn or PlayerController registers the mapping context."));
    }
    return true;
}

bool CreateInputAssets(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FPlan& Plan,
                       FString& OutError, FString& OutCode)
{
    TArray<FString> Created;
    const bool bCreated =
        (!Plan.bCreateAction || CreateInputAsset(Bridge, RequestId, false, Plan.ActionPath, Created, OutError, OutCode)) &&
        (!Plan.bCreateContext || CreateInputAsset(Bridge, RequestId, true, Plan.ContextPath, Created, OutError, OutCode));
    if (Plan.Input.IsValid())
    {
        // Listed even when the second one failed: created assets are kept, never rolled back.
        TArray<TSharedPtr<FJsonValue>> Paths;
        for (const FString& Path : Created)
        {
            Paths.Add(MakeShared<FJsonValueString>(Path));
        }
        Plan.Input->SetArrayField(TEXT("createdAssets"), Paths);
    }
    return bCreated;
}

bool EnsureKeyMapping(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FPlan& Plan,
                      FString& OutError)
{
    if (Plan.Key.IsEmpty() || Plan.ActionPath.IsEmpty())
    {
        return true;
    }
    FString Normalized;
    UInputMappingContext* Context = McpInputHandlers::LoadInputMappingContextAsset(Plan.ContextPath, Normalized);
    UInputAction* Action = McpInputHandlers::LoadInputActionAsset(Plan.ActionPath, Normalized);
    const FKey Key = FKey(FName(*Plan.Key));
    // add_mapping on a pair that exists resets its triggers and modifiers, so a mapped pair is left alone.
    const bool bMapped = Context && Action && McpInputHandlers::FindInputMapping(Context, Action, Key);
    Plan.Input->SetBoolField(TEXT("keyAlreadyMapped"), bMapped);
    if (bMapped)
    {
        Plan.Input->SetBoolField(TEXT("keyMapped"), true);
        return true;
    }
    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("contextPath"), Plan.ContextPath);
    Payload->SetStringField(TEXT("actionPath"), Plan.ActionPath);
    Payload->SetStringField(TEXT("key"), Plan.Key);
    const FString Id = RequestId + TEXT("#keymapping");
    FMcpResponseCaptureRegistry::Get().Begin(Id);
    McpInputHandlers::HandleAddInputMapping(Bridge, Id, TEXT("add_mapping"), Payload, nullptr);
    const FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(Id);
    Plan.Input->SetBoolField(TEXT("keyMapped"), Reply.bSuccess);
    OutError = FString::Printf(TEXT("Mapping key %s to %s failed: %s"), *Plan.Key, *Plan.ActionPath, *Reply.Message);
    return Reply.bSuccess;
}
} // namespace McpBlueprintBehaviour::Detail
