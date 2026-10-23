// explain_widget: why a widget is not on screen. During Play: whether an instance of the class exists, sits in the
// viewport (or inside another widget), and is visible, opaque and sized. Outside Play: whether a player Blueprint (the
// game mode's controller, pawn or HUD, or the level's own) has a Create Widget node for the class and adds it.
#include "Domains/Debug/Explain/McpExplain.h"

#include "Blueprint/UserWidget.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "K2Node_CallFunction.h"
#include "UObject/UObjectIterator.h"

namespace McpExplain
{
namespace
{
// A Create Widget node for Class (the node of the editor module, read by class name and its Class pin).
bool CreatesWidget(const UBlueprint* Blueprint, const UClass* Class)
{
    return AnyNode(Blueprint, [Class](const UEdGraphNode* Node) {
        const UEdGraphPin* Pin = Node->GetClass()->GetName() == TEXT("K2Node_CreateWidget") ? Node->FindPin(TEXT("Class")) : nullptr;
        return Pin && (Pin->DefaultObject == Class || Pin->LinkedTo.Num() > 0);
    });
}

bool AddsToScreen(const UBlueprint* Blueprint)
{
    return AnyNode(Blueprint, [](const UEdGraphNode* Node) {
        const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
        const FName Function = Call ? Call->FunctionReference.GetMemberName() : NAME_None;
        return Function == TEXT("AddToViewport") || Function == TEXT("AddToPlayerScreen");
    });
}

void CheckStatic(FTree& Tree, UWorld* World, const UClass* Class)
{
    const TArray<UBlueprint*> Blueprints = PlayerBlueprints(World);
    const UBlueprint* Creator = nullptr;
    for (const UBlueprint* Blueprint : Blueprints)
    {
        Creator = Creator ? Creator : (CreatesWidget(Blueprint, Class) ? Blueprint : nullptr);
    }
    if (!Creator)
    {
        Tree.Fail(TEXT("created"), TEXT("NOT_CREATED_ANYWHERE"),
                  FString::Printf(TEXT("No Create Widget node for '%s' is in the player controller, pawn, HUD or level Blueprint, so Play never makes one."), *Class->GetName()));
        return;
    }
    Tree.Pass(TEXT("created"), FString::Printf(TEXT("'%s' has a Create Widget node for it."), *Creator->GetName()));
    if (AddsToScreen(Creator))
    {
        Tree.Pass(TEXT("added"), FString::Printf(TEXT("'%s' adds a widget to the viewport."), *Creator->GetName()));
    }
    else
    {
        Tree.Fail(TEXT("added"), TEXT("NOT_ADDED"), FString::Printf(TEXT("'%s' creates it but never calls Add to Viewport, so it is made and never shown."), *Creator->GetName()));
    }
    Tree.Unknown(TEXT("running"), TEXT("Whether it is on screen is read during Play: ask again while the game runs."));
}

void CheckLive(FTree& Tree, UWorld* World, const UClass* Class)
{
    TArray<UUserWidget*> Instances;
    for (TObjectIterator<UUserWidget> It; It; ++It)
    {
        if (It->GetClass()->IsChildOf(Class) && It->GetWorld() == World && !It->IsTemplate())
        {
            Instances.Add(*It);
        }
    }
    if (Instances.Num() == 0)
    {
        Tree.Fail(TEXT("created"), TEXT("NOT_CREATED"), FString::Printf(TEXT("No '%s' exists in the running game: nothing created it (Create Widget)."), *Class->GetName()));
        CheckStatic(Tree, McpHandlerUtils::GetEditorWorld(), Class);
        return;
    }
    const UUserWidget* Widget = Instances[0];
    for (const UUserWidget* Candidate : Instances)
    {
        Widget = Candidate->IsInViewport() ? Candidate : Widget;
    }
    Tree.Pass(TEXT("created"), FString::Printf(TEXT("%d instance(s) exist in the running game."), Instances.Num()));
    if (!Widget->IsInViewport() && !Widget->GetParent())
    {
        Tree.Fail(TEXT("added"), TEXT("NOT_IN_VIEWPORT"), TEXT("It was created but never added to the viewport (Add to Viewport) or to another widget."));
    }
    const ESlateVisibility Visibility = Widget->GetVisibility();
    if (Visibility == ESlateVisibility::Collapsed || Visibility == ESlateVisibility::Hidden)
    {
        Tree.Fail(TEXT("visibility"), TEXT("HIDDEN"), FString::Printf(TEXT("Its visibility is %s."), Visibility == ESlateVisibility::Collapsed ? TEXT("Collapsed") : TEXT("Hidden")));
    }
    if (Widget->GetRenderOpacity() <= 0.01f)
    {
        Tree.Fail(TEXT("opacity"), TEXT("TRANSPARENT"), FString::Printf(TEXT("Its render opacity is %.2f."), Widget->GetRenderOpacity()));
    }
    if (Widget->GetDesiredSize().IsNearlyZero())
    {
        Tree.Fail(TEXT("size"), TEXT("ZERO_SIZE"), TEXT("Its desired size is 0 by 0: it has nothing to draw, or its content is collapsed."));
    }
    if (!Tree.HasFailed())
    {
        Tree.Pass(TEXT("onScreen"), TEXT("An instance is in the viewport, visible, opaque and sized."));
    }
}
} // namespace

bool ExplainWidget(UEngineWireSubsystem& Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                   TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const FString Path = GetJsonStringField(Payload, TEXT("widgetPath"));
    const UBlueprint* Blueprint = Path.IsEmpty() ? nullptr : LoadObject<UBlueprint>(nullptr, *Path);
    const UClass* Class = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
    if (!Class || !Class->IsChildOf(UUserWidget::StaticClass()))
    {
        Self.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No Widget Blueprint at '%s': widgetPath names the widget that should show."), *Path), TEXT("WIDGET_NOT_FOUND"));
        return true;
    }
    FTree Tree;
    if (GEditor && GEditor->PlayWorld)
    {
        CheckLive(Tree, GEditor->PlayWorld.Get(), Class);
    }
    else
    {
        CheckStatic(Tree, McpHandlerUtils::GetEditorWorld(), Class);
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Tree.WriteTo(Result);
    Self.SendAutomationResponse(Socket, RequestId, true, Tree.Summary(FString::Printf(TEXT("Why '%s' is not on screen"), *Blueprint->GetName())), Result);
    return true;
}
} // namespace McpExplain
