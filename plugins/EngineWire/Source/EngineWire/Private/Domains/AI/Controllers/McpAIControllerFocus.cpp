#include "Domains/AI/McpAIContext.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"

// The controller actions author their logic into the AIController Blueprint (recipes under Resources/Recipes/AI) as
// plain engine nodes. They used to add an empty FocusActor or AssignedBehaviorTree variable and answer that the focus
// was set and the tree assigned, while nothing focused and no tree ran.
namespace McpAIHandlers
{
namespace
{
constexpr const TCHAR* RunTreeTag = TEXT("MCP run behavior tree");
constexpr const TCHAR* FocusBehaviourTag = TEXT("MCP AI focus");

// The AIController Blueprint at controllerPath, or null once the refusal is sent.
UBlueprint* LoadControllerBlueprint(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                                    TSharedPtr<FMcpBridgeWebSocket> Socket, FString& OutPath)
{
    const FString Path = GetJsonStringField(Payload, TEXT("controllerPath"));
    if (Path.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("controllerPath required: the AIController Blueprint to edit"), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }
    FString Error;
    UBlueprint* Blueprint = LoadBlueprintAsset(Path, OutPath, Error);
    if (!Blueprint)
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Controller Blueprint not found: %s. %s"), *Path, *Error), TEXT("NOT_FOUND"));
        return nullptr;
    }
    const UClass* Class = Blueprint->GeneratedClass ? Blueprint->GeneratedClass.Get() : Blueprint->ParentClass.Get();
    if (!Class || !Class->IsChildOf(AAIController::StaticClass()))
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("%s is not an AIController Blueprint (%s): focus and Behavior Trees belong to the AI controller that possesses the pawn; make one with create_ai_controller"),
                            *Path, Class ? *Class->GetName() : TEXT("no class")),
            TEXT("NOT_AI_CONTROLLER"));
        return nullptr;
    }
    if (OutPath.IsEmpty())
    {
        OutPath = Path;
    }
    return Blueprint;
}

UBehaviorTree* LoadTree(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                        TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString Path = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
    UBehaviorTree* Tree = Path.IsEmpty() ? nullptr : Cast<UBehaviorTree>(McpLoadAsset(Path));
    if (!Tree)
    {
        Self->SendAutomationError(Socket, RequestId,
            Path.IsEmpty() ? FString(TEXT("behaviorTreePath required")) : FString::Printf(TEXT("No Behavior Tree at %s"), *Path),
            Path.IsEmpty() ? TEXT("INVALID_ARGUMENT") : TEXT("NOT_FOUND"));
    }
    return Tree;
}

// The variable that holds the controller's tree: a BehaviorTree variable it declares, else DefaultBehaviorTree.
FString TreeVariable(const UBlueprint* Blueprint)
{
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        const UClass* Type = Cast<UClass>(Variable.VarType.PinSubCategoryObject.Get());
        if (Variable.VarType.PinCategory == UEdGraphSchema_K2::PC_Object && !Variable.VarType.IsContainer() && Type &&
            Type->IsChildOf(UBehaviorTree::StaticClass()))
        {
            return Variable.VarName.ToString();
        }
    }
    return TEXT("DefaultBehaviorTree");
}

// Authors recipe AI/<Name> into the controller; false once a refusal is sent. OutResult.Report drops the batch's own
// bookkeeping (per-step results, node ids).
bool AuthorControllerRecipe(UEngineWireSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                            const FString& Path, const TCHAR* Name, const TMap<FString, TSharedPtr<FJsonValue>>& Values,
                            McpBlueprintBehaviour::FAuthorResult& OutResult)
{
    FString Error;
    const TSharedPtr<FJsonObject> Recipe = McpBlueprintBehaviour::LoadRecipe(TEXT("AI"), Name, Values, Error);
    if (!Recipe)
    {
        Self->SendAutomationError(Socket, RequestId, Error, TEXT("INVALID_RECIPE"));
        return false;
    }
    OutResult = McpBlueprintBehaviour::Author(*Self, RequestId, Socket, Path, Recipe);
    for (const TCHAR* Field : {TEXT("results"), TEXT("nodeIds"), TEXT("stepCount"), TEXT("succeeded")})
    {
        OutResult.Report->RemoveField(Field);
    }
    OutResult.Report->SetStringField(TEXT("controllerPath"), Path);
    return true;
}

// assign_behavior_tree and run_behavior_tree: the tree goes in the controller's variable; run also runs it on possess.
bool AuthorTree(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                TSharedPtr<FMcpBridgeWebSocket> Socket, bool bRun)
{
    FString Path;
    UBlueprint* Blueprint = LoadControllerBlueprint(Self, RequestId, Payload, Socket, Path);
    UBehaviorTree* Tree = Blueprint ? LoadTree(Self, RequestId, Payload, Socket) : nullptr;
    if (!Tree)
    {
        return true;
    }
    const FString Variable = TreeVariable(Blueprint);
    TMap<FString, TSharedPtr<FJsonValue>> Values;
    Values.Add(TEXT("VariableName"), MakeShared<FJsonValueString>(Variable));
    Values.Add(TEXT("BehaviorTreePath"), MakeShared<FJsonValueString>(Tree->GetPathName()));
    McpBlueprintBehaviour::FAuthorResult Result;
    if (!AuthorControllerRecipe(Self, RequestId, Socket, Path, bRun ? TEXT("RunBehaviorTree") : TEXT("AssignBehaviorTree"), Values, Result))
    {
        return true;
    }
    Result.Report->SetStringField(TEXT("behaviorTreePath"), Tree->GetPathName());
    Result.Report->SetStringField(TEXT("variable"), Variable);
    const FString Done = bRun
        ? FString::Printf(TEXT("%s runs %s when it possesses a pawn (Run Behavior Tree on On Possess, reading its variable %s)"), *Path, *Tree->GetName(), *Variable)
        : FString::Printf(TEXT("%s holds %s in its variable %s; nothing runs it until run_behavior_tree or your own Run Behavior Tree call"), *Path, *Tree->GetName(), *Variable);
    Self->SendAutomationResponse(Socket, RequestId, Result.bApplied, Result.bApplied ? Done : Result.Message, Result.Report, Result.ErrorCode);
    return true;
}

// stop_behavior_tree and clear_focus: take out what the paired action authored, or say nothing of it was there.
bool RemoveControllerBehaviour(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                               TSharedPtr<FMcpBridgeWebSocket> Socket, const TCHAR* Tag, const TCHAR* Recipe, const TCHAR* What)
{
    FString Path;
    UBlueprint* Blueprint = LoadControllerBlueprint(Self, RequestId, Payload, Socket, Path);
    if (!Blueprint)
    {
        return true;
    }
    if (McpBlueprintBehaviour::FindOwned(Blueprint, Tag).Num() == 0)
    {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("controllerPath"), Path);
        Result->SetBoolField(TEXT("removed"), false);
        Self->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("%s has no %s that this tool authored; nothing changed"), *Path, What), Result);
        return true;
    }
    McpBlueprintBehaviour::FAuthorResult Result;
    if (!AuthorControllerRecipe(Self, RequestId, Socket, Path, Recipe, {}, Result))
    {
        return true;
    }
    Result.Report->SetBoolField(TEXT("removed"), Result.bApplied);
    Self->SendAutomationResponse(Socket, RequestId, Result.bApplied,
        Result.bApplied ? FString::Printf(TEXT("Removed the %s from %s"), What, *Path) : Result.Message, Result.Report, Result.ErrorCode);
    return true;
}
} // namespace

bool HandleSetFocus(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    bool bPlayer = false;
    Payload->TryGetBoolField(TEXT("focusPlayer"), bPlayer);
    const FString Tag = GetJsonStringField(Payload, TEXT("focusTag")).TrimStartAndEnd();
    if (bPlayer == !Tag.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("set_focus needs one target: focusPlayer true (the player's pawn) or focusTag (the first actor carrying that tag)"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FString Path;
    if (!LoadControllerBlueprint(Self, RequestId, Payload, RequestingSocket, Path))
    {
        return true;
    }
    TMap<FString, TSharedPtr<FJsonValue>> Values;
    if (!bPlayer)
    {
        Values.Add(TEXT("FocusTag"), MakeShared<FJsonValueString>(Tag));
    }
    McpBlueprintBehaviour::FAuthorResult Result;
    if (!AuthorControllerRecipe(Self, RequestId, RequestingSocket, Path, bPlayer ? TEXT("FocusPlayer") : TEXT("FocusTag"), Values, Result))
    {
        return true;
    }
    Result.Report->SetStringField(TEXT("focusTarget"), bPlayer ? FString(TEXT("player")) : TEXT("tag ") + Tag);
    const FString Done = FString::Printf(TEXT("%s keeps its focus on %s: each frame it has none, it finds the target and sets it"),
                                         *Path, bPlayer ? TEXT("the player's pawn") : *FString::Printf(TEXT("the first actor tagged %s"), *Tag));
    Self->SendAutomationResponse(RequestingSocket, RequestId, Result.bApplied, Result.bApplied ? Done : Result.Message, Result.Report, Result.ErrorCode);
    return true;
}

bool HandleClearFocus(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return RemoveControllerBehaviour(Self, RequestId, Payload, RequestingSocket, FocusBehaviourTag, TEXT("ClearFocus"), TEXT("focus logic"));
}

bool HandleAssignBehaviorTree(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return AuthorTree(Self, RequestId, Payload, RequestingSocket, false);
}

bool HandleRunBehaviorTree(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return AuthorTree(Self, RequestId, Payload, RequestingSocket, true);
}

bool HandleStopBehaviorTree(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return RemoveControllerBehaviour(Self, RequestId, Payload, RequestingSocket, RunTreeTag, TEXT("StopBehaviorTree"), TEXT("Behavior Tree run logic"));
}
} // namespace McpAIHandlers
