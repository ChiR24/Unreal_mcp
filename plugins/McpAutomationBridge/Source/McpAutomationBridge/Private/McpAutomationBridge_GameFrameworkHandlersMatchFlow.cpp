#include "McpAutomationBridge_GameFrameworkHandlersContext.h"

namespace McpGameFrameworkHandlers
{
#if WITH_EDITOR
static void SendVariablesResponse(FActionContext& Context, UBlueprint* Blueprint, const FString& Message, int32 VarsAdded, const TSharedPtr<FJsonObject>& Config = nullptr)
{
    TSharedPtr<FJsonObject> Response = MakeBlueprintResponse(Message, Blueprint);
    Response->SetNumberField(TEXT("variablesAdded"), VarsAdded);
    if (Config.IsValid())
    {
        Response->SetObjectField(TEXT("configuration"), Config);
    }
    Context.SendSuccess(Response);
}

static bool SetupMatchStates(FActionContext& Context)
{
    if (!RequireGameModePath(Context)) return true;
    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;

    TArray<FString> StateNames;
    if (const TArray<TSharedPtr<FJsonValue>>* StatesArray = GetArrayField(Context.Payload, TEXT("states")))
    {
        for (const TSharedPtr<FJsonValue>& StateVal : *StatesArray)
        {
            if (StateVal.IsValid() && StateVal->Type == EJson::String) StateNames.Add(StateVal->AsString());
        }
    }

    int32 VarsAdded = AddVariableWithDefault(Blueprint, TEXT("CurrentMatchState"), MakeBytePinType(), TEXT("Match Flow"), TEXT("0"));

    FEdGraphPinType NamePinType = MakeNamePinType();
    NamePinType.ContainerType = EPinContainerType::Array;
    VarsAdded += AddVariable(Blueprint, TEXT("MatchStateNames"), NamePinType, TEXT("Match Flow"));
    VarsAdded += AddVariable(Blueprint, TEXT("PreviousMatchState"), MakeBytePinType(), TEXT("Match Flow"));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("bMatchInProgress"), MakeBoolPinType(), TEXT("Match Flow"), TEXT("false"));
    VarsAdded += AddVariable(Blueprint, TEXT("MatchStartTime"), MakeFloatPinType(), TEXT("Match Flow"));
    VarsAdded += AddVariable(Blueprint, TEXT("MatchElapsedTime"), MakeFloatPinType(), TEXT("Match Flow"));

    FinishBlueprintMutation(Blueprint, Context.bSave);

    TSharedPtr<FJsonObject> Response = MakeBlueprintResponse(
        FString::Printf(TEXT("Added %d match state variables to Blueprint"), VarsAdded),
        Blueprint);
    Response->SetNumberField(TEXT("stateCount"), StateNames.Num());
    Response->SetNumberField(TEXT("variablesAdded"), VarsAdded);

    TArray<TSharedPtr<FJsonValue>> StatesJsonArray;
    for (const FString& StateName : StateNames)
    {
        StatesJsonArray.Add(MakeShared<FJsonValueString>(StateName));
    }
    Response->SetArrayField(TEXT("configuredStates"), StatesJsonArray);
    Context.SendSuccess(Response);
    return true;
}

static bool ConfigureRoundSystem(FActionContext& Context)
{
    if (!RequireGameModePath(Context)) return true;
    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;

    const int32 NumRounds = static_cast<int32>(GetNumberField(Context.Payload, TEXT("numRounds"), 0));
    const double RoundTime = GetNumberField(Context.Payload, TEXT("roundTime"), 0);
    const double IntermissionTime = GetNumberField(Context.Payload, TEXT("intermissionTime"), 0);
    UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configuring round system: rounds=%d, roundTime=%.1f, intermission=%.1f"), NumRounds, RoundTime, IntermissionTime);

    int32 VarsAdded = AddVariableWithDefault(Blueprint, TEXT("NumRounds"), MakeIntPinType(), TEXT("Round System"), FString::FromInt(NumRounds));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("CurrentRound"), MakeIntPinType(), TEXT("Round System"), TEXT("0"));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("RoundTime"), MakeFloatPinType(), TEXT("Round System"), FString::SanitizeFloat(RoundTime));
    VarsAdded += AddVariable(Blueprint, TEXT("RoundTimeRemaining"), MakeFloatPinType(), TEXT("Round System"));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("IntermissionTime"), MakeFloatPinType(), TEXT("Round System"), FString::SanitizeFloat(IntermissionTime));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("bIsInIntermission"), MakeBoolPinType(), TEXT("Round System"), TEXT("false"));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("bRoundInProgress"), MakeBoolPinType(), TEXT("Round System"), TEXT("false"));

    FinishBlueprintMutation(Blueprint, Context.bSave);

    TSharedPtr<FJsonObject> ConfigObj = McpHandlerUtils::CreateResultObject();
    ConfigObj->SetNumberField(TEXT("numRounds"), NumRounds);
    ConfigObj->SetNumberField(TEXT("roundTime"), RoundTime);
    ConfigObj->SetNumberField(TEXT("intermissionTime"), IntermissionTime);
    SendVariablesResponse(Context, Blueprint, FString::Printf(TEXT("Added %d round system variables to Blueprint"), VarsAdded), VarsAdded, ConfigObj);
    return true;
}

static bool ConfigureTeamSystem(FActionContext& Context)
{
    if (!RequireGameModePath(Context)) return true;
    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;

    const int32 NumTeams = static_cast<int32>(GetNumberField(Context.Payload, TEXT("numTeams"), 2));
    const int32 TeamSize = static_cast<int32>(GetNumberField(Context.Payload, TEXT("teamSize"), 0));
    const bool bAutoBalance = GetBoolField(Context.Payload, TEXT("autoBalance"), true);
    const bool bFriendlyFire = GetBoolField(Context.Payload, TEXT("friendlyFire"), false);
    UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configuring team system: teams=%d, size=%d, autoBalance=%d, friendlyFire=%d"), NumTeams, TeamSize, bAutoBalance, bFriendlyFire);

    int32 VarsAdded = AddVariableWithDefault(Blueprint, TEXT("NumTeams"), MakeIntPinType(), TEXT("Team System"), FString::FromInt(NumTeams));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("MaxTeamSize"), MakeIntPinType(), TEXT("Team System"), FString::FromInt(TeamSize));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("bAutoBalance"), MakeBoolPinType(), TEXT("Team System"), bAutoBalance ? TEXT("true") : TEXT("false"));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("bFriendlyFire"), MakeBoolPinType(), TEXT("Team System"), bFriendlyFire ? TEXT("true") : TEXT("false"));
    FEdGraphPinType IntArrayPinType = MakeIntPinType();
    IntArrayPinType.ContainerType = EPinContainerType::Array;
    VarsAdded += AddVariable(Blueprint, TEXT("TeamScores"), IntArrayPinType, TEXT("Team System"));
    VarsAdded += AddVariable(Blueprint, TEXT("TeamPlayerCounts"), IntArrayPinType, TEXT("Team System"));

    FinishBlueprintMutation(Blueprint, Context.bSave);

    TSharedPtr<FJsonObject> ConfigObj = McpHandlerUtils::CreateResultObject();
    ConfigObj->SetNumberField(TEXT("numTeams"), NumTeams);
    ConfigObj->SetNumberField(TEXT("teamSize"), TeamSize);
    ConfigObj->SetBoolField(TEXT("autoBalance"), bAutoBalance);
    ConfigObj->SetBoolField(TEXT("friendlyFire"), bFriendlyFire);
    SendVariablesResponse(Context, Blueprint, FString::Printf(TEXT("Added %d team system variables to Blueprint"), VarsAdded), VarsAdded, ConfigObj);
    return true;
}

static bool ConfigureScoringSystem(FActionContext& Context)
{
    if (!RequireGameModePath(Context)) return true;
    UBlueprint* Blueprint = LoadRequiredGameMode(Context);
    if (!Blueprint) return true;

    const double ScorePerKill = GetNumberField(Context.Payload, TEXT("scorePerKill"), 100);
    const double ScorePerObjective = GetNumberField(Context.Payload, TEXT("scorePerObjective"), 500);
    const double ScorePerAssist = GetNumberField(Context.Payload, TEXT("scorePerAssist"), 50);
    const double WinScore = GetNumberField(Context.Payload, TEXT("winScore"), 0);
    const double ScorePerDeath = GetNumberField(Context.Payload, TEXT("scorePerDeath"), 0);
    UE_LOG(LogMcpGameFrameworkHandlers, Log, TEXT("Configuring scoring: kill=%.0f, objective=%.0f, assist=%.0f"), ScorePerKill, ScorePerObjective, ScorePerAssist);

    int32 VarsAdded = AddVariableWithDefault(Blueprint, TEXT("ScorePerKill"), MakeIntPinType(), TEXT("Scoring System"), FString::FromInt(static_cast<int32>(ScorePerKill)));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("ScorePerObjective"), MakeIntPinType(), TEXT("Scoring System"), FString::FromInt(static_cast<int32>(ScorePerObjective)));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("ScorePerAssist"), MakeIntPinType(), TEXT("Scoring System"), FString::FromInt(static_cast<int32>(ScorePerAssist)));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("WinScore"), MakeIntPinType(), TEXT("Scoring System"), FString::FromInt(static_cast<int32>(WinScore)));
    VarsAdded += AddVariableWithDefault(Blueprint, TEXT("ScorePerDeath"), MakeIntPinType(), TEXT("Scoring System"), FString::FromInt(static_cast<int32>(ScorePerDeath)));

    FinishBlueprintMutation(Blueprint, Context.bSave);

    TSharedPtr<FJsonObject> ConfigObj = McpHandlerUtils::CreateResultObject();
    ConfigObj->SetNumberField(TEXT("scorePerKill"), ScorePerKill);
    ConfigObj->SetNumberField(TEXT("scorePerObjective"), ScorePerObjective);
    ConfigObj->SetNumberField(TEXT("scorePerAssist"), ScorePerAssist);
    ConfigObj->SetNumberField(TEXT("winScore"), WinScore);
    ConfigObj->SetNumberField(TEXT("scorePerDeath"), ScorePerDeath);
    SendVariablesResponse(Context, Blueprint, FString::Printf(TEXT("Added %d scoring system variables to Blueprint"), VarsAdded), VarsAdded, ConfigObj);
    return true;
}

bool HandleMatchFlowAction(FActionContext& Context)
{
    if (Context.SubAction == TEXT("setup_match_states")) return SetupMatchStates(Context);
    if (Context.SubAction == TEXT("configure_round_system")) return ConfigureRoundSystem(Context);
    if (Context.SubAction == TEXT("configure_team_system")) return ConfigureTeamSystem(Context);
    if (Context.SubAction == TEXT("configure_scoring_system")) return ConfigureScoringSystem(Context);
    return false;
}
#endif
}
