// manage_blueprint slice_function: every call of one Blueprint function or custom event across the project's
// Blueprints, each with the events that reach it and the Branches it sits behind (the variable slice's rows), and
// whether nothing in scope calls it. An event the engine, a timer or a bound dispatcher fires is no call here.

#include "Domains/BlueprintGraph/Slice/McpBPSlice.h"

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "Domains/BlueprintGraph/Slice/McpBPSliceWriters.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetResolve.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"

namespace McpBPSlice
{
using namespace McpBPSliceDetail;

bool HandleSliceFunction(const McpBlueprintGraphHandlers::FActionContext& Context)
{
    UBlueprint* Owner = Context.Blueprint;
    const FName Function(*GetJsonStringField(Context.Payload, TEXT("function")));
    const UClass* Class = Owner->SkeletonGeneratedClass ? Owner->SkeletonGeneratedClass : Owner->GeneratedClass;
    if (Function.IsNone() || !Class || !Class->FindFunctionByName(Function))
    {
        TArray<FString> Names;
        for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It && Names.Num() < 40; ++It)
        {
            if (!It->GetName().StartsWith(TEXT("ExecuteUbergraph"))) Names.Add(It->GetName());
        }
        Context.SendError(FString::Printf(TEXT("%s has no function or custom event '%s'. Its own: %s."), *Owner->GetName(),
                                          *Function.ToString(), Names.Num() ? *FString::Join(Names, TEXT(", ")) : TEXT("none")),
                          TEXT("FUNCTION_NOT_FOUND"));
        return true;
    }
    const FNormalizedAssetPath Folder = NormalizeAssetPath(GetJsonStringField(Context.Payload, TEXT("scope"), TEXT("/Game")));
    if (!Folder.bIsValid)
    {
        Context.SendError(Folder.ErrorMessage, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    int32 Skipped = 0;
    const TArray<UBlueprint*> Scope = ScopeFor(Owner, Folder.Path, FMath::Clamp(GetJsonIntField(Context.Payload, TEXT("maxBlueprints"), 200), 1, 1000), Skipped);
    TArray<TSharedPtr<FJsonValue>> Callers;
    int32 Count = 0;
    TMap<const UEdGraph*, McpBPLint::FFlow> Flows;
    for (const UBlueprint* Blueprint : Scope)
    {
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (const UEdGraph* Graph : Graphs)
        {
            if (!Graph) continue;
            for (const UEdGraphNode* Node : Graph->Nodes)
            {
                const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
                if (!Call || Call->FunctionReference.GetMemberName() != Function ||
                    !SameBlueprintChain(Call->FunctionReference.GetMemberParentClass(Call->GetBlueprintClassFromNode()), Owner))
                {
                    continue;
                }
                if (++Count <= MaxRows) Callers.Add(MakeShared<FJsonValueObject>(WriterRow(Blueprint, Call, Scope, Flows)));
            }
        }
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("function"), Function.ToString());
    Result->SetArrayField(TEXT("callers"), Callers);
    Result->SetNumberField(TEXT("callerCount"), Count);
    Result->SetStringField(TEXT("scope"), Folder.Path);
    Result->SetNumberField(TEXT("blueprintsRead"), Scope.Num());
    if (Skipped > 0) Result->SetBoolField(TEXT("truncated"), true);
    if (Count == 0) Result->SetBoolField(TEXT("uncalled"), true);
    Context.NameBlueprint(Result, false);
    Context.SendResponse(FString::Printf(TEXT("%s.%s: %d call(s) in %d Blueprint(s) under %s.%s"), *Owner->GetName(),
        *Function.ToString(), Count, Scope.Num(), *Folder.Path, Count == 0
            ? TEXT(" Nothing calls it here (an event the engine, a timer or a bound dispatcher fires is not a call).") : TEXT("")), Result);
    return true;
}
} // namespace McpBPSlice
