// duplicate_widget: a copy of a widget and everything under it (a panel row with its label, icon and
// button), seated under a parent of choice. Building ten list rows took ten full re-authorings.
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

namespace
{
FName McpFreeCopyName(UWidgetTree* Tree, const FString& Wanted)
{
    FName Name(*Wanted);
    for (int32 Suffix = 1; Tree->FindWidget(Name) || StaticFindObjectFast(nullptr, Tree, Name); ++Suffix)
    {
        Name = FName(*FString::Printf(TEXT("%s_%d"), *Wanted, Suffix));
    }
    return Name;
}

// Children are copied first and handed to the parent's copy as a duplication seed, so every slot the
// parent's copy carries points at a copied child. A plain duplicate of a panel shares the original
// children through its slots, and the first edit of either tree then corrupts the other.
UWidget* McpCopySubtree(UWidgetTree* Tree, UWidget* Source, const FString& RootName, TArray<TSharedPtr<FJsonValue>>& Names)
{
    const FName CopyName = McpFreeCopyName(Tree, RootName.IsEmpty() ? Source->GetName() + TEXT("_Copy") : RootName);
    FObjectDuplicationParameters Params = InitStaticDuplicateObjectParams(Source, Tree, CopyName);
    if (UPanelWidget* Panel = Cast<UPanelWidget>(Source))
    {
        for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
        {
            if (UWidget* Child = Panel->GetChildAt(Index))
            {
                Params.DuplicationSeed.Add(Child, McpCopySubtree(Tree, Child, FString(), Names));
            }
        }
    }
    UWidget* Copy = Cast<UWidget>(StaticDuplicateObjectEx(Params));
    if (!Copy)
    {
        return nullptr;
    }
    // Its own slot still names the original parent; the copy is seated afresh by the caller.
    Copy->Slot = nullptr;
    if (UPanelWidget* CopyPanel = Cast<UPanelWidget>(Copy))
    {
        for (UPanelSlot* CopySlot : CopyPanel->GetSlots())
        {
            if (CopySlot && CopySlot->Content)
            {
                CopySlot->Content->Slot = CopySlot;
            }
        }
    }
    TSharedPtr<FJsonObject> Pair = MakeShared<FJsonObject>();
    Pair->SetStringField(TEXT("source"), Source->GetName());
    Pair->SetStringField(TEXT("copy"), Copy->GetName());
    Names.Add(MakeShared<FJsonValueObject>(Pair));
    return Copy;
}
}

bool HandleWidgetAuthoringDuplicate(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (!SubAction.Equals(TEXT("duplicate_widget"), ESearchCase::IgnoreCase))
    {
        return false;
    }
    const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    const FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
    UWidgetBlueprint* WidgetBP = WidgetPath.IsEmpty() ? nullptr : LoadWidgetBlueprint(WidgetPath);
    UWidget* Source = WidgetBP && WidgetBP->WidgetTree ? WidgetBP->WidgetTree->FindWidget(FName(*SlotName)) : nullptr;
    if (!Source)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, WidgetBP
            ? FString::Printf(TEXT("Widget '%s' not found in %s (get_widget_info lists the tree)."), *SlotName, *WidgetPath)
            : FString::Printf(TEXT("Widget Blueprint '%s' not found."), *WidgetPath), TEXT("NOT_FOUND"));
        return true;
    }
    // Under the source's own parent by default, right after it.
    const FString NewParent = GetJsonStringField(Payload, TEXT("newParent"));
    UPanelWidget* Parent = NewParent.IsEmpty() ? Source->GetParent()
                                               : Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*NewParent)));
    if (!Parent)
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, NewParent.IsEmpty()
            ? FString::Printf(TEXT("'%s' is the root and has no parent to hold a copy; pass newParent."), *SlotName)
            : FString::Printf(TEXT("newParent '%s' is not a panel in %s."), *NewParent, *WidgetPath), TEXT("INVALID_PARENT"));
        return true;
    }
    WidgetBP->Modify();
    TArray<TSharedPtr<FJsonValue>> Names;
    UWidget* Copy = McpCopySubtree(WidgetBP->WidgetTree, Source, GetJsonStringField(Payload, TEXT("newName")), Names);
    if (!Copy || !SafeAddWidgetToTree(WidgetBP, Copy, Parent->GetName()))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
            TEXT("'%s' could not be copied under '%s'. Nothing was added."), *SlotName, *Parent->GetName()), TEXT("DUPLICATE_FAILED"));
        return true;
    }
    CarrySlotLayout(Source->Slot, Copy->Slot);
    const int32 Wanted = Payload->HasField(TEXT("index"))
        ? static_cast<int32>(GetJsonNumberField(Payload, TEXT("index"), 0))
        : (Parent == Source->GetParent() ? Parent->GetChildIndex(Source) + 1 : Parent->GetChildrenCount() - 1);
    Parent->ShiftChild(FMath::Clamp(Wanted, 0, Parent->GetChildrenCount() - 1), Copy);
    RegisterAllWidgetGuids(WidgetBP);
    MarkWidgetBlueprintModifiedAndSave(WidgetBP);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
    ResultJson->SetStringField(TEXT("slotName"), Copy->GetName());
    ResultJson->SetStringField(TEXT("newParent"), Parent->GetName());
    ResultJson->SetNumberField(TEXT("index"), Parent->GetChildIndex(Copy));
    ResultJson->SetArrayField(TEXT("copiedWidgets"), Names);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(
        TEXT("Copied '%s' and %d widget(s) under it as '%s'"), *SlotName, Names.Num() - 1, *Copy->GetName()), ResultJson);
    return true;
}
}
