#include "McpAutomationBridge_InteractionHandlersPrivate.h"

namespace McpInteractionHandlers
{
#if WITH_EDITOR
void AddBlueprintVariableIfMissing(
    UBlueprint* Blueprint,
    const FName& VariableName,
    const FEdGraphPinType& PinType,
    TArray<TSharedPtr<FJsonValue>>* AddedVariables)
{
    if (!Blueprint)
    {
        return;
    }

    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == VariableName)
        {
            return;
        }
    }

    FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableName, PinType);
    if (AddedVariables)
    {
        AddedVariables->Add(MakeShared<FJsonValueString>(VariableName.ToString()));
    }
}

bool FindEditorActorByName(const FString& ActorName, AActor*& OutActor)
{
    OutActor = nullptr;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return false;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            OutActor = *It;
            return true;
        }
    }

    return true;
}

FString MakeLegacyPackageName(const FString& Folder, const FString& Name, const FString& DefaultFolder)
{
    FString PackagePath = Folder.IsEmpty() ? DefaultFolder : Folder;
    if (!PackagePath.StartsWith(TEXT("/")))
    {
        PackagePath = TEXT("/Game/") + PackagePath;
    }
    return PackagePath / Name;
}
#endif
}
