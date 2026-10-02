// McpAutomationBridge_EditorToolsetsCall.cpp — call_editor_tool(_destructive): one tool of Epic's toolset registry,
// run through UToolsetRegistry::ExecuteTool (reflection) and answered when its async result settles.
#include "Domains/ControlEditor/Toolsets/McpAutomationBridge_EditorToolsets.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpReflectedInvoke.h"

#include "Containers/Ticker.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformTime.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace McpEditorToolsets
{
namespace
{
// A tool that has not settled after this long is reported as timed out; its work may still finish in the editor.
constexpr double MaxWaitSeconds = 600.0;

// The settled state of Epic's UToolCallAsyncResultString: false while the call runs. Value is the tool's JSON output.
bool ReadSettled(UObject* Pending, bool& bOutOk, FString& OutValue, FString& OutError)
{
    UClass* Class = Pending ? Pending->GetClass() : nullptr;
    const FBoolProperty* Complete = Class ? FindFProperty<FBoolProperty>(Class, TEXT("bIsComplete")) : nullptr;
    if (!Complete || !Complete->GetPropertyValue_InContainer(Pending))
    {
        return false;
    }
    const FStrProperty* ErrorField = FindFProperty<FStrProperty>(Class, TEXT("Error"));
    const FStrProperty* ValueField = FindFProperty<FStrProperty>(Class, TEXT("Value"));
    OutError = ErrorField ? ErrorField->GetPropertyValue_InContainer(Pending) : FString();
    OutValue = ValueField ? ValueField->GetPropertyValue_InContainer(Pending) : FString();
    bOutOk = OutError.IsEmpty();
    return true;
}

void Reply(UMcpAutomationBridgeSubsystem* Self, const TSharedPtr<FMcpBridgeWebSocket>& Socket, const FString& RequestId,
           const FString& ToolName, EToolClass Class, bool bOk, const FString& Value, const FString& Error)
{
    if (!bOk)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s failed: %s"), *ToolName, *Error),
                                  TEXT("EDITOR_TOOL_FAILED"));
        return;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("toolName"), ToolName);
    Result->SetStringField(TEXT("effect"), ToolClassName(Class));
    TSharedPtr<FJsonValue> Output;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Value);
    if (!Value.IsEmpty() && FJsonSerializer::Deserialize(Reader, Output) && Output.IsValid())
    {
        Result->SetField(TEXT("output"), Output);
    }
    else
    {
        Result->SetStringField(TEXT("output"), Value);
    }
    Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Ran %s"), *ToolName), Result);
}

// Starts the call and returns Epic's pending result object, or null with OutError.
UObject* StartCall(const FString& Toolset, const FString& Tool, const FString& InputJson, FString& OutError)
{
    UClass* Registry = FindObject<UClass>(nullptr, TEXT("/Script/ToolsetRegistry.ToolsetRegistry"));
    UFunction* Execute = Registry ? Registry->FindFunctionByName(TEXT("ExecuteTool")) : nullptr;
    const FObjectPropertyBase* Return = Execute ? CastField<FObjectPropertyBase>(Execute->GetReturnProperty()) : nullptr;
    if (!Return)
    {
        OutError = TEXT("The toolset registry has no ExecuteTool function in this engine build.");
        return nullptr;
    }
    FMcpScopedParamBlock Block(Execute);
    TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
    Args->SetStringField(TEXT("ToolsetName"), Toolset);
    Args->SetStringField(TEXT("ToolName"), Tool);
    Args->SetStringField(TEXT("JsonInput"), InputJson);
    TArray<TSharedPtr<FJsonValue>> Unset;
    if (!McpBindJsonArgsToParams(Execute, Args, Block.Data(), Unset, OutError))
    {
        return nullptr;
    }
    Registry->GetDefaultObject()->ProcessEvent(Execute, Block.Data());
    UObject* Pending = Return->GetObjectPropertyValue_InContainer(Block.Data());
    if (!Pending)
    {
        OutError = TEXT("The toolset registry returned no result object.");
    }
    return Pending;
}
} // namespace

bool HandleCallTool(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                    bool bDestructiveAllowed)
{
    FString Requested;
    if (!Payload->TryGetStringField(TEXT("toolName"), Requested) || Requested.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("toolName is required: the full Toolset.Tool name that list_editor_toolsets shows."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    TArray<TSharedPtr<FJsonValue>> Toolsets;
    FString Error;
    if (!ReadToolsets(Toolsets, Error))
    {
        Self->SendAutomationError(Socket, RequestId, Error, TEXT("EDITOR_TOOLSETS_UNAVAILABLE"));
        return true;
    }
    const TSharedPtr<FJsonObject> Schema = FindTool(Toolsets, Requested);
    FString ToolName;
    if (!Schema.IsValid() || !Schema->TryGetStringField(TEXT("name"), ToolName))
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No registered toolset has a tool named '%s'. "
            "list_editor_toolsets lists them (query narrows the list)."), *Requested), TEXT("EDITOR_TOOL_NOT_FOUND"));
        return true;
    }
    const EToolClass Class = ClassifyTool(ToolName);
    if (Class == EToolClass::Blocked)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s is not called: it would run code outside this "
            "plugin's console and script policy."), *ToolName), TEXT("EDITOR_TOOL_BLOCKED"));
        return true;
    }
    if (Class == EToolClass::Destructive && !bDestructiveAllowed)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s deletes or resets editor state (its name starts "
            "with a destructive verb). Run it with control_editor.call_editor_tool_destructive, which needs elevated consent."),
            *ToolName), TEXT("DESTRUCTIVE_EDITOR_TOOL"));
        return true;
    }

    FString InputJson = TEXT("{}");
    const TSharedPtr<FJsonObject>* Input = nullptr;
    if (Payload->TryGetObjectField(TEXT("input"), Input) && Input && Input->IsValid())
    {
        InputJson.Reset();
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&InputJson);
        FJsonSerializer::Serialize(Input->ToSharedRef(), Writer);
    }
    FString Toolset;
    FString Tool;
    ToolName.Split(TEXT("."), &Toolset, &Tool, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    UObject* Pending = StartCall(Toolset, Tool, InputJson, Error);
    if (!Pending)
    {
        Self->SendAutomationError(Socket, RequestId, Error, TEXT("EDITOR_TOOL_FAILED"));
        return true;
    }

    bool bOk = false;
    FString Value;
    if (ReadSettled(Pending, bOk, Value, Error))
    {
        Reply(Self, Socket, RequestId, ToolName, Class, bOk, Value, Error);
        return true;
    }
    // Epic keeps the result alive only inside its own completion callback, so hold it until it settles.
    const TSharedPtr<TStrongObjectPtr<UObject>> Held = MakeShared<TStrongObjectPtr<UObject>>(Pending);
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSelf(Self);
    const double Deadline = FPlatformTime::Seconds() + MaxWaitSeconds;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
        [WeakSelf, Held, Socket, RequestId, ToolName, Class, Deadline](float) {
            if (!WeakSelf.IsValid())
            {
                return false;
            }
            bool bSettledOk = false;
            FString SettledValue;
            FString SettledError;
            if (ReadSettled(Held->Get(), bSettledOk, SettledValue, SettledError))
            {
                Reply(WeakSelf.Get(), Socket, RequestId, ToolName, Class, bSettledOk, SettledValue, SettledError);
                return false;
            }
            if (FPlatformTime::Seconds() > Deadline)
            {
                WeakSelf->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s did not finish within %.0f s; "
                    "its work may still complete in the editor."), *ToolName, MaxWaitSeconds), TEXT("EDITOR_TOOL_TIMEOUT"));
                return false;
            }
            return true;
        }), 0.05f);
    return true;
}
} // namespace McpEditorToolsets
