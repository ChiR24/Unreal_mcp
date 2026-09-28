// Input assets and project checks behind a recipe's input section: the Enhanced
// Input module and default classes, the one context resolver every caller shares,
// the mapping-context registration (a recipe file itself, Recipes/Behaviour), and
// input assets created through the Input domain's own handlers. EnhancedInput is an
// optional, delay-loaded module: nothing here touches its classes before
// EnhancedInputLoaded() said yes.
#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h"

#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersActionRouting.h"
#include "Domains/Input/McpAutomationBridge_InputHandlersAssetResolution.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "GameFramework/InputSettings.h"
#include "InputMappingContext.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

namespace McpBlueprintBehaviour
{
namespace Detail
{
bool EnhancedInputLoaded()
{
    FModuleManager& Modules = FModuleManager::Get();
    return Modules.IsModuleLoaded(TEXT("EnhancedInput")) ||
           (Modules.ModuleExists(TEXT("EnhancedInput")) && Modules.LoadModule(TEXT("EnhancedInput")) != nullptr);
}

FString DefaultActionPath(UBlueprint* Blueprint, const FString& Tag)
{
    FString Safe;
    for (const TCHAR Char : Tag)
    {
        Safe.AppendChar(FChar::IsAlnum(Char) ? Char : TEXT('_'));
    }
    return FPackageName::GetLongPackagePath(Blueprint->GetOutermost()->GetName()) + TEXT("/Input/IA_") + Safe;
}

UObject* RegisteredContext(UBlueprint* Blueprint, const UObject* Wanted, bool& bOutWanted)
{
    static const FName AddMappingContext(TEXT("AddMappingContext"));
    bOutWanted = false;
    UObject* First = nullptr;
    UClass* Generated = Blueprint->GeneratedClass;
    UObject* Defaults = Generated ? Generated->GetDefaultObject() : nullptr;
    TArray<UK2Node_CallFunction*> Calls;
    FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Calls);
    for (UK2Node_CallFunction* Call : Calls)
    {
        // The context is the pin's default, or a variable wired into the pin (the
        // templates' DefaultMappingContext), read off the class default object.
        UEdGraphPin* Pin =
            Call->FunctionReference.GetMemberName() == AddMappingContext ? Call->FindPin(TEXT("MappingContext")) : nullptr;
        const UK2Node_VariableGet* Get =
            Pin && Pin->LinkedTo.Num() == 1 ? Cast<UK2Node_VariableGet>(Pin->LinkedTo[0]->GetOwningNode()) : nullptr;
        const FObjectPropertyBase* Variable =
            Get && Defaults ? FindFProperty<FObjectPropertyBase>(Generated, Get->GetVarName()) : nullptr;
        UObject* Context = Variable ? Variable->GetObjectPropertyValue_InContainer(Defaults)
                                    : (Pin ? Pin->DefaultObject.Get() : nullptr);
        if (Cast<UInputMappingContext>(Context))
        {
            First = First ? First : Context;
            bOutWanted |= Context == Wanted;
        }
    }
    return First;
}

bool CheckDefaultInputClasses(const TSharedPtr<FJsonObject>& Input, const TSharedPtr<FJsonObject>& Report,
                              FString& OutError, FString& OutCode)
{
    UClass* PlayerInput = UInputSettings::GetDefaultPlayerInputClass();
    UClass* Component = UInputSettings::GetDefaultInputComponentClass();
    Input->SetStringField(TEXT("defaultPlayerInputClass"), PlayerInput ? PlayerInput->GetPathName() : FString());
    Input->SetStringField(TEXT("defaultInputComponentClass"), Component ? Component->GetPathName() : FString());
    const bool bPlayerInput = PlayerInput && PlayerInput->IsChildOf(UEnhancedPlayerInput::StaticClass());
    if (bPlayerInput && Component && Component->IsChildOf(UEnhancedInputComponent::StaticClass()))
    {
        return true;
    }
    // Legacy classes (UE 5.0 and projects upgraded from UE4): the node compiles and never fires.
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("section"), TEXT("/Script/Engine.InputSettings"));
    Params->SetStringField(TEXT("key"), bPlayerInput ? TEXT("DefaultInputComponentClass") : TEXT("DefaultPlayerInputClass"));
    Params->SetStringField(TEXT("value"), bPlayerInput ? TEXT("/Script/EnhancedInput.EnhancedInputComponent")
                                                       : TEXT("/Script/EnhancedInput.EnhancedPlayerInput"));
    TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
    Next->SetStringField(TEXT("operation"), TEXT("execute"));
    Next->SetStringField(TEXT("tool"), TEXT("system_control"));
    Next->SetStringField(TEXT("action"), TEXT("set_project_setting"));
    Next->SetObjectField(TEXT("params"), Params);
    Report->SetObjectField(TEXT("nextCall"), Next);
    OutCode = TEXT("ENHANCED_INPUT_NOT_DEFAULT");
    OutError = TEXT("This project's default input classes are not Enhanced Input, so an Enhanced Input node would compile "
                    "and never fire. Set DefaultPlayerInputClass to /Script/EnhancedInput.EnhancedPlayerInput and "
                    "DefaultInputComponentClass to /Script/EnhancedInput.EnhancedInputComponent in section "
                    "/Script/Engine.InputSettings (system_control set_project_setting; nextCall sets the first), then retry.");
    return false;
}

bool AppendRegistration(FPlan& Plan, bool bPawn, FString& OutError)
{
    TMap<FString, TSharedPtr<FJsonValue>> Values;
    Values.Add(TEXT("MappingContext"), MakeShared<FJsonValueString>(Plan.ContextPath));
    const TSharedPtr<FJsonObject> Registration = LoadRecipe(
        TEXT("Behaviour"), bPawn ? TEXT("InputRegistrationPawn") : TEXT("InputRegistrationController"), Values, OutError);
    TArray<TSharedPtr<FJsonObject>> Steps;
    if (!Registration || !ObjectsIn(Registration, TEXT("hooks"), Plan.Hooks, OutError) ||
        !ObjectsIn(Registration, TEXT("eventGraph"), Steps, OutError))
    {
        return false;
    }
    for (const TSharedPtr<FJsonObject>& Step : Steps)
    {
        AddOp(Plan, CopyObject(Step), TEXT("input registration"));
        const FString Id = GetJsonStringField(Step, TEXT("id"));
        if (!Id.IsEmpty())
        {
            Plan.SharedStepTags.Add(Id, InputContextTag);
        }
    }
    return true;
}

bool CreateInputAsset(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, bool bContext, const FString& Path,
                      TArray<FString>& OutCreated, FString& OutError, FString& OutCode)
{
    int32 Dot = INDEX_NONE;
    const FString Package = Path.FindChar(TEXT('.'), Dot) ? Path.Left(Dot) : Path;
    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("name"), FPackageName::GetShortName(Package));
    Payload->SetStringField(TEXT("path"), FPackageName::GetLongPackagePath(Package));
    const FString Id = RequestId + TEXT("#inputasset");
    FMcpResponseCaptureRegistry::Get().Begin(Id);
    if (bContext)
    {
        McpInputHandlers::HandleCreateInputMappingContext(Bridge, Id, Payload, nullptr);
    }
    else
    {
        Payload->SetStringField(TEXT("valueType"), TEXT("digital"));
        McpInputHandlers::HandleCreateInputAction(Bridge, Id, Payload, nullptr);
    }
    const FMcpCapturedResponse Reply = FMcpResponseCaptureRegistry::Get().End(Id);
    if (!Reply.bSuccess)
    {
        OutCode = Reply.ErrorCode.IsEmpty() ? TEXT("INPUT_ASSET_FAILED") : *Reply.ErrorCode;
        OutError = FString::Printf(TEXT("Could not create %s: %s"), *Package, *Reply.Message);
        return false;
    }
    OutCreated.Add(Package);
    return true;
}
} // namespace Detail

bool EnsureInputAssets(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, UBlueprint* Blueprint,
                       const FString& Tag, FString& InOutActionPath, FString& InOutContextPath,
                       TArray<FString>& OutCreated, FString& OutError, FString& OutCode)
{
    OutCode = TEXT("ENHANCEDINPUT_PLUGIN_NOT_ENABLED");
    OutError = TEXT("The Enhanced Input plugin is not enabled in this project; enable it to bind input.");
    if (!Blueprint || !Detail::EnhancedInputLoaded())
    {
        return false;
    }
    bool bUnused = false;
    const UObject* Registered = Detail::RegisteredContext(Blueprint, nullptr, bUnused);
    InOutActionPath = InOutActionPath.IsEmpty() ? Detail::DefaultActionPath(Blueprint, Tag) : InOutActionPath;
    InOutContextPath = !InOutContextPath.IsEmpty() ? InOutContextPath
                       : (Registered ? Registered->GetPathName() : FString(Detail::DefaultInputContext));
    FString Normalized;
    return (McpInputHandlers::LoadInputActionAsset(InOutActionPath, Normalized) ||
            Detail::CreateInputAsset(Bridge, RequestId, false, InOutActionPath, OutCreated, OutError, OutCode)) &&
           (McpInputHandlers::LoadInputMappingContextAsset(InOutContextPath, Normalized) ||
            Detail::CreateInputAsset(Bridge, RequestId, true, InOutContextPath, OutCreated, OutError, OutCode));
}
} // namespace McpBlueprintBehaviour
