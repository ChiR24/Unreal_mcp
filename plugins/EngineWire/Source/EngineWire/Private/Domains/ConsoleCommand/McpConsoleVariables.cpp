// McpConsoleVariables.cpp — inspect get_console_variables: console variables read without running a command.
//
// console_command reads a variable when its name is typed alone, but it is a write (a command can change anything), so
// a client holding only the read scope could not see how the engine is set.
#include "Domains/ConsoleCommand/McpConsolePrivate.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Dom/JsonValue.h"
#include "HAL/IConsoleManager.h"

namespace
{
constexpr int32 MaxNames = 50;
constexpr int32 MaxNameChars = 128;
constexpr int32 MaxHelpChars = 200;

// The layer that set the value last. The enum's values shift between versions (5.3 added Hotfix, 5.8 plugin, preview
// and temp layers); the engine names them itself from 5.3, and before that each version's own enumerators are compared.
const TCHAR* SetByName(EConsoleVariableFlags Flags)
{
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 3
    return GetConsoleVariableSetByName(Flags);
#else
    switch (static_cast<uint32>(Flags) & static_cast<uint32>(ECVF_SetByMask))
    {
    case ECVF_SetByConstructor: return TEXT("Constructor");
    case ECVF_SetByScalability: return TEXT("Scalability");
    case ECVF_SetByGameSetting: return TEXT("GameSetting");
    case ECVF_SetByProjectSetting: return TEXT("ProjectSetting");
    case ECVF_SetBySystemSettingsIni: return TEXT("SystemSettingsIni");
    case ECVF_SetByDeviceProfile: return TEXT("DeviceProfile");
    case ECVF_SetByGameOverride: return TEXT("GameOverride");
    case ECVF_SetByConsoleVariablesIni: return TEXT("ConsoleVariablesIni");
    case ECVF_SetByCommandline: return TEXT("Commandline");
    case ECVF_SetByCode: return TEXT("Code");
    case ECVF_SetByConsole: return TEXT("Console");
    default: return TEXT("<UNKNOWN>");
    }
#endif
}

const TCHAR* TypeName(const IConsoleVariable& Variable)
{
    return Variable.IsVariableBool() ? TEXT("bool") : Variable.IsVariableInt() ? TEXT("int")
        : Variable.IsVariableFloat() ? TEXT("float") : Variable.IsVariableString() ? TEXT("string") : TEXT("other");
}

bool IsVariableName(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len() > MaxNameChars)
    {
        return false;
    }
    for (const TCHAR Char : Name)
    {
        if (!FChar::IsAlnum(Char) && Char != TEXT('.') && Char != TEXT('_'))
        {
            return false;
        }
    }
    return true;
}

FString FirstHelpLine(const IConsoleVariable& Variable)
{
    FString Help = Variable.GetHelp() ? FString(Variable.GetHelp()) : FString();
    int32 Break = INDEX_NONE;
    if (Help.FindChar(TEXT('\n'), Break))
    {
        Help.LeftInline(Break);
    }
    Help.TrimStartAndEndInline();
    return Help.Len() > MaxHelpChars ? Help.Left(MaxHelpChars - 3) + TEXT("...") : Help;
}
} // namespace

namespace McpConsoleVariables
{
// A read scope reaches every variable it names, so one whose name reads like a credential keeps its value back. Whole
// words of the name, split at dots, underscores and capitals (an acronym ends before the capital that starts the next
// word): net.EncryptionKey and Analytics.APIKey are, Slate.EnableKeyboardNavigation is not.
bool NamesACredential(const FString& Name)
{
    static const TCHAR* const Words[] = {TEXT("key"), TEXT("token"), TEXT("secret"), TEXT("password"), TEXT("auth"), TEXT("credential")};
    FString Word;
    const auto Matches = [&Word]()
    {
        for (const TCHAR* Candidate : Words)
        {
            if (Word.Equals(Candidate, ESearchCase::IgnoreCase) || Word.Equals(FString(Candidate) + TEXT("s"), ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    };
    for (int32 Index = 0; Index <= Name.Len(); ++Index)
    {
        const TCHAR Char = Index < Name.Len() ? Name[Index] : TEXT('.');
        const bool bAfterLower = Index > 0 && !FChar::IsUpper(Name[Index - 1]);
        const bool bBeforeLower = Index + 1 < Name.Len() && FChar::IsLower(Name[Index + 1]);
        const bool bStartsWord = FChar::IsUpper(Char) && Index > 0 && (bAfterLower || bBeforeLower);
        if (Char == TEXT('.') || Char == TEXT('_') || bStartsWord)
        {
            if (Matches())
            {
                return true;
            }
            Word.Reset();
        }
        if (Char != TEXT('.') && Char != TEXT('_'))
        {
            Word.AppendChar(Char);
        }
    }
    return false;
}

bool HandleGet(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
               TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
    if (!Payload->TryGetArrayField(TEXT("names"), Names) || !Names || Names->Num() == 0 || Names->Num() > MaxNames)
    {
        Bridge.SendAutomationError(Socket, RequestId,
                                   FString::Printf(TEXT("names lists 1 to %d console variable names."), MaxNames),
                                   TEXT("INVALID_ARGUMENT"));
        return true;
    }
    TArray<TSharedPtr<FJsonValue>> Variables;
    TArray<TSharedPtr<FJsonValue>> NotFound;
    for (const TSharedPtr<FJsonValue>& Entry : *Names)
    {
        const FString Name = Entry.IsValid() && Entry->Type == EJson::String ? Entry->AsString().TrimStartAndEnd() : FString();
        if (!IsVariableName(Name))
        {
            Bridge.SendAutomationError(Socket, RequestId,
                                       FString::Printf(TEXT("'%s' is not a console variable name: letters, digits, dots and underscores, as r.ScreenPercentage."), *Name),
                                       TEXT("INVALID_ARGUMENT"));
            return true;
        }
        const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(*Name, false);
        if (!Variable)
        {
            NotFound.Add(MakeShared<FJsonValueString>(Name));
            continue;
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Name);
        if (NamesACredential(Name))
        {
            Row->SetBoolField(TEXT("withheld"), true);
        }
        else
        {
            Row->SetStringField(TEXT("value"), Variable->GetString());
        }
        Row->SetStringField(TEXT("type"), TypeName(*Variable));
        Row->SetStringField(TEXT("setBy"), SetByName(Variable->GetFlags()));
        Row->SetStringField(TEXT("help"), FirstHelpLine(*Variable));
        Variables.Add(MakeShared<FJsonValueObject>(Row));
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("variables"), Variables);
    Result->SetArrayField(TEXT("notFound"), NotFound);
    const FString Message = NotFound.Num() == 0
        ? FString::Printf(TEXT("Read %d console variable(s)."), Variables.Num())
        : FString::Printf(TEXT("Read %d console variable(s); %d name(s) match none (notFound)."), Variables.Num(), NotFound.Num());
    Bridge.SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}
} // namespace McpConsoleVariables
