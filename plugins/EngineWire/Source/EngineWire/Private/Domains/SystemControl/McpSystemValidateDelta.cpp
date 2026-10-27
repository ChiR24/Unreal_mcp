#include "Domains/SystemControl/McpSystemValidateDelta.h"

#include "Core/Journal/McpChangeJournal.h"
#include "Core/Requests/McpResponseCaptureRegistry.h"

#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

namespace McpValidateDelta
{
namespace
{
constexpr int32 MaxValidated = 8; // each runs every validator the project has on that asset
constexpr int32 MaxListed = 8;
} // namespace

void AddWarnings(const FString& RequestId, const TSharedPtr<FJsonObject>& Result)
{
    // A batch step's reply is captured into its batch's, which is checked once.
    if (!Result.IsValid() || FMcpResponseCaptureRegistry::Get().IsCapturing(RequestId))
    {
        return;
    }
    TArray<FString> Packages;
    McpChangeJournal::AssetsChangedBy(RequestId, Packages);
    // The plugin is optional and its module delay-loaded: its subsystem is reached only while that module is loaded.
    UEditorValidatorSubsystem* Validator =
        Packages.Num() > 0 && GEditor && FModuleManager::Get().IsModuleLoaded(TEXT("DataValidation"))
            ? GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>()
            : nullptr;
    if (!Validator)
    {
        return;
    }
    TArray<FString> Found;
    int32 Validated = 0;
    for (const FString& Package : Packages)
    {
        // What a call changed is in memory; a removed asset, or a package whose asset has another name, is skipped.
        UObject* Asset = FindObject<UObject>(nullptr, *(Package + TEXT(".") + FPackageName::GetShortName(Package)));
        if (!IsValid(Asset) || ++Validated > MaxValidated)
        {
            continue;
        }
        // The Save use case: the checks the editor makes on every save. A manual or script check also runs heavier
        // validators (5.8's material validator compiles shaders), which no reply should wait for.
        TArray<FText> Errors, Warnings;
        const EDataValidationResult Verdict =
            Validator->IsAssetValid(FAssetData(Asset), Errors, Warnings, EDataValidationUsecase::Save);
        for (const FText& Text : Errors)
        {
            Found.AddUnique(FString::Printf(TEXT("'%s': Data Validation error: %s"), *Package, *Text.ToString()));
        }
        for (const FText& Text : Warnings)
        {
            Found.AddUnique(FString::Printf(TEXT("'%s': Data Validation: %s"), *Package, *Text.ToString()));
        }
        if (Verdict == EDataValidationResult::Invalid && Errors.Num() == 0)
        {
            Found.Add(FString::Printf(TEXT("'%s': Data Validation found it invalid without saying why."), *Package));
        }
    }
    if (Found.Num() == 0 && Validated <= MaxValidated)
    {
        return;
    }
    TArray<TSharedPtr<FJsonValue>> Lines;
    const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
    if (Result->TryGetArrayField(TEXT("warnings"), Existing) && Existing)
    {
        Lines = *Existing;
    }
    for (int32 Index = 0; Index < Found.Num() && Index < MaxListed; ++Index)
    {
        Lines.Add(MakeShared<FJsonValueString>(Found[Index]));
    }
    TArray<FString> Unsaid;
    if (Found.Num() > MaxListed)
    {
        Unsaid.Add(FString::Printf(TEXT("%d more Data Validation finding(s)"), Found.Num() - MaxListed));
    }
    if (Validated > MaxValidated)
    {
        Unsaid.Add(FString::Printf(TEXT("%d of the %d assets it changed were not validated"), Validated - MaxValidated, Validated));
    }
    if (Unsaid.Num() > 0)
    {
        Lines.Add(MakeShared<FJsonValueString>(FString::Join(Unsaid, TEXT("; ")) +
            TEXT(": system_control validate_assets with dataValidation true checks them all.")));
    }
    Result->SetArrayField(TEXT("warnings"), Lines);
}
} // namespace McpValidateDelta
