// duplicate_widget: a copy of a widget and everything under it (a panel row with its label, icon and
// button), seated under a parent of choice. Building ten list rows took ten full re-authorings.
#include "Domains/WidgetAuthoring/McpWidgetActions.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetGuidRegistry.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetTreeMutation.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetValidate.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "EngineWireSubsystem.h"
#include "UObject/UObjectGlobals.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

namespace
{
// Past the tree's widgets and objects, and past a name a member of the Blueprint holds (a variable, a function,
// UWidget's DisplayLabel): a copy under one was saved, then failed to compile behind a success reply.
FName McpFreeCopyName(UWidgetBlueprint* WidgetBP, const FString& Wanted)
{
    FName Name(*Wanted);
    for (int32 Suffix = 1; StaticFindObjectFast(nullptr, WidgetBP->WidgetTree, Name) || !McpWidgetNameConflict(WidgetBP, Name).IsEmpty();
         ++Suffix)
    {
        Name = FName(*FString::Printf(TEXT("%s_%d"), *Wanted, Suffix));
    }
    return Name;
}

// Children are copied first and handed to the parent's copy as a duplication seed, so every slot the
// parent's copy carries points at a copied child. A plain duplicate of a panel shares the original
// children through its slots, and the first edit of either tree then corrupts the other.
UWidget* McpCopySubtree(UWidgetBlueprint* WidgetBP, UWidget* Source, const FString& RootName, TArray<TSharedPtr<FJsonValue>>& Names)
{
    TMap<UObject*, UObject*> Seed;
    if (UPanelWidget* Panel = Cast<UPanelWidget>(Source))
    {
        for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
        {
            if (UWidget* Child = Panel->GetChildAt(Index))
            {
                Seed.Add(Child, McpCopySubtree(WidgetBP, Child, FString(), Names));
            }
        }
    }
    // Named after the children exist: a newName equal to a child's "<name>_Copy" was taken
    // by then, and duplicating onto it replaced that child in place.
    const FName CopyName = McpFreeCopyName(WidgetBP, RootName.IsEmpty() ? Source->GetName() + TEXT("_Copy") : RootName);
    FObjectDuplicationParameters Params = InitStaticDuplicateObjectParams(Source, WidgetBP->WidgetTree, CopyName);
    Params.DuplicationSeed = MoveTemp(Seed);
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
    UEngineWireSubsystem& Subsystem,
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
    UWidgetBlueprint* WidgetBP = nullptr;
    UWidget* Source = ResolveWidgetTarget(Subsystem, RequestId, RequestingSocket, Payload, WidgetBP);
    if (!Source)
    {
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
    UWidget* Copy = McpCopySubtree(WidgetBP, Source, GetJsonStringField(Payload, TEXT("newName")), Names);
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
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
    // The copies are variables: without a compile the generated class has no property for them, and an
    // edit_graph step naming one failed as "not marked as a variable".
    CompileAndSaveWidgetBlueprint(WidgetBP, ResultJson);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetAuthoringHelpers::WidgetBlueprintPackagePath(WidgetBP));
    ResultJson->SetStringField(TEXT("slotName"), Copy->GetName());
    ResultJson->SetStringField(TEXT("newParent"), Parent->GetName());
    ResultJson->SetNumberField(TEXT("index"), Parent->GetChildIndex(Copy));
    ResultJson->SetArrayField(TEXT("copiedWidgets"), Names);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(
        TEXT("Copied '%s' and %d widget(s) under it as '%s'"), *SlotName, Names.Num() - 1, *Copy->GetName()), ResultJson);
    return true;
}
}
