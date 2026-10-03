#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringValidation.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Editor.h"
#include "Kismet2/Kismet2NameValidators.h"
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

FString McpWidgetNameConflict(UWidgetBlueprint* WidgetBP, const FName Name, const UClass* WidgetClass)
{
    if (const UWidget* Existing = WidgetBP->WidgetTree->FindWidget(Name))
    {
        return Existing->GetClass() == WidgetClass ? FString()
            : FString::Printf(TEXT("'%s' is already a %s in this widget tree"), *Name.ToString(), *Existing->GetClass()->GetName());
    }
    const FProperty* Bound = WidgetBP->ParentClass ? WidgetBP->ParentClass->FindPropertyByName(Name) : nullptr;
    if (Bound && (Bound->HasMetaData(TEXT("BindWidget")) || Bound->HasMetaData(TEXT("BindWidgetOptional"))))
    {
        return FString();
    }
    // The validator's own text ("Name is already in use.") does not say which name it refused.
    const EValidatorResult Result = FKismetNameValidator(WidgetBP).IsValid(Name);
    return Result == EValidatorResult::Ok ? FString()
        : FString::Printf(TEXT("'%s': %s"), *Name.ToString(), *INameValidatorInterface::GetErrorString(Name.ToString(), Result));
}

FString McpFreeWidgetName(UWidgetBlueprint* WidgetBP, const FName Name, const UClass* WidgetClass)
{
    for (int32 Suffix = 1; Suffix < 1000; ++Suffix)
    {
        const FString Candidate = FString::Printf(TEXT("%s_%d"), *Name.ToString(), Suffix);
        if (!WidgetBP->WidgetTree->FindWidget(FName(*Candidate)) && McpWidgetNameConflict(WidgetBP, FName(*Candidate), WidgetClass).IsEmpty())
        {
            return Candidate;
        }
    }
    return Name.ToString() + TEXT("_") + FGuid::NewGuid().ToString().Left(8);
}
}
