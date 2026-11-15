// McpLevelStructureBPNodeAliases.cpp — friendly node names for add_level_blueprint_node, as data: the node type blueprint's
// create_node takes and what to bind (an event, a function, the class that owns the function when it is not the level Blueprint
// itself or one of the Kismet System, Gameplay Statics, Math, String and Text libraries).
#include "Domains/LevelStructure/McpLevelStructureActions.h"

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

namespace McpLevelStructure
{
bool ResolveLevelBlueprintNodeAlias(FString& InOutNodeType, FString& OutEventName, FString& OutFunctionName, FString& OutMemberClass)
{
    const FString Key = McpPlainKey(InOutNodeType);
    OutEventName.Reset();
    OutFunctionName.Reset();
    OutMemberClass.Reset();
    struct FAlias { const TCHAR* Key; const TCHAR* NodeType; const TCHAR* Event; const TCHAR* Function; const TCHAR* Owner; };
    static const FAlias Aliases[] = {
        {TEXT("eventbeginplay"), TEXT("K2Node_Event"), TEXT("ReceiveBeginPlay"), nullptr, nullptr},
        {TEXT("beginplay"), TEXT("K2Node_Event"), TEXT("ReceiveBeginPlay"), nullptr, nullptr},
        {TEXT("receivebeginplay"), TEXT("K2Node_Event"), TEXT("ReceiveBeginPlay"), nullptr, nullptr},
        {TEXT("eventtick"), TEXT("K2Node_Event"), TEXT("ReceiveTick"), nullptr, nullptr},
        {TEXT("tick"), TEXT("K2Node_Event"), TEXT("ReceiveTick"), nullptr, nullptr},
        {TEXT("receivetick"), TEXT("K2Node_Event"), TEXT("ReceiveTick"), nullptr, nullptr},
        {TEXT("eventendplay"), TEXT("K2Node_Event"), TEXT("ReceiveEndPlay"), nullptr, nullptr},
        {TEXT("endplay"), TEXT("K2Node_Event"), TEXT("ReceiveEndPlay"), nullptr, nullptr},
        {TEXT("printstring"), TEXT("K2Node_CallFunction"), nullptr, TEXT("PrintString"), nullptr},
        {TEXT("print"), TEXT("K2Node_CallFunction"), nullptr, TEXT("PrintString"), nullptr},
        {TEXT("printtext"), TEXT("K2Node_CallFunction"), nullptr, TEXT("PrintText"), nullptr},
        {TEXT("delay"), TEXT("K2Node_CallFunction"), nullptr, TEXT("Delay"), nullptr},
        // Gameplay aliases.
        {TEXT("getplayerpawn"), TEXT("K2Node_CallFunction"), nullptr, TEXT("GetPlayerPawn"), nullptr},
        {TEXT("getplayercharacter"), TEXT("K2Node_CallFunction"), nullptr, TEXT("GetPlayerCharacter"), nullptr},
        {TEXT("getallactorsofclass"), TEXT("K2Node_CallFunction"), nullptr, TEXT("GetAllActorsOfClass"), nullptr},
        {TEXT("getgameplaystatics"), TEXT("K2Node_CallFunction"), nullptr, TEXT("GetGameInstance"), nullptr},
        {TEXT("applydamage"), TEXT("K2Node_CallFunction"), nullptr, TEXT("ApplyDamage"), nullptr},
        {TEXT("spawnsound2d"), TEXT("K2Node_CallFunction"), nullptr, TEXT("SpawnSound2D"), nullptr},
        {TEXT("spawnsoundatlocation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("SpawnSoundAtLocation"), nullptr},
        {TEXT("spawnsystematlocation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("SpawnSystemAtLocation"), TEXT("/Script/Niagara.NiagaraFunctionLibrary")},
        {TEXT("spawnemitteratlocation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("SpawnEmitterAtLocation"), nullptr},
        {TEXT("maketransform"), TEXT("K2Node_CallFunction"), nullptr, TEXT("MakeTransform"), nullptr},
        {TEXT("makevector"), TEXT("K2Node_CallFunction"), nullptr, TEXT("MakeVector"), nullptr},
        {TEXT("randomfloatinrange"), TEXT("K2Node_CallFunction"), nullptr, TEXT("RandomFloatInRange"), nullptr},
        {TEXT("randomintegerinrange"), TEXT("K2Node_CallFunction"), nullptr, TEXT("RandomIntegerInRange"), nullptr},
        {TEXT("settimer"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_SetTimer"), nullptr},
        {TEXT("k2settimer"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_SetTimer"), nullptr},
        {TEXT("setactorlocation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_SetActorLocation"), nullptr},
        {TEXT("getactorlocation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_GetActorLocation"), nullptr},
        {TEXT("setactorrotation"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_SetActorRotation"), nullptr},
        {TEXT("destroyactor"), TEXT("K2Node_CallFunction"), nullptr, TEXT("K2_DestroyActor"), nullptr},
        {TEXT("setlifespan"), TEXT("K2Node_CallFunction"), nullptr, TEXT("SetLifeSpan"), nullptr},
        {TEXT("branch"), TEXT("K2Node_IfThenElse"), nullptr, nullptr, nullptr},
        {TEXT("ifthenelse"), TEXT("K2Node_IfThenElse"), nullptr, nullptr, nullptr},
        {TEXT("sequence"), TEXT("K2Node_ExecutionSequence"), nullptr, nullptr, nullptr},
        {TEXT("executionsequence"), TEXT("K2Node_ExecutionSequence"), nullptr, nullptr, nullptr},
        {TEXT("spawnactorfromclass"), TEXT("K2Node_SpawnActorFromClass"), nullptr, nullptr, nullptr},
        // Standard macros: create_node takes the macro itself as the node type (the bare IsValid is the pure function; the exec
        // macro is K2Node_IsValid, and MultiGate is a node class of its own).
        {TEXT("foreachloop"), TEXT("ForEachLoop"), nullptr, nullptr, nullptr},
        {TEXT("forloop"), TEXT("ForLoop"), nullptr, nullptr, nullptr},
        {TEXT("whileloop"), TEXT("WhileLoop"), nullptr, nullptr, nullptr},
        {TEXT("doonce"), TEXT("DoOnce"), nullptr, nullptr, nullptr},
        {TEXT("gate"), TEXT("Gate"), nullptr, nullptr, nullptr},
        {TEXT("multigate"), TEXT("K2Node_MultiGate"), nullptr, nullptr, nullptr},
        {TEXT("flipflop"), TEXT("FlipFlop"), nullptr, nullptr, nullptr},
        {TEXT("isvalid"), TEXT("K2Node_IsValid"), nullptr, nullptr, nullptr},
    };
    for (const FAlias& Alias : Aliases)
    {
        if (Key == Alias.Key)
        {
            InOutNodeType = Alias.NodeType;
            OutEventName = Alias.Event ? Alias.Event : TEXT("");
            OutFunctionName = Alias.Function ? Alias.Function : TEXT("");
            OutMemberClass = Alias.Owner ? Alias.Owner : TEXT("");
            return true;
        }
    }
    return false;
}

}
