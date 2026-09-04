#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "Domains/AI/StateTree/McpAutomationBridge_AIStateTreeFeature.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "Modules/ModuleManager.h"

namespace McpAIHandlers
{
namespace
{
struct FAIAssetClassSpec
{
    const TCHAR* Field;
    const TCHAR* ModulePath;
    const TCHAR* ClassName;
};

const FAIAssetClassSpec AIAssetClasses[] = {
    { TEXT("behaviorTrees"), TEXT("/Script/AIModule"), TEXT("BehaviorTree") },
    { TEXT("blackboards"), TEXT("/Script/AIModule"), TEXT("BlackboardData") },
    { TEXT("stateTrees"), TEXT("/Script/StateTreeModule"), TEXT("StateTree") },
    { TEXT("envQueries"), TEXT("/Script/AIModule"), TEXT("EnvQuery") },
    { TEXT("smartObjectDefinitions"), TEXT("/Script/SmartObjectsModule"), TEXT("SmartObjectDefinition") },
};

constexpr int32 MaxListedAIAssetPaths = 20;
}

// Asset-registry inventory of the AI assets under /Game, so a caller who
// passed no target learns what the project has to point get_ai_info at.
void AddAIAssetInventory(const TSharedPtr<FJsonObject>& Result)
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TSharedPtr<FJsonObject> Counts = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Paths;
    int32 Total = 0;

    for (const FAIAssetClassSpec& Spec : AIAssetClasses)
    {
        FARFilter Filter;
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        Filter.bRecursivePaths = true;
        Filter.bRecursiveClasses = true;
#if MCP_HAS_ASSET_CLASS_PATHS
        Filter.ClassPaths.Add(FTopLevelAssetPath(FName(Spec.ModulePath), FName(Spec.ClassName)));
#else
        Filter.ClassNames.Add(FName(Spec.ClassName));
#endif

        TArray<FAssetData> Assets;
        AssetRegistryModule.Get().GetAssets(Filter, Assets);
        Counts->SetNumberField(Spec.Field, Assets.Num());
        Total += Assets.Num();
        for (const FAssetData& Asset : Assets)
        {
            if (Paths.Num() >= MaxListedAIAssetPaths)
            {
                break;
            }
            Paths.Add(MakeShared<FJsonValueString>(MCP_ASSET_DATA_GET_SOFT_PATH(Asset)));
        }
    }

    TSharedPtr<FJsonObject> Inventory = MakeShared<FJsonObject>();
    Inventory->SetObjectField(TEXT("counts"), Counts);
    Inventory->SetNumberField(TEXT("total"), Total);
    Inventory->SetArrayField(TEXT("paths"), Paths);
    Inventory->SetBoolField(TEXT("pathsTruncated"), Total > Paths.Num());
    Result->SetObjectField(TEXT("assets"), Inventory);
}

// stateTreePath: every authored state (depth-first) with its task count.
bool DescribeAIStateTree(const FString& StateTreePath, const TSharedPtr<FJsonObject>& Result, FString& OutError)
{
#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
    UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *StateTreePath);
    if (!StateTree)
    {
        OutError = FString::Printf(TEXT("StateTree not found: %s"), *StateTreePath);
        return false;
    }

    Result->SetStringField(TEXT("stateTreeName"), StateTree->GetName());
    Result->SetStringField(TEXT("stateTreePath"), StateTree->GetPathName());

    TArray<TSharedPtr<FJsonValue>> States;
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
    if (EditorData)
    {
        TFunction<void(UStateTreeState*, int32)> Visit;
        Visit = [&States, &Visit](UStateTreeState* State, int32 Depth)
        {
            if (!State)
            {
                return;
            }
            TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("name"), State->Name.ToString());
            Entry->SetNumberField(TEXT("taskCount"), State->Tasks.Num());
            Entry->SetNumberField(TEXT("depth"), Depth);
            Entry->SetNumberField(TEXT("childCount"), State->Children.Num());
            States.Add(MakeShared<FJsonValueObject>(Entry));
            for (UStateTreeState* Child : State->Children)
            {
                Visit(Child, Depth + 1);
            }
        };
        for (UStateTreeState* SubTree : EditorData->SubTrees)
        {
            Visit(SubTree, 0);
        }
    }

    Result->SetArrayField(TEXT("states"), States);
    Result->SetNumberField(TEXT("stateCount"), States.Num());
    Result->SetBoolField(TEXT("hasEditorData"), EditorData != nullptr);
    return true;
#else
    OutError = TEXT("State Trees require UE 5.3+ with the StateTree plugin enabled");
    return false;
#endif
}
}
#endif
