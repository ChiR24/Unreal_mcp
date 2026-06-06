#include "McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "McpAutomationBridgeSubsystem.h"
#include "WidgetBlueprint.h"

namespace
{
UMcpAutomationBridgeSubsystem* GetAutomationBridgeSubsystem()
{
    if (GEditor)
    {
        return GEditor->GetEditorSubsystem<UMcpAutomationBridgeSubsystem>();
    }
    return nullptr;
}

bool CheckForEngineErrors()
{
    if (UMcpAutomationBridgeSubsystem* Subsystem = GetAutomationBridgeSubsystem())
    {
        return Subsystem->HasCapturedErrors();
    }
    return false;
}

TArray<FString> GetCapturedErrors()
{
    TArray<FString> Errors;
    if (UMcpAutomationBridgeSubsystem* Subsystem = GetAutomationBridgeSubsystem())
    {
        Errors.Append(Subsystem->GetCapturedErrorMessages());
    }
    return Errors;
}
}

namespace WidgetAuthoringHelpers
{
bool ValidateWidgetCreation(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FString& OutError)
{
    if (!WidgetBP || !WidgetBP->WidgetTree)
    {
        OutError = TEXT("Invalid widget blueprint");
        return false;
    }
    UWidget* FoundWidget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
    if (!FoundWidget)
    {
        OutError = FString::Printf(TEXT("Widget '%s' was not found in widget tree after creation"), *WidgetName);
        return false;
    }
    if (CheckForEngineErrors())
    {
        TArray<FString> Errors = GetCapturedErrors();
        OutError = Errors.Num() > 0
            ? FString::Printf(TEXT("Engine error during widget creation: %s"), *Errors[0])
            : TEXT("Engine error occurred during widget creation");
        return false;
    }
    return true;
}

bool CheckWidgetExists(UWidgetBlueprint* WidgetBP, const FString& WidgetName, FString& OutError)
{
    if (!WidgetBP || !WidgetBP->WidgetTree)
    {
        return false;
    }
    UWidget* ExistingWidget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
    if (ExistingWidget)
    {
        OutError = FString::Printf(TEXT("Widget '%s' already exists in blueprint"), *WidgetName);
        return true;
    }
    return false;
}
}
