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
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CallFunction.h"

namespace McpBPSlice
{
using namespace McpBPSliceDetail;

FCallSlice SliceCalls(UBlueprint* Owner, FName Function, const TArray<UBlueprint*>& Scope)
{
    FCallSlice Out;
    const UClass* Class = Owner->SkeletonGeneratedClass ? Owner->SkeletonGeneratedClass : Owner->GeneratedClass;
    // An event dispatcher by its name: its Calls (broadcasts) are the callers, its Binds and Assigns the binders.
    Out.bDispatcher = Class && FindFProperty<FMulticastDelegateProperty>(Class, Function) != nullptr;
    if (Function.IsNone() || !Class || (!Out.bDispatcher && !Class->FindFunctionByName(Function)))
    {
        TArray<FString> Names;
        for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::ExcludeSuper); It && Names.Num() < 40; ++It)
        {
            if (!It->GetName().StartsWith(TEXT("ExecuteUbergraph"))) Names.Add(It->GetName());
        }
        for (TFieldIterator<FMulticastDelegateProperty> It(Class, EFieldIteratorFlags::ExcludeSuper); It && Names.Num() < 40; ++It)
        {
            Names.Add(It->GetName() + TEXT(" (dispatcher)"));
        }
        Out.Error = FString::Printf(TEXT("%s has no function, custom event or dispatcher '%s'. Its own: %s."), *Owner->GetName(),
                                    *Function.ToString(), Names.Num() ? *FString::Join(Names, TEXT(", ")) : TEXT("none"));
        return Out;
    }
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
                const UK2Node_BaseMCDelegate* Delegate = Out.bDispatcher ? Cast<UK2Node_BaseMCDelegate>(Node) : nullptr;
                if (Delegate && Delegate->GetPropertyName() == Function &&
                    SameBlueprintChain(Delegate->DelegateReference.GetMemberParentClass(Delegate->GetBlueprintClassFromNode()), Owner))
                {
                    const bool bCall = Node->GetClass()->GetName() == TEXT("K2Node_CallDelegate"); // a Bind or Assign otherwise
                    if (++(bCall ? Out.Count : Out.BinderCount) <= MaxRows)
                    {
                        (bCall ? Out.Callers : Out.Binders).Add(MakeShared<FJsonValueObject>(WriterRow(Blueprint, Node, Scope, Flows)));
                    }
                    continue;
                }
                const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
                if (!Call || Call->FunctionReference.GetMemberName() != Function ||
                    !SameBlueprintChain(Call->FunctionReference.GetMemberParentClass(Call->GetBlueprintClassFromNode()), Owner))
                {
                    continue;
                }
                if (++Out.Count <= MaxRows) Out.Callers.Add(MakeShared<FJsonValueObject>(WriterRow(Blueprint, Call, Scope, Flows)));
            }
        }
    }
    return Out;
}

bool HandleSliceFunction(const McpBlueprintGraphHandlers::FActionContext& Context)
{
    UBlueprint* Owner = Context.Blueprint;
    const FName Function(*GetJsonStringField(Context.Payload, TEXT("function")));
    const FNormalizedAssetPath Folder = NormalizeAssetPath(GetJsonStringField(Context.Payload, TEXT("scope"), TEXT("/Game")));
    if (!Folder.bIsValid)
    {
        Context.SendError(Folder.ErrorMessage, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    int32 Skipped = 0;
    const TArray<UBlueprint*> Scope = ScopeFor(Owner, Folder.Path, FMath::Clamp(GetJsonIntField(Context.Payload, TEXT("maxBlueprints"), 200), 1, 1000), Skipped);
    const FCallSlice Calls = SliceCalls(Owner, Function, Scope);
    if (!Calls.Error.IsEmpty())
    {
        Context.SendError(Calls.Error, TEXT("FUNCTION_NOT_FOUND"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("function"), Function.ToString());
    Result->SetArrayField(TEXT("callers"), Calls.Callers);
    Result->SetNumberField(TEXT("callerCount"), Calls.Count);
    if (Calls.bDispatcher)
    {
        Result->SetStringField(TEXT("kind"), TEXT("dispatcher"));
        Result->SetArrayField(TEXT("binders"), Calls.Binders);
        Result->SetNumberField(TEXT("binderCount"), Calls.BinderCount);
    }
    Result->SetStringField(TEXT("scope"), Folder.Path);
    Result->SetNumberField(TEXT("blueprintsRead"), Scope.Num());
    if (Skipped > 0) Result->SetBoolField(TEXT("truncated"), true);
    if (Calls.Count == 0) Result->SetBoolField(TEXT("uncalled"), true);
    Context.NameBlueprint(Result, false);
    Context.SendResponse(FString::Printf(TEXT("%s.%s: %d call(s)%s in %d Blueprint(s) under %s.%s"), *Owner->GetName(),
        *Function.ToString(), Calls.Count, Calls.bDispatcher ? *FString::Printf(TEXT(" (broadcasts) and %d bind(s)"), Calls.BinderCount) : TEXT(""),
        Scope.Num(), *Folder.Path, Calls.Count == 0
            ? TEXT(" Nothing calls it here (an event the engine, a timer or a bound dispatcher fires is not a call).") : TEXT("")), Result);
    return true;
}
} // namespace McpBPSlice
