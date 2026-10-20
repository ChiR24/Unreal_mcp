#include "Domains/AssetWorkflow/Enums/Shared.h"
#include "Kismet2/EnumEditorUtils.h"

// split_enum: the new enum takes the named `values` (by internal or display name) or every value from position `index`
// on, each keeping its internal and display name; the source enum is left unchanged.
bool HandleEnumSplit(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult)
{
    if (Action != TEXT("split_enum"))
    {
        return false;
    }
    bool bHandled = false;
    UUserDefinedEnum* Enum = RequireEnum(Params, OutResult, bHandled);
    if (bHandled) { return true; }

    FString NewName = GetJsonStringField(Params, TEXT("newEnumName"));
    if (NewName.IsEmpty()) { SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: newEnumName"), TEXT("MISSING_PARAMETER")); return true; }

    const TArray<TSharedPtr<FJsonValue>>* ValuesArr = nullptr;
    const bool bHasValues = Params->TryGetArrayField(TEXT("values"), ValuesArr) && ValuesArr;
    const bool bHasIndex = Params->HasField(TEXT("index"));
    const int32 SplitIndex = static_cast<int32>(GetJsonNumberField(Params, TEXT("index"), 0.0));
    if (bHasValues == bHasIndex)
    {
        SetEnumResultFields(OutResult, false, TEXT("Pass values (names to copy) or index (copy every value from that position on), not both."), bHasValues ? TEXT("INVALID_ARGUMENT") : TEXT("MISSING_PARAMETER"));
        return true;
    }

    // The indices of the values to copy, in the source order.
    TArray<int32> Picked;
    if (bHasValues)
    {
        for (const TSharedPtr<FJsonValue>& V : *ValuesArr)
        {
            const int32 Index = McpFindEnumValue(Enum, V->AsString());
            if (Index == INDEX_NONE)
            {
                SetEnumResultFields(OutResult, false, FString::Printf(TEXT("Enum value '%s' not found; nothing was split."), *V->AsString()), TEXT("NOT_FOUND"));
                return true;
            }
            Picked.AddUnique(Index);
        }
        Picked.Sort();
    }
    else
    {
        for (int32 Index = FMath::Max(SplitIndex, 0); Index < Enum->NumEnums(); ++Index)
        {
            if (!Enum->GetNameStringByIndex(Index).EndsWith(TEXT("_MAX"))) { Picked.Add(Index); }
        }
    }
    if (Picked.Num() == 0) { SetEnumResultFields(OutResult, false, TEXT("No matching values to split"), TEXT("NOT_FOUND")); return true; }

    FString Path = GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Enums"));
    FString PathError;
    FString SanitizedName = SanitizeAssetName(NewName);
    FString PackageName;
    if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, PathError)) { SetEnumResultFields(OutResult, false, PathError); return true; }

    FString CreateError, CreateCode;
    UPackage* Package = McpCreatePackageForNewAsset(PackageName, CreateError, CreateCode);
    if (!Package) { SetEnumResultFields(OutResult, false, CreateError, *CreateCode); return true; }

    UUserDefinedEnum* NewEnum = Cast<UUserDefinedEnum>(FEnumEditorUtils::CreateUserDefinedEnum(Package, FName(*SanitizedName), RF_Public | RF_Standalone));
    if (!NewEnum) { SetEnumResultFields(OutResult, false, TEXT("Failed to create split enum")); return true; }

    TArray<TPair<FName, int64>> NewNames;
    for (int32 i = 0; i < Picked.Num(); ++i)
    {
        NewNames.Emplace(*NewEnum->GenerateFullEnumName(*Enum->GetNameStringByIndex(Picked[i])), static_cast<int64>(i));
    }
    MCP_SET_ENUMS(NewEnum, NewNames, NewEnum->GetCppForm());
    for (int32 i = 0; i < Picked.Num(); ++i)
    {
        FEnumEditorUtils::SetEnumeratorDisplayName(NewEnum, i, Enum->GetDisplayNameTextByIndex(Picked[i]));
    }
    FinalizeEnum(NewEnum, EnumSaveRequested(Params));
    FAssetRegistryModule::AssetCreated(NewEnum);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("enumPath"), PackageName + TEXT(".") + SanitizedName);
    Result->SetStringField(TEXT("enumName"), SanitizedName);
    Result->SetNumberField(TEXT("valueCount"), Picked.Num());
    Result->SetStringField(TEXT("sourceEnumPath"), GetJsonStringField(Params, TEXT("enumPath")));
    Result->SetBoolField(TEXT("sourceEnumModified"), false);
    Result->SetStringField(TEXT("message"), TEXT("split_enum copies the listed values into a new enum; the source enum is left unchanged"));
    OutResult = Result;
    return true;
}
