#include "Core/Subsystem/McpAutomationBridgeSubsystemCustomHandlerAliases.h"

#include "McpAutomationBridgeSubsystem.h"

namespace McpCustomHandlerAliases
{
namespace
{
bool IsLowerAsciiLetter(const TCHAR Character)
{
    return Character >= TCHAR('a') && Character <= TCHAR('z');
}

bool IsAsciiDigit(const TCHAR Character)
{
    return Character >= TCHAR('0') && Character <= TCHAR('9');
}
}

bool IsValidActionIdentifier(const FString& Action)
{
    if (Action.IsEmpty() || Action.Len() > 96 || !IsLowerAsciiLetter(Action[0]))
    {
        return false;
    }

    for (int32 Index = 0; Index < Action.Len(); ++Index)
    {
        const TCHAR Character = Action[Index];
        if (!IsLowerAsciiLetter(Character) &&
            !IsAsciiDigit(Character) &&
            Character != TCHAR('_'))
        {
            return false;
        }
    }
    return true;
}

bool ReadAliasEntry(
    const TSharedPtr<FJsonObject>& EntryObject,
    FActionAliasEntry& OutEntry,
    FString& OutError)
{
    if (!EntryObject.IsValid())
    {
        OutError = TEXT("entry is not an object");
        return false;
    }

    FString AliasAction;
    FString TargetAction;
    EntryObject->TryGetStringField(TEXT("alias"), AliasAction);
    EntryObject->TryGetStringField(TEXT("target"), TargetAction);

    if (!IsValidActionIdentifier(AliasAction))
    {
        OutError = FString::Printf(
            TEXT("alias '%s' is not a lower_snake_case identifier"),
            *AliasAction);
        return false;
    }
    if (!IsValidActionIdentifier(TargetAction))
    {
        OutError = FString::Printf(
            TEXT("target '%s' is not a lower_snake_case identifier"),
            *TargetAction);
        return false;
    }
    if (AliasAction == TargetAction)
    {
        OutError = FString::Printf(
            TEXT("alias '%s' targets itself"),
            *AliasAction);
        return false;
    }

    OutEntry.AliasAction = MoveTemp(AliasAction);
    OutEntry.TargetAction = MoveTemp(TargetAction);
    return true;
}
}

bool UMcpAutomationBridgeSubsystem::RegisterHandler(
    const FString& Action,
    FAutomationHandler Handler)
{
    if (!McpCustomHandlerAliases::IsValidActionIdentifier(Action))
    {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Warning,
            TEXT("Skipping automation handler with invalid action '%s'."),
            *Action);
        return false;
    }
    if (!Handler)
    {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Warning,
            TEXT("Skipping automation handler '%s' with an empty callback."),
            *Action);
        return false;
    }
    if (AutomationHandlers.Contains(Action) || AutomationAliasActions.Contains(Action))
    {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Warning,
            TEXT("Skipping duplicate automation handler action '%s'."),
            *Action);
        return false;
    }

    AutomationHandlers.Add(Action, MoveTemp(Handler));
    TryActivatePendingActionAliases(Action);
    return true;
}

bool UMcpAutomationBridgeSubsystem::RegisterActionAlias(
    const FString& AliasAction,
    const FString& TargetAction)
{
    return RegisterActionAliasInternal(AliasAction, TargetAction, false);
}

bool UMcpAutomationBridgeSubsystem::RegisterActionAliasInternal(
    const FString& AliasAction,
    const FString& TargetAction,
    const bool bAllowPendingTarget)
{
    if (!McpCustomHandlerAliases::IsValidActionIdentifier(AliasAction) ||
        !McpCustomHandlerAliases::IsValidActionIdentifier(TargetAction) ||
        AliasAction == TargetAction)
    {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Warning,
            TEXT("Skipping invalid automation action alias '%s' -> '%s'."),
            *AliasAction,
            *TargetAction);
        return false;
    }
    if (AutomationHandlers.Contains(AliasAction) ||
        AutomationAliasActions.Contains(AliasAction) ||
        PendingAutomationActionAliases.Contains(AliasAction))
    {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Warning,
            TEXT("Skipping duplicate automation action alias '%s'."),
            *AliasAction);
        return false;
    }
    if (AutomationAliasActions.Contains(TargetAction) ||
        PendingAutomationActionAliases.Contains(TargetAction))
    {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Warning,
            TEXT("Skipping automation action alias '%s' because target '%s' is an alias."),
            *AliasAction,
            *TargetAction);
        return false;
    }
    if (TargetAction == TEXT("inspect") || TargetAction == TEXT("manage_tools"))
    {
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Warning,
            TEXT("Skipping automation action alias '%s' because target '%s' is protected."),
            *AliasAction,
            *TargetAction);
        return false;
    }

    const FAutomationHandler* TargetHandler = AutomationHandlers.Find(TargetAction);
    if (!TargetHandler)
    {
        if (!bAllowPendingTarget)
        {
            UE_LOG(
                LogMcpAutomationBridgeSubsystem,
                Warning,
                TEXT("Skipping automation action alias '%s' because target '%s' is not registered."),
                *AliasAction,
                *TargetAction);
            return false;
        }

        PendingAutomationActionAliases.Add(AliasAction, TargetAction);
        AutomationAliasActions.Add(AliasAction);
        UE_LOG(
            LogMcpAutomationBridgeSubsystem,
            Log,
            TEXT("Pending automation action alias '%s' until target '%s' is registered."),
            *AliasAction,
            *TargetAction);
        return true;
    }

    const FAutomationHandler CapturedTargetHandler = *TargetHandler;
    AutomationHandlers.Add(
        AliasAction,
        [CapturedTargetHandler, TargetAction](
            const FString& RequestId,
            const FString& Action,
            const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
        {
            (void)Action;
            return CapturedTargetHandler(
                RequestId,
                TargetAction,
                Payload,
                RequestingSocket);
        });
    AutomationAliasActions.Add(AliasAction);
    return true;
}

void UMcpAutomationBridgeSubsystem::TryActivatePendingActionAliases(
    const FString& TargetAction)
{
    TArray<FString> AliasesToActivate;
    for (const TPair<FString, FString>& PendingAlias : PendingAutomationActionAliases)
    {
        if (PendingAlias.Value == TargetAction)
        {
            AliasesToActivate.Add(PendingAlias.Key);
        }
    }

    for (const FString& AliasAction : AliasesToActivate)
    {
        PendingAutomationActionAliases.Remove(AliasAction);
        AutomationAliasActions.Remove(AliasAction);
        RegisterActionAliasInternal(AliasAction, TargetAction, false);
    }
}
