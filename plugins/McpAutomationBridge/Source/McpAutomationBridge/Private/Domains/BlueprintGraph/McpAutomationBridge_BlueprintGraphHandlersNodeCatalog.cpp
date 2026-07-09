#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

namespace McpBlueprintGraphHandlers
{
const TTuple<FString, FString>* FindCommonFunctionNode(
    const FString& NodeType)
{
    static const TMap<FString, TTuple<FString, FString>> FunctionNodes = {
        {TEXT("PrintString"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("PrintString"))},
        {TEXT("Print"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("PrintString"))},
        {TEXT("PrintText"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("PrintText"))},
        {TEXT("SetActorLocation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_SetActorLocation"))},
        {TEXT("GetActorLocation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_GetActorLocation"))},
        {TEXT("SetActorRotation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_SetActorRotation"))},
        {TEXT("GetActorRotation"),
         MakeTuple(TEXT("AActor"), TEXT("K2_GetActorRotation"))},
        {TEXT("SetActorTransform"),
         MakeTuple(TEXT("AActor"), TEXT("K2_SetActorTransform"))},
        {TEXT("GetActorTransform"),
         MakeTuple(TEXT("AActor"), TEXT("K2_GetActorTransform"))},
        {TEXT("AddActorLocalOffset"),
         MakeTuple(TEXT("AActor"), TEXT("K2_AddActorLocalOffset"))},
        {TEXT("Delay"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("Delay"))},
        {TEXT("DestroyActor"),
         MakeTuple(TEXT("AActor"), TEXT("K2_DestroyActor"))},
        {TEXT("SpawnActor"),
         MakeTuple(
             TEXT("UGameplayStatics"),
             TEXT("BeginDeferredActorSpawnFromClass"))},
        {TEXT("GetPlayerPawn"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("GetPlayerPawn"))},
        {TEXT("GetPlayerController"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("GetPlayerController"))},
        {TEXT("PlaySound"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("PlaySound2D"))},
        {TEXT("PlaySound2D"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("PlaySound2D"))},
        {TEXT("PlaySoundAtLocation"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("PlaySoundAtLocation"))},
        {TEXT("GetWorldDeltaSeconds"),
         MakeTuple(TEXT("UGameplayStatics"), TEXT("GetWorldDeltaSeconds"))},
        {TEXT("SetTimerByFunctionName"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("K2_SetTimer"))},
        {TEXT("ClearTimer"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("K2_ClearTimer"))},
        {TEXT("IsValid"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("IsValid"))},
        {TEXT("IsValidClass"),
         MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("IsValidClass"))},
        {TEXT("Add_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Add_IntInt"))},
        {TEXT("Subtract_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Subtract_IntInt"))},
        {TEXT("Multiply_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Multiply_IntInt"))},
        {TEXT("Divide_IntInt"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Divide_IntInt"))},
        {TEXT("Add_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Add_DoubleDouble"))},
        {TEXT("Subtract_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Subtract_DoubleDouble"))},
        {TEXT("Multiply_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Multiply_DoubleDouble"))},
        {TEXT("Divide_DoubleDouble"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("Divide_DoubleDouble"))},
        {TEXT("FTrunc"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("FTrunc"))},
        {TEXT("MakeVector"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("MakeVector"))},
        {TEXT("BreakVector"),
         MakeTuple(TEXT("UKismetMathLibrary"), TEXT("BreakVector"))},
        {TEXT("GetComponentByClass"),
         MakeTuple(TEXT("AActor"), TEXT("GetComponentByClass"))},
        {TEXT("GetWorldTimerManager"),
         MakeTuple(
             TEXT("UKismetSystemLibrary"),
             TEXT("K2_GetTimerManager"))}};
    return FunctionNodes.Find(NodeType);
}

bool TryCreateCommonFunctionNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    const TTuple<FString, FString>* FunctionInfo =
        FindCommonFunctionNode(NodeType);
    if (!FunctionInfo)
    {
        return false;
    }

    const FString ClassName = FunctionInfo->Get<0>();
    const FString FunctionName = FunctionInfo->Get<1>();
    UClass* Class = nullptr;
    if (ClassName == TEXT("UKismetSystemLibrary"))
    {
        Class = UKismetSystemLibrary::StaticClass();
    }
    else if (ClassName == TEXT("UGameplayStatics"))
    {
        Class = UGameplayStatics::StaticClass();
    }
    else if (ClassName == TEXT("AActor"))
    {
        Class = AActor::StaticClass();
    }
    else if (ClassName == TEXT("UKismetMathLibrary"))
    {
        Class = UKismetMathLibrary::StaticClass();
    }
    else
    {
        Class = ResolveUClass(ClassName);
    }

    UFunction* Function =
        Class ? Class->FindFunctionByName(*FunctionName) : nullptr;
    if (!Function)
    {
        Context.SendError(
            FString::Printf(
                TEXT("Could not find function '%s::%s' for node type '%s'"),
                *ClassName,
                *FunctionName,
                *NodeType),
            TEXT("FUNCTION_NOT_FOUND"));
        return true;
    }

    FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Context.TargetGraph);
    UK2Node_CallFunction* Node = NodeCreator.CreateNode(false);
    Node->SetFromFunction(Function);
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
        {TEXT("DoOnce"), TEXT("K2Node_DoOnce")},
        {TEXT("DoN"), TEXT("K2Node_DoN")},
        {TEXT("FlipFlop"), TEXT("K2Node_FlipFlop")},
        {TEXT("Gate"), TEXT("K2Node_Gate")},
        {TEXT("MultiGate"), TEXT("K2Node_MultiGate")},
        // ForLoop / ForLoopWithBreak / WhileLoop / ForEachLoop are StandardMacros
        // library macros (no UK2Node_* class), resolved earlier by
        // TryCreateMacroNode via K2Node_MacroInstance. Listing them here mis-resolved
        // them (nonexistent class -> NODE_TYPE_NOT_FOUND; ForEachLoop -> the wrong
        // enum iterator). Kept out on purpose.
        {TEXT("MakeArray"), TEXT("K2Node_MakeArray")},
        {TEXT("MakeStruct"), TEXT("K2Node_MakeStruct")},
        {TEXT("BreakStruct"), TEXT("K2Node_BreakStruct")},
        {TEXT("MakeMap"), TEXT("K2Node_MakeMap")},
        {TEXT("MakeSet"), TEXT("K2Node_MakeSet")},
        {TEXT("SpawnActorFromClass"), TEXT("K2Node_SpawnActorFromClass")},
        {TEXT("GetAllActorsOfClass"), TEXT("K2Node_GetAllActorsOfClass")},
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

    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (!It->IsChildOf(UEdGraphNode::StaticClass()) ||
            It->HasAnyClassFlags(CLASS_Abstract))
        {
            continue;
        }
        for (const FString& NameToMatch : NamesToTry)
        {
            if (It->GetName().Equals(
                    NameToMatch,
                    ESearchCase::IgnoreCase))
            {
                return *It;
            }
        }
    }
    return nullptr;
}
}
#endif
