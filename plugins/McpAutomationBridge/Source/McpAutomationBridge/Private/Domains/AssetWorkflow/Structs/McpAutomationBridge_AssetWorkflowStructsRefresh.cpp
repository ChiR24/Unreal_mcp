#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#include "EdGraphSchema_K2.h"
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"

#if WITH_EDITOR

void McpRefreshStructDependents(UUserDefinedStruct* S,
    TArray<FString>* OutBlueprints,
    TArray<FString>* OutDataTables,
    TArray<FString>* OutEnums,
    TArray<FString>* OutStructs,
    TArray<FString>* OutDataAssets)
{
    if (!S) return;

    // Recompile the struct itself so downstream referencers rebuild against the new layout.
    FStructureEditorUtils::CompileStructure(S);

    // 1) Collect and categorize all referencers via the asset registry.
    TArray<UUserDefinedStruct*> NestedStructs;
    TArray<UBlueprint*> ReferencingBPs;

    ForEachReferencingAsset(S, [&](UObject* Asset)
    {
        if (UUserDefinedStruct* Nested = Cast<UUserDefinedStruct>(Asset))
        {
            NestedStructs.Add(Nested);
        }
        else if (UBlueprint* BP = Cast<UBlueprint>(Asset))
        {
            ReferencingBPs.Add(BP);
        }
        else if (UDataAsset* DA = Cast<UDataAsset>(Asset))
        {
            if (OutDataAssets)
            {
                OutDataAssets->AddUnique(DA->GetPathName());
            }
        }
    });

    // 2) Recompile nested UserDefinedStruct referencers FIRST so that
    //    downstream Blueprints rebuild against the updated nested layout.
    for (UUserDefinedStruct* Nested : NestedStructs)
    {
        FStructureEditorUtils::CompileStructure(Nested);
        Nested->GetOutermost()->MarkPackageDirty();
        if (OutStructs)
        {
            OutStructs->AddUnique(Nested->GetPathName());
        }
    }

    // 3) Recompile every directly- and transitively-referencing Blueprint.
    for (UBlueprint* BP : ReferencingBPs)
    {
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None);
        if (OutBlueprints)
        {
            OutBlueprints->AddUnique(BP->GetPathName());
        }
    }

    // 4) Notify DataTables whose RowStruct points at this struct (rows may need re-validation).
    IAssetRegistry& AR = FAssetRegistryModule::GetRegistry();
    TArray<FAssetData> DataTableAssets;
    AR.GetAssetsByClass(UDataTable::StaticClass()->GetClassPathName(), DataTableAssets, true);
    for (const FAssetData& AD : DataTableAssets)
    {
        UDataTable* DT = Cast<UDataTable>(AD.GetAsset());
        if (DT && DT->GetRowStruct() == Cast<UScriptStruct>(S))
        {
            DT->HandleDataTableChanged();
            if (OutDataTables)
            {
                OutDataTables->AddUnique(DT->GetPathName());
            }
        }
    }

    // 5) Record enum-typed members (enums need no recompile, just reported for coverage).
    for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(S))
    {
        const FEdGraphPinType PinType = Var.ToPinType();
        if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
        {
            if (UEnum* E = Cast<UEnum>(PinType.PinSubCategoryObject.Get()))
            {
                if (OutEnums)
                {
                    OutEnums->AddUnique(E->GetPathName());
                }
            }
        }
    }
}

#endif // WITH_EDITOR
