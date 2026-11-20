// A static slice over one member variable (Weiser's program slicing, the question the Whyline answers): who sets it,
// on what paths and behind which Branches, and who reads it. Exec paths and dominators come from the lint's flow
// (McpBPLintFlow.h).

#include "Domains/BlueprintGraph/Slice/McpBPSlice.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "Domains/BlueprintGraph/Slice/McpBPSliceWriters.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetResolve.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"

namespace McpBPSliceDetail
{
// Owner, then the Blueprints under Folder up to Cap in all; OutSkipped counts the ones past it.
TArray<UBlueprint*> ScopeBlueprints(UBlueprint* Owner, const FString& Folder, int32 Cap, int32& OutSkipped)
{
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*Folder));
    Filter.bRecursivePaths = true;
    Filter.bRecursiveClasses = true;
    MCP_AR_FILTER_ADD_CLASS(Filter, UBlueprint::StaticClass());
    TArray<FAssetData> Assets;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssets(Filter, Assets);
    OutSkipped = 0;
    TArray<UBlueprint*> Scope{Owner};
    const FName OwnerPackage = Owner->GetOutermost()->GetFName();
    for (const FAssetData& Asset : Assets)
    {
        if (Asset.PackageName == OwnerPackage)
        {
            continue;
        }
        UBlueprint* Blueprint = Scope.Num() < Cap ? Cast<UBlueprint>(Asset.GetAsset()) : nullptr;
        OutSkipped += Scope.Num() < Cap ? 0 : 1;
        if (Blueprint)
        {
            Scope.Add(Blueprint);
        }
    }
    return Scope;
}
} // namespace McpBPSliceDetail

namespace McpBPSlice
{
using namespace McpBPSliceDetail;

bool Touches(const UEdGraphNode* Node, const UBlueprint* Owner, FName Variable, bool& bOutSet)
{
    const UK2Node_Variable* Var = Cast<UK2Node_Variable>(Node);
    bOutSet = Cast<UK2Node_VariableSet>(Node) != nullptr;
    return Var && (bOutSet || Cast<UK2Node_VariableGet>(Node)) && Var->GetVarName() == Variable
        && SameBlueprintChain(Var->GetVariableSourceClass(), Owner);
}

bool HandleSlice(const McpBlueprintGraphHandlers::FActionContext& Context)
{
    if (Context.SubAction != TEXT("slice_variable"))
    {
        return false;
    }
    UBlueprint* Owner = Context.Blueprint;
    const FName Variable(*GetJsonStringField(Context.Payload, TEXT("variable")));
    const UClass* Class = Owner->SkeletonGeneratedClass;
    if (!Class)
    {
        Class = Owner->GeneratedClass;
    }
    if (Variable.IsNone() || !Class || !FindFProperty<FProperty>(Class, Variable))
    {
        FString Names;
        for (const FBPVariableDescription& Each : Owner->NewVariables)
        {
            Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Each.VarName.ToString();
        }
        Context.SendError(FString::Printf(TEXT("%s has no variable '%s'. Its own variables: %s."), *Owner->GetName(),
                                          *Variable.ToString(), Names.IsEmpty() ? TEXT("none") : *Names), TEXT("VARIABLE_NOT_FOUND"));
        return true;
    }
    const FNormalizedAssetPath Folder = NormalizeAssetPath(GetJsonStringField(Context.Payload, TEXT("scope"), TEXT("/Game")));
    if (!Folder.bIsValid)
    {
        Context.SendError(Folder.ErrorMessage, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const int32 Cap = FMath::Clamp(GetJsonIntField(Context.Payload, TEXT("maxBlueprints"), 200), 1, 1000);
    int32 Skipped = 0;
    const TArray<UBlueprint*> Scope = ScopeBlueprints(Owner, Folder.Path, Cap, Skipped);
    TSharedPtr<FJsonObject> Result = Slice(Owner, Variable, Scope);
    Result->SetStringField(TEXT("scope"), Folder.Path);
    Result->SetNumberField(TEXT("blueprintsRead"), Scope.Num());
    if (Skipped > 0)
    {
        Result->SetBoolField(TEXT("truncated"), true);
    }
    Context.NameBlueprint(Result, false);
    const bool bDead = Result->HasField(TEXT("deadWrite"));
    Context.SendResponse(FString::Printf(TEXT("%s.%s: %d writer(s), %d reader(s) in %d Blueprint(s) under %s.%s%s"),
        *Owner->GetName(), *Variable.ToString(), int32(Result->GetNumberField(TEXT("writerCount"))), int32(Result->GetNumberField(TEXT("readerCount"))),
        Scope.Num(), *Folder.Path, Skipped > 0 ? *FString::Printf(TEXT(" %d more not read: raise maxBlueprints or name a narrower scope."), Skipped) : TEXT(""),
        bDead ? TEXT(" Set but never read in a graph (a widget binding or Anim Blueprint property access is not read here).") : TEXT("")), Result);
    return true;
}

TSharedPtr<FJsonObject> Slice(const UBlueprint* Owner, FName Variable, const TArray<UBlueprint*>& Scope)
{
    TArray<TSharedPtr<FJsonValue>> Writers, Readers;
    int32 WriterCount = 0, ReaderCount = 0;
    TMap<const UEdGraph*, McpBPLint::FFlow> Flows;
    for (const UBlueprint* Blueprint : Scope)
    {
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (const UEdGraph* Graph : Graphs)
        {
            if (!Graph)
            {
                continue;
            }
            for (const UEdGraphNode* Node : Graph->Nodes)
            {
                bool bSet = false;
                if (!Node || !Touches(Node, Owner, Variable, bSet))
                {
                    continue;
                }
                int32& Count = bSet ? WriterCount : ReaderCount;
                if (++Count <= MaxRows)
                {
                    (bSet ? Writers : Readers).Add(MakeShared<FJsonValueObject>(bSet ? WriterRow(Blueprint, Node, Scope, Flows) : NodeRow(Blueprint, Node)));
                }
            }
        }
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("variable"), Variable.ToString());
    Result->SetArrayField(TEXT("writers"), Writers);
    Result->SetArrayField(TEXT("readers"), Readers);
    Result->SetNumberField(TEXT("writerCount"), WriterCount);
    Result->SetNumberField(TEXT("readerCount"), ReaderCount);
    if (WriterCount > 0 && ReaderCount == 0)
    {
        Result->SetBoolField(TEXT("deadWrite"), true);
    }
    return Result;
}
} // namespace McpBPSlice
