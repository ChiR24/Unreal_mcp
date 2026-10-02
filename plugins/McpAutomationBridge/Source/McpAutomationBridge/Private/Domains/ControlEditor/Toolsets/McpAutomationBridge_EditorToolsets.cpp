#include "Domains/ControlEditor/Toolsets/McpAutomationBridge_EditorToolsets.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpReflectedInvoke.h"

#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

namespace McpEditorToolsets
{
namespace
{
const TCHAR* const UnavailableMessage =
    TEXT("Epic's editor toolsets are not loaded. They ship with Unreal Engine 5.8: enable the Toolset Registry plugin ")
    TEXT("and the toolsets to expose (All Toolsets enables every one), then restart the editor.");

// True when Tool starts with one of Verbs as a whole word: "Get" matches GetActors, not Getaway.
bool StartsWithVerb(const FString& Tool, std::initializer_list<const TCHAR*> Verbs)
{
    for (const TCHAR* Verb : Verbs)
    {
        const int32 Len = FCString::Strlen(Verb);
        if (Tool.StartsWith(Verb, ESearchCase::CaseSensitive) && (Tool.Len() == Len || !FChar::IsLower(Tool[Len])))
        {
            return true;
        }
    }
    return false;
}

// Every word of Query (any case) appears in the tool's name or description.
bool MatchesQuery(const FString& Name, const FString& Description, const TArray<FString>& Words)
{
    for (const FString& Word : Words)
    {
        if (!Name.Contains(Word) && !Description.Contains(Word))
        {
            return false;
        }
    }
    return true;
}
} // namespace

EToolClass ClassifyTool(const FString& FullToolName)
{
    FString Toolset;
    FString Tool;
    if (!FullToolName.Split(TEXT("."), &Toolset, &Tool, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
    {
        Tool = FullToolName;
    }
    const FString Lower = FullToolName.ToLower();
    if (Lower.Contains(TEXT("python")) || Lower.Contains(TEXT("console")) || Lower.Contains(TEXT("shell")))
    {
        return EToolClass::Blocked;
    }
    // ConfigSettingsToolset reads and writes any settings section, this plugin's own included: its capability token
    // and its token and loopback switches. Project settings go through system_control, which refuses that section.
    if (Lower.StartsWith(TEXT("configsettingstoolset.")))
    {
        return EToolClass::Blocked;
    }
    if (StartsWithVerb(Tool, {TEXT("Delete"), TEXT("Remove"), TEXT("Destroy"), TEXT("Clear"), TEXT("Reset"),
                              TEXT("Purge"), TEXT("Discard"), TEXT("Revert"), TEXT("Wipe"), TEXT("Erase"), TEXT("Kill"),
                              TEXT("Drop"), TEXT("Uninstall"), TEXT("Unregister")}))
    {
        return EToolClass::Destructive;
    }
    if (StartsWithVerb(Tool, {TEXT("Get"), TEXT("List"), TEXT("Find"), TEXT("Search"), TEXT("Is"), TEXT("Has"),
                              TEXT("Can"), TEXT("Read"), TEXT("Query"), TEXT("Describe"), TEXT("Inspect"), TEXT("Count"),
                              TEXT("Validate"), TEXT("Capture"), TEXT("Resolve"), TEXT("Parse"), TEXT("Extract")}))
    {
        return EToolClass::Read;
    }
    return EToolClass::Write;
}

const TCHAR* ToolClassName(EToolClass Class)
{
    switch (Class)
    {
    case EToolClass::Read: return TEXT("read");
    case EToolClass::Destructive: return TEXT("destructive");
    case EToolClass::Blocked: return TEXT("blocked");
    default: return TEXT("write");
    }
}

bool ReadToolsets(TArray<TSharedPtr<FJsonValue>>& OutToolsets, FString& OutError)
{
    UClass* Registry = FindObject<UClass>(nullptr, TEXT("/Script/ToolsetRegistry.ToolsetRegistry"));
    UFunction* Function = Registry ? Registry->FindFunctionByName(TEXT("GetAllToolsetJsonSchemas")) : nullptr;
    const FStrProperty* Return = Function ? CastField<FStrProperty>(Function->GetReturnProperty()) : nullptr;
    if (!Return)
    {
        OutError = UnavailableMessage;
        return false;
    }
    FMcpScopedParamBlock Block(Function);
    Registry->GetDefaultObject()->ProcessEvent(Function, Block.Data());
    const FString Json = Return->GetPropertyValue_InContainer(Block.Data());
    OutToolsets.Reset();
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (Json.IsEmpty() || !FJsonSerializer::Deserialize(Reader, OutToolsets))
    {
        // An empty answer is the registry's own "no editor subsystem" case.
        OutError = Json.IsEmpty() ? FString(UnavailableMessage) : TEXT("The toolset registry answered with schemas that are not a JSON list.");
        return false;
    }
    return true;
}

TSharedPtr<FJsonObject> FindTool(const TArray<TSharedPtr<FJsonValue>>& Toolsets, const FString& FullToolName)
{
    for (const TSharedPtr<FJsonValue>& Value : Toolsets)
    {
        const TSharedPtr<FJsonObject>* Toolset = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
        if (!Value.IsValid() || !Value->TryGetObject(Toolset) || !(*Toolset)->TryGetArrayField(TEXT("tools"), Tools))
        {
            continue;
        }
        for (const TSharedPtr<FJsonValue>& ToolValue : *Tools)
        {
            const TSharedPtr<FJsonObject>* Tool = nullptr;
            FString Name;
            if (ToolValue.IsValid() && ToolValue->TryGetObject(Tool) && (*Tool)->TryGetStringField(TEXT("name"), Name) &&
                Name.Equals(FullToolName, ESearchCase::IgnoreCase))
            {
                return *Tool;
            }
        }
    }
    return nullptr;
}

bool HandleListToolsets(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TArray<TSharedPtr<FJsonValue>> Toolsets;
    FString Error;
    if (!ReadToolsets(Toolsets, Error))
    {
        Self->SendAutomationError(Socket, RequestId, Error, TEXT("EDITOR_TOOLSETS_UNAVAILABLE"));
        return true;
    }
    FString ToolName, Query, OnlyToolset;
    Payload->TryGetStringField(TEXT("toolName"), ToolName);
    Payload->TryGetStringField(TEXT("query"), Query);
    Payload->TryGetStringField(TEXT("toolset"), OnlyToolset);
    double LimitValue = 50.0;
    Payload->TryGetNumberField(TEXT("limit"), LimitValue);
    const int32 Limit = FMath::Clamp(static_cast<int32>(LimitValue), 1, 200);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (!ToolName.IsEmpty())
    {
        const TSharedPtr<FJsonObject> Tool = FindTool(Toolsets, ToolName);
        if (!Tool.IsValid())
        {
            Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No registered toolset has a tool named '%s'. "
                "List the tools without toolName (query narrows them) and copy a name."), *ToolName), TEXT("EDITOR_TOOL_NOT_FOUND"));
            return true;
        }
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->Values = Tool->Values;
        Entry->SetStringField(TEXT("effect"), ToolClassName(ClassifyTool(ToolName)));
        Result->SetObjectField(TEXT("tool"), Entry);
        Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Schema of %s"), *ToolName), Result);
        return true;
    }

    TArray<FString> Words;
    Query.ParseIntoArrayWS(Words);
    TArray<TSharedPtr<FJsonValue>> ToolsetRows;
    TArray<TSharedPtr<FJsonValue>> ToolRows;
    int32 Matched = 0;
    for (const TSharedPtr<FJsonValue>& Value : Toolsets)
    {
        const TSharedPtr<FJsonObject>* Toolset = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
        FString Name;
        if (!Value.IsValid() || !Value->TryGetObject(Toolset) || !(*Toolset)->TryGetStringField(TEXT("name"), Name) ||
            !(*Toolset)->TryGetArrayField(TEXT("tools"), Tools))
        {
            continue;
        }
        FString ToolsetDescription;
        (*Toolset)->TryGetStringField(TEXT("description"), ToolsetDescription);
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Name);
        Row->SetStringField(TEXT("description"), ToolsetDescription);
        Row->SetNumberField(TEXT("toolCount"), Tools->Num());
        ToolsetRows.Add(MakeShared<FJsonValueObject>(Row));
        if (!OnlyToolset.IsEmpty() && !Name.Equals(OnlyToolset, ESearchCase::IgnoreCase))
        {
            continue;
        }
        for (const TSharedPtr<FJsonValue>& ToolValue : *Tools)
        {
            const TSharedPtr<FJsonObject>* Tool = nullptr;
            FString ToolFull, Description;
            if (!ToolValue.IsValid() || !ToolValue->TryGetObject(Tool) || !(*Tool)->TryGetStringField(TEXT("name"), ToolFull))
            {
                continue;
            }
            (*Tool)->TryGetStringField(TEXT("description"), Description);
            if (!MatchesQuery(ToolFull, Description, Words) || ++Matched > Limit)
            {
                continue;
            }
            TSharedPtr<FJsonObject> ToolRow = MakeShared<FJsonObject>();
            ToolRow->SetStringField(TEXT("name"), ToolFull);
            ToolRow->SetStringField(TEXT("description"), Description.Left(240));
            ToolRow->SetStringField(TEXT("effect"), ToolClassName(ClassifyTool(ToolFull)));
            ToolRows.Add(MakeShared<FJsonValueObject>(ToolRow));
        }
    }
    Result->SetArrayField(TEXT("toolsets"), ToolsetRows);
    Result->SetArrayField(TEXT("tools"), ToolRows);
    Result->SetNumberField(TEXT("matchedTools"), Matched);
    Result->SetBoolField(TEXT("hasMore"), Matched > ToolRows.Num());
    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%d toolsets; %d tools matched, %d listed"), ToolsetRows.Num(), Matched, ToolRows.Num()), Result);
    return true;
}
} // namespace McpEditorToolsets
