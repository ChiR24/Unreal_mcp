#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"

#include "Domains/Lighting/McpAutomationBridge_LightingHandlersPrivate.h"
#include "Domains/Level/World/McpAutomationBridge_LevelHandlersWorldAccess.h"
#include "Editor.h"
#include "EditorBuildUtils.h"
#include "Engine/Level.h"
#include "Engine/World.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
namespace {

TSharedPtr<FJsonObject> ClonePayload(const TSharedPtr<FJsonObject> &Payload)
{
    TSharedPtr<FJsonObject> ClonedPayload = McpHandlerUtils::CreateResultObject();
    if (Payload.IsValid())
    {
        ClonedPayload->Values = Payload->Values;
    }
    return ClonedPayload;
}

void CopyTopLevelLightProperties(const TSharedPtr<FJsonObject> &SourcePayload, const TSharedPtr<FJsonObject> &TargetPayload)
{
    if (!SourcePayload.IsValid() || !TargetPayload.IsValid())
    {
        return;
    }

    TSharedPtr<FJsonObject> Properties = McpHandlerUtils::CreateResultObject();
    bool bHasProperties = false;

    const TSharedPtr<FJsonObject> *ExistingProperties = nullptr;
    if (SourcePayload->TryGetObjectField(TEXT("properties"), ExistingProperties) && ExistingProperties && ExistingProperties->IsValid())
    {
        Properties->Values = (*ExistingProperties)->Values;
        bHasProperties = true;
    }

    double Intensity = 0.0;
    if (SourcePayload->TryGetNumberField(TEXT("intensity"), Intensity) && !Properties->HasField(TEXT("intensity")))
    {
        Properties->SetNumberField(TEXT("intensity"), Intensity);
        bHasProperties = true;
    }

    const TSharedPtr<FJsonObject> *ColorObject = nullptr;
    if (SourcePayload->TryGetObjectField(TEXT("color"), ColorObject) && ColorObject && ColorObject->IsValid() && !Properties->HasField(TEXT("color")))
    {
        Properties->SetObjectField(TEXT("color"), *ColorObject);
        bHasProperties = true;
    }
    else
    {
        const TArray<TSharedPtr<FJsonValue>> *ColorArray = nullptr;
        if (SourcePayload->TryGetArrayField(TEXT("color"), ColorArray) && ColorArray && ColorArray->Num() >= 3 && !Properties->HasField(TEXT("color")))
        {
            TSharedPtr<FJsonObject> Color = McpHandlerUtils::CreateResultObject();
            Color->SetNumberField(TEXT("r"), (*ColorArray)[0].IsValid() ? (*ColorArray)[0]->AsNumber() : 1.0);
            Color->SetNumberField(TEXT("g"), (*ColorArray)[1].IsValid() ? (*ColorArray)[1]->AsNumber() : 1.0);
            Color->SetNumberField(TEXT("b"), (*ColorArray)[2].IsValid() ? (*ColorArray)[2]->AsNumber() : 1.0);
            Color->SetNumberField(TEXT("a"), ColorArray->Num() > 3 && (*ColorArray)[3].IsValid() ? (*ColorArray)[3]->AsNumber() : 1.0);
            Properties->SetObjectField(TEXT("color"), Color);
            bHasProperties = true;
        }
    }

    if (bHasProperties)
    {
        TargetPayload->SetObjectField(TEXT("properties"), Properties);
    }
}

ULevel* ResolveLightingScenarioLevel(UWorld& World, const FString& RequestedLevelPath)
{
    if (RequestedLevelPath.IsEmpty())
    {
        return World.GetCurrentLevel();
    }

    for (ULevel* Level : GetAllLevelsFromWorld(&World))
    {
        if (Level && Level->GetOutermost() &&
            Level->GetOutermost()->GetName().Equals(RequestedLevelPath, ESearchCase::IgnoreCase))
        {
            return Level;
        }
    }
    return nullptr;
}

bool SendEditorBuildResult(
    UMcpAutomationBridgeSubsystem &Subsystem, const FString &RequestId, bool bBuildSucceeded,
    const FString &SuccessMessage, const FString &FailureMessage, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("buildStarted"), bBuildSucceeded);
    Subsystem.SendAutomationResponse(
        RequestingSocket,
        RequestId,
        bBuildSucceeded,
        bBuildSucceeded ? SuccessMessage : FailureMessage,
        Result,
        bBuildSucceeded ? FString() : TEXT("BUILD_FAILED"));
    return true;
}

}

bool HandleBuildLightingAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return McpLightingHandlers::HandleBuildLighting(Subsystem, RequestId, Payload, RequestingSocket);
}

bool HandleSpawnLightAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> RoutedPayload = ClonePayload(Payload);
    CopyTopLevelLightProperties(Payload, RoutedPayload);
    return McpLightingHandlers::HandleSpawnLight(Subsystem, RequestId, RoutedPayload, RequestingSocket);
}

bool HandleSetLevelLightingAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString RequestedLevelPath;
    bool bIsLightingScenario = true;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("levelPath"), RequestedLevelPath);
        if (RequestedLevelPath.IsEmpty())
        {
            Payload->TryGetStringField(TEXT("level_path"), RequestedLevelPath);
        }
        if (!Payload->TryGetBoolField(TEXT("isLightingScenario"), bIsLightingScenario))
        {
            Payload->TryGetBoolField(TEXT("bIsLightingScenario"), bIsLightingScenario);
        }
    }

    if (!RequestedLevelPath.IsEmpty())
    {
        RequestedLevelPath = SanitizeProjectRelativePath(RequestedLevelPath);
        if (RequestedLevelPath.IsEmpty())
        {
            Subsystem.SendAutomationResponse(
                RequestingSocket,
                RequestId,
                false,
                TEXT("Invalid levelPath"),
                nullptr,
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    ULevel* TargetLevel = ResolveLightingScenarioLevel(*World, RequestedLevelPath);
    if (!TargetLevel)
    {
        Subsystem.SendAutomationResponse(
            RequestingSocket,
            RequestId,
            false,
            RequestedLevelPath.IsEmpty()
                ? TEXT("No current level")
                : FString::Printf(TEXT("Level not found: %s"), *RequestedLevelPath),
            nullptr,
            RequestedLevelPath.IsEmpty() ? TEXT("NO_LEVEL") : TEXT("LEVEL_NOT_FOUND"));
        return true;
    }

    TargetLevel->Modify();
    TargetLevel->SetLightingScenario(bIsLightingScenario);
    TargetLevel->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(
        TEXT("levelPath"),
        TargetLevel->GetOutermost() ? TargetLevel->GetOutermost()->GetName() : FString());
    Result->SetBoolField(TEXT("isLightingScenario"), bIsLightingScenario);
    Result->SetBoolField(TEXT("lightingSet"), true);
    Subsystem.SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("Level lighting settings updated"), Result);
    return true;
}

bool HandleGetLevelLightingScenariosAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    (void)Payload;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> Scenarios;
    for (ULevel* Level : GetAllLevelsFromWorld(World))
    {
        if (Level && Level->bIsLightingScenario)
        {
            TSharedPtr<FJsonObject> ScenarioInfo = McpHandlerUtils::CreateResultObject();
            ScenarioInfo->SetStringField(
                TEXT("levelPath"),
                Level->GetOutermost() ? Level->GetOutermost()->GetName() : FString());
            ScenarioInfo->SetStringField(TEXT("levelName"), Level->GetName());
            Scenarios.Add(MakeShared<FJsonValueObject>(ScenarioInfo));
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("scenarios"), Scenarios);
    Result->SetNumberField(TEXT("count"), Scenarios.Num());
    Subsystem.SendAutomationResponse(
        RequestingSocket, RequestId, true, TEXT("Lighting scenarios retrieved"), Result);
    return true;
}

bool HandleBuildLevelNavigationAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    (void)Payload;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    const bool bBuildSucceeded =
        FEditorBuildUtils::EditorBuild(World, FBuildOptions::BuildAIPaths);
    return SendEditorBuildResult(
        Subsystem,
        RequestId,
        bBuildSucceeded,
        TEXT("Navigation build started"),
        TEXT("Navigation build failed or was cancelled"),
        RequestingSocket);
}

bool HandleBuildAllLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    (void)Payload;
    UWorld *World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    const bool bBuildSucceeded =
        FEditorBuildUtils::EditorBuild(World, FBuildOptions::BuildAll);
    return SendEditorBuildResult(
        Subsystem,
        RequestId,
        bBuildSucceeded,
        TEXT("Full level build started"),
        TEXT("Full level build failed or was cancelled"),
        RequestingSocket);
}
#endif
}
