// The recipe's input section: one Enhanced Input action node, the mapping-context
// registration a Pawn or PlayerController needs before any key fires (made through
// the same hook code as every behaviour), and the key mapping, done last because
// the mapping context is another asset.
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"

#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "Domains/Input/McpInputActionRouting.h"
#include "Domains/Input/McpInputAssetResolve.h"
#include "Domains/Input/McpInputKeyResolve.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/McpCapabilityPrincipal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/PackageName.h"

namespace McpBlueprintBehaviour::Detail
{
namespace
{
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
    // Names only the path that is wrong ("'' is not an Input Action" blamed an action left out on purpose) and the fix.
    OutCode = TEXT("ASSET_NOT_FOUND");
    OutError = !ActionArg.IsEmpty() && !Action
        ? FString::Printf(TEXT("input: '%s' is not an Input Action; leave it out to make one, or create it with "
                               "manage_networking configure_input create_action."), *ActionArg)
        : FString::Printf(TEXT("input: '%s' is not an Input Mapping Context; leave it out to use the one the Blueprint "
                               "registers (else %s), or create it with manage_networking configure_input "
                               "create_mapping_context."), *ContextArg, DefaultInputContext);
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
    Plan.ContextPath = Context ? Context->GetPathName() : McpToObjectPath(DefaultInputContext);
    if (!Action && !Plan.Key.IsEmpty())
    {
        const FString Default = DefaultActionPath(Blueprint, Plan.Tag);
        Action = McpInputHandlers::LoadInputActionAsset(Default, Normalized);
        Plan.bCreateAction = Action == nullptr;
        Plan.ActionPath = McpToObjectPath(Default);
    }
    Plan.ActionPath = Action ? Action->GetPathName() : Plan.ActionPath;
    // The context and action can come from the Blueprint's own wiring or the shared defaults rather than from paths
    // the request named: a path-restricted caller maps keys into, and creates, only assets inside its prefixes.
    OutCode = TEXT("PATH_NOT_PERMITTED");
    OutError = TEXT("input: the mapping context or input action it would use is outside the caller's allowed paths; "
                    "pass mappingContextPath and inputActionPath inside them.");
    if (!McpCapabilityPrincipal::ActiveRequestPermitsPath(Plan.ContextPath) ||
        (!Plan.ActionPath.IsEmpty() && !McpCapabilityPrincipal::ActiveRequestPermitsPath(Plan.ActionPath)))
    {
        return false;
    }
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

bool CreateInputAssets(UEngineWireSubsystem& Bridge, const FString& RequestId, const FPlan& Plan,
                       FString& OutError, FString& OutCode)
{
    TArray<FString> Created;
    const bool bCreated =
        (!Plan.bCreateAction || CreateInputAsset(Bridge, RequestId, false, Plan.ActionPath, Created, OutError, OutCode)) &&
        (!Plan.bCreateContext || CreateInputAsset(Bridge, RequestId, true, Plan.ContextPath, Created, OutError, OutCode));
    if (Plan.Input.IsValid())
    {
        // Listed even when the second one failed: created assets are kept, never rolled back.
        Plan.Input->SetArrayField(TEXT("createdAssets"), McpHandlerUtils::ToJsonStringArray(Created));
    }
    return bCreated;
}

bool EnsureKeyMapping(UEngineWireSubsystem& Bridge, const FString& RequestId, const FPlan& Plan,
                      FString& OutError)
{
    if (Plan.Key.IsEmpty() || Plan.ActionPath.IsEmpty())
    {
        return true;
    }
    bool bMapped = false;
    FString Message;
    const bool bOk = McpInputHandlers::EnsureInputMapping(Bridge, RequestId + TEXT("#keymapping"), Plan.ContextPath,
                                                          Plan.ActionPath, Plan.Key, bMapped, Message);
    Plan.Input->SetBoolField(TEXT("keyAlreadyMapped"), bMapped);
    Plan.Input->SetBoolField(TEXT("keyMapped"), bOk);
    if (!bMapped)
    {
        OutError = FString::Printf(TEXT("Mapping key %s to %s failed: %s"), *Plan.Key, *Plan.ActionPath, *Message);
    }
    return bOk;
}
} // namespace McpBlueprintBehaviour::Detail
