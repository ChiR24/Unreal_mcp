#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

namespace McpBlueprintGraphHandlers
{
namespace
{
// A node type that names a stock function: the class that declares it and the function.
const TTuple<UClass*, const TCHAR*>* FindCommonFunctionNode(
    const FString& NodeType)
{
    static const TMap<FString, TTuple<UClass*, const TCHAR*>> FunctionNodes = {
        {TEXT("PrintString"),
         MakeTuple(UKismetSystemLibrary::StaticClass(), TEXT("PrintString"))},
        {TEXT("Print"),
         MakeTuple(UKismetSystemLibrary::StaticClass(), TEXT("PrintString"))},
        {TEXT("PrintText"),
         MakeTuple(UKismetSystemLibrary::StaticClass(), TEXT("PrintText"))},
        {TEXT("SetActorLocation"),
         MakeTuple(AActor::StaticClass(), TEXT("K2_SetActorLocation"))},
        {TEXT("GetActorLocation"),
         MakeTuple(AActor::StaticClass(), TEXT("K2_GetActorLocation"))},
        {TEXT("SetActorRotation"),
         MakeTuple(AActor::StaticClass(), TEXT("K2_SetActorRotation"))},
        {TEXT("GetActorRotation"),
         MakeTuple(AActor::StaticClass(), TEXT("K2_GetActorRotation"))},
        {TEXT("SetActorTransform"),
         MakeTuple(AActor::StaticClass(), TEXT("K2_SetActorTransform"))},
        {TEXT("GetActorTransform"),
         MakeTuple(AActor::StaticClass(), TEXT("K2_GetActorTransform"))},
        {TEXT("AddActorLocalOffset"),
         MakeTuple(AActor::StaticClass(), TEXT("K2_AddActorLocalOffset"))},
        {TEXT("Delay"),
         MakeTuple(UKismetSystemLibrary::StaticClass(), TEXT("Delay"))},
        {TEXT("DestroyActor"),
         MakeTuple(AActor::StaticClass(), TEXT("K2_DestroyActor"))},
        {TEXT("SpawnActor"),
         MakeTuple(
             UGameplayStatics::StaticClass(),
             TEXT("BeginDeferredActorSpawnFromClass"))},
        {TEXT("GetPlayerPawn"),
         MakeTuple(UGameplayStatics::StaticClass(), TEXT("GetPlayerPawn"))},
        {TEXT("GetPlayerController"),
         MakeTuple(UGameplayStatics::StaticClass(), TEXT("GetPlayerController"))},
        // Plain UFUNCTIONs whose output follows the ActorClass pin; there is no K2Node class for them.
        {TEXT("GetAllActorsOfClass"),
         MakeTuple(UGameplayStatics::StaticClass(), TEXT("GetAllActorsOfClass"))},
        {TEXT("GetActorOfClass"),
         MakeTuple(UGameplayStatics::StaticClass(), TEXT("GetActorOfClass"))},
        {TEXT("PlaySound"),
         MakeTuple(UGameplayStatics::StaticClass(), TEXT("PlaySound2D"))},
        {TEXT("PlaySound2D"),
         MakeTuple(UGameplayStatics::StaticClass(), TEXT("PlaySound2D"))},
        {TEXT("PlaySoundAtLocation"),
         MakeTuple(UGameplayStatics::StaticClass(), TEXT("PlaySoundAtLocation"))},
        {TEXT("GetWorldDeltaSeconds"),
         MakeTuple(UGameplayStatics::StaticClass(), TEXT("GetWorldDeltaSeconds"))},
        {TEXT("SetTimerByFunctionName"),
         MakeTuple(UKismetSystemLibrary::StaticClass(), TEXT("K2_SetTimer"))},
        {TEXT("ClearTimer"),
         MakeTuple(UKismetSystemLibrary::StaticClass(), TEXT("K2_ClearTimer"))},
        {TEXT("IsValid"),
         MakeTuple(UKismetSystemLibrary::StaticClass(), TEXT("IsValid"))},
        {TEXT("IsValidClass"),
         MakeTuple(UKismetSystemLibrary::StaticClass(), TEXT("IsValidClass"))},
        {TEXT("Add_IntInt"),
         MakeTuple(UKismetMathLibrary::StaticClass(), TEXT("Add_IntInt"))},
        {TEXT("Subtract_IntInt"),
         MakeTuple(UKismetMathLibrary::StaticClass(), TEXT("Subtract_IntInt"))},
        {TEXT("Multiply_IntInt"),
         MakeTuple(UKismetMathLibrary::StaticClass(), TEXT("Multiply_IntInt"))},
        {TEXT("Divide_IntInt"),
         MakeTuple(UKismetMathLibrary::StaticClass(), TEXT("Divide_IntInt"))},
        {TEXT("Add_DoubleDouble"),
         MakeTuple(UKismetMathLibrary::StaticClass(), TEXT("Add_DoubleDouble"))},
        {TEXT("Subtract_DoubleDouble"),
         MakeTuple(UKismetMathLibrary::StaticClass(), TEXT("Subtract_DoubleDouble"))},
        {TEXT("Multiply_DoubleDouble"),
         MakeTuple(UKismetMathLibrary::StaticClass(), TEXT("Multiply_DoubleDouble"))},
        {TEXT("Divide_DoubleDouble"),
         MakeTuple(UKismetMathLibrary::StaticClass(), TEXT("Divide_DoubleDouble"))},
        {TEXT("FTrunc"),
         MakeTuple(UKismetMathLibrary::StaticClass(), TEXT("FTrunc"))},
        {TEXT("MakeVector"),
         MakeTuple(UKismetMathLibrary::StaticClass(), TEXT("MakeVector"))},
        {TEXT("BreakVector"),
         MakeTuple(UKismetMathLibrary::StaticClass(), TEXT("BreakVector"))},
        {TEXT("GetComponentByClass"),
         MakeTuple(AActor::StaticClass(), TEXT("GetComponentByClass"))},
        {TEXT("GetWorldTimerManager"),
         MakeTuple(
             UKismetSystemLibrary::StaticClass(),
             TEXT("K2_GetTimerManager"))}};
    return FunctionNodes.Find(NodeType);
}
} // namespace

bool TryCreateCommonFunctionNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    const TTuple<UClass*, const TCHAR*>* FunctionInfo =
        FindCommonFunctionNode(NodeType);
    if (!FunctionInfo)
    {
        return false;
    }

    UClass* Class = FunctionInfo->Get<0>();
    const FString FunctionName = FunctionInfo->Get<1>();
    UFunction* Function = Class->FindFunctionByName(*FunctionName);
    if (!Function)
    {
        const FString MemberHint = SuggestMemberFix(Class, FunctionName);
        Context.SendError(
            FString::Printf(
                TEXT("Could not find function '%s%s::%s' for node type '%s'.%s"),
                Class->GetPrefixCPP(), *Class->GetName(), *FunctionName, *NodeType, *MemberHint),
            TEXT("FUNCTION_NOT_FOUND"));
        return true;
    }

    // nodeType GetAllActorsOfClass with targetClass: the class it looks for, as on the CallFunction spelling.
    FString OutputClassText;
    Context.Payload->TryGetStringField(TEXT("targetClass"), OutputClassText);
    bool bRefused = false;
    UClass* OutputClass = ResolveOutputClass(Context, *Function, OutputClassText, bRefused);
    if (bRefused)
    {
        return true;
    }
    FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Context.TargetGraph);
    UK2Node_CallFunction* Node = NodeCreator.CreateNode(false);
    Node->SetFromFunction(Function);
    ApplyOutputClass(*Node, *Function, OutputClass);
    Context.FinalizeNode(NodeCreator, Node, X, Y);
    return true;
}

UClass* FindNodeClassByName(const FString& NodeType)
{
    static const TMap<FString, FString> NodeTypeAliases = {
        {TEXT("Branch"), TEXT("K2Node_IfThenElse")},
        {TEXT("IfThenElse"), TEXT("K2Node_IfThenElse")},
        {TEXT("Sequence"), TEXT("K2Node_ExecutionSequence")},
        {TEXT("ExecutionSequence"), TEXT("K2Node_ExecutionSequence")},
        {TEXT("Select"), TEXT("K2Node_Select")},
        {TEXT("Switch"), TEXT("K2Node_SwitchInteger")},
        {TEXT("SwitchOnInt"), TEXT("K2Node_SwitchInteger")},
        {TEXT("SwitchOnEnum"), TEXT("K2Node_SwitchEnum")},
        {TEXT("SwitchOnString"), TEXT("K2Node_SwitchString")},
        {TEXT("SwitchOnName"), TEXT("K2Node_SwitchName")},
        {TEXT("MultiGate"), TEXT("K2Node_MultiGate")},
        // ForLoop / WhileLoop / ForEachLoop, DoOnce, DoN, Gate and FlipFlop are
        // StandardMacros library macros (no UK2Node_* class), resolved earlier by
        // TryCreateMacroNode via K2Node_MacroInstance. Listing them here mis-resolved
        // them (nonexistent class -> NODE_TYPE_NOT_FOUND; ForEachLoop -> the wrong
        // enum iterator). Kept out on purpose.
        {TEXT("MakeArray"), TEXT("K2Node_MakeArray")},
        {TEXT("MakeStruct"), TEXT("K2Node_MakeStruct")},
        {TEXT("BreakStruct"), TEXT("K2Node_BreakStruct")},
        {TEXT("MakeMap"), TEXT("K2Node_MakeMap")},
        {TEXT("MakeSet"), TEXT("K2Node_MakeSet")},
        {TEXT("SpawnActorFromClass"), TEXT("K2Node_SpawnActorFromClass")},
        {TEXT("Self"), TEXT("K2Node_Self")},
        {TEXT("GetSelf"), TEXT("K2Node_Self")},
        {TEXT("Timeline"), TEXT("K2Node_Timeline")},
        {TEXT("Knot"), TEXT("K2Node_Knot")},
        {TEXT("Reroute"), TEXT("K2Node_Knot")},
        {TEXT("Comment"), TEXT("EdGraphNode_Comment")},
        {TEXT("Literal"), TEXT("K2Node_Literal")}};

    FString ResolvedName = NodeType;
    if (const FString* Alias = NodeTypeAliases.Find(NodeType))
    {
        ResolvedName = *Alias;
    }

    TArray<FString> NamesToTry{
        ResolvedName,
        FString::Printf(TEXT("K2Node_%s"), *ResolvedName),
        FString::Printf(TEXT("UK2Node_%s"), *ResolvedName)};
    if (ResolvedName != NodeType)
    {
        NamesToTry.Add(NodeType);
        NamesToTry.Add(FString::Printf(TEXT("K2Node_%s"), *NodeType));
        NamesToTry.Add(FString::Printf(TEXT("UK2Node_%s"), *NodeType));
    }

    const auto FindClass = [](const TArray<FString>& Names) -> UClass*
    {
        for (TObjectIterator<UClass> It; It; ++It)
        {
            if (It->IsChildOf(UEdGraphNode::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract) &&
                Names.ContainsByPredicate([&It](const FString& Name) { return It->GetName().Equals(Name, ESearchCase::IgnoreCase); }))
            {
                return *It;
            }
        }
        return nullptr;
    };
    // An animation graph node also goes by the name after its class prefix, spaces allowed (TwoBoneIK, Two Bone IK),
    // tried only after every Blueprint spelling so a K2 node of the same name keeps winning.
    const FString Compact = ResolvedName.Replace(TEXT(" "), TEXT(""));
    UClass* Found = FindClass(NamesToTry);
    return Found ? Found : FindClass(TArray<FString>{Compact, FString::Printf(TEXT("AnimGraphNode_%s"), *Compact)});
}

bool HandleListNodeTypes(FActionContext& Context)
{
    if (Context.SubAction != TEXT("list_node_types"))
    {
        return false;
    }
    // filter: a class or display name containing it (case-insensitive, spaces ignored), as get_graph_details reads it.
    FString Filter;
    if (Context.Payload.IsValid())
    {
        Context.Payload->TryGetStringField(TEXT("filter"), Filter);
    }
    Filter.ReplaceInline(TEXT(" "), TEXT(""));
    TArray<TSharedPtr<FJsonValue>> NodeTypes;
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (!It->IsChildOf(UK2Node::StaticClass()) || It->HasAnyClassFlags(CLASS_Abstract))
        {
            continue;
        }
        const FString DisplayName = It->GetDisplayNameText().ToString();
        if (!Filter.IsEmpty() && !It->GetName().Contains(Filter) && !DisplayName.Replace(TEXT(" "), TEXT("")).Contains(Filter))
        {
            continue;
        }
        TSharedPtr<FJsonObject> TypeObj = McpHandlerUtils::CreateResultObject();
        TypeObj->SetStringField(TEXT("className"), It->GetName());
        TypeObj->SetStringField(TEXT("displayName"), DisplayName);
        NodeTypes.Add(MakeShared<FJsonValueObject>(TypeObj));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("nodeTypes"), NodeTypes);
    Result->SetNumberField(TEXT("count"), NodeTypes.Num());
    Context.SendResponse(TEXT("Node types listed."), Result);
    return true;
}
}
