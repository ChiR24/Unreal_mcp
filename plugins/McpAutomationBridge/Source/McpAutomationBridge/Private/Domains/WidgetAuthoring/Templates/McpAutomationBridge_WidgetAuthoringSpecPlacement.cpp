#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringSpec.h"

#include "Animation/WidgetAnimation.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Domains/WidgetAuthoring/Layout/McpAutomationBridge_WidgetAuthoringSlotAlignment.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringAnimationKeys.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringTreeMutation.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
namespace
{
bool SlotNumbers(const TSharedPtr<FJsonObject>& SlotSpec, const TCHAR* Key, int32 Count, TArray<double>& Out)
{
    Out.Reset();
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!SlotSpec->TryGetArrayField(Key, Values) || Values->Num() != Count)
    {
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        Out.Add(Value->AsNumber());
    }
    return true;
}

void ApplyCanvasSpec(UCanvasPanelSlot* Canvas, const TSharedPtr<FJsonObject>& SlotSpec)
{
    TArray<double> V;
    if (SlotNumbers(SlotSpec, TEXT("anchors"), 4, V)) { Canvas->SetAnchors(FAnchors(V[0], V[1], V[2], V[3])); }
    if (SlotNumbers(SlotSpec, TEXT("alignment"), 2, V)) { Canvas->SetAlignment(FVector2D(V[0], V[1])); }
    if (SlotNumbers(SlotSpec, TEXT("offsets"), 4, V)) { Canvas->SetOffsets(FMargin(V[0], V[1], V[2], V[3])); }
    if (SlotNumbers(SlotSpec, TEXT("position"), 2, V)) { Canvas->SetPosition(FVector2D(V[0], V[1])); }
    if (SlotNumbers(SlotSpec, TEXT("size"), 2, V)) { Canvas->SetSize(FVector2D(V[0], V[1])); }
    bool bAutoSize = false;
    if (SlotSpec->TryGetBoolField(TEXT("autoSize"), bAutoSize)) { Canvas->SetAutoSize(bAutoSize); }
    double ZOrder = 0.0;
    if (SlotSpec->TryGetNumberField(TEXT("z"), ZOrder)) { Canvas->SetZOrder(static_cast<int32>(ZOrder)); }
}
}

void McpApplySpecSlot(UWidget* Widget, const TSharedPtr<FJsonObject>& SlotSpec)
{
    UPanelSlot* Slot = Widget ? Widget->Slot : nullptr;
    if (!Slot || !SlotSpec.IsValid())
    {
        return;
    }
    if (UCanvasPanelSlot* Canvas = Cast<UCanvasPanelSlot>(Slot))
    {
        ApplyCanvasSpec(Canvas, SlotSpec);
        return;
    }
    TArray<double> V;
    FStructProperty* PaddingProp = FindFProperty<FStructProperty>(Slot->GetClass(), TEXT("Padding"));
    if (PaddingProp && PaddingProp->Struct == TBaseStructure<FMargin>::Get() && SlotSpec->HasField(TEXT("padding")))
    {
        *PaddingProp->ContainerPtrToValuePtr<FMargin>(Slot) = SlotNumbers(SlotSpec, TEXT("padding"), 4, V)
            ? FMargin(V[0], V[1], V[2], V[3]) : FMargin(static_cast<float>(SlotSpec->GetNumberField(TEXT("padding"))));
    }
    uint8 Align = 0;
    FString AlignName;
    if (McpWidgetSlotAlignment::ResolveAlignmentValue(SlotSpec->TryGetField(TEXT("hAlign")), Align, AlignName, true))
    {
        McpWidgetSlotAlignment::WriteAlignmentProperty(Slot, TEXT("HorizontalAlignment"), Align);
    }
    if (McpWidgetSlotAlignment::ResolveAlignmentValue(SlotSpec->TryGetField(TEXT("vAlign")), Align, AlignName, false))
    {
        McpWidgetSlotAlignment::WriteAlignmentProperty(Slot, TEXT("VerticalAlignment"), Align);
    }
    double Fill = 0.0;
    if (SlotSpec->TryGetNumberField(TEXT("fill"), Fill))
    {
        FSlateChildSize Size(ESlateSizeRule::Fill);
        Size.Value = static_cast<float>(Fill);
        if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Slot)) { HSlot->SetSize(Size); }
        if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Slot)) { VSlot->SetSize(Size); }
    }
    if (UUniformGridSlot* Grid = Cast<UUniformGridSlot>(Slot))
    {
        Grid->SetRow(static_cast<int32>(GetJsonNumberField(SlotSpec, TEXT("row"), 0.0)));
        Grid->SetColumn(static_cast<int32>(GetJsonNumberField(SlotSpec, TEXT("column"), 0.0)));
    }
    Slot->SynchronizeProperties();
}

bool McpAddSpecToWidget(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                        TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
                        UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Spec,
                        const FString& SlotName, TArray<UWidget*>& OutCreated)
{
    UWidgetTree* Tree = WidgetBP->WidgetTree;
    TArray<FString> Names;
    McpCollectSpecNames(Spec, SlotName, Names);
    for (const FString& Name : Names)
    {
        if (Tree->FindWidget(FName(*Name)))
        {
            Subsystem.SendAutomationError(Socket, RequestId, FString::Printf(
                TEXT("'%s' already holds a widget named '%s'. Pick another slotName, or remove_widget the old one first."),
                *WidgetBP->GetName(), *Name), TEXT("SLOT_EXISTS"));
            return false;
        }
    }
    const FString ParentSlot = ResolveParentSlotName(Payload);
    if (!ParentSlot.IsEmpty() && !Cast<UPanelWidget>(Tree->FindWidget(FName(*ParentSlot))))
    {
        Subsystem.SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("'%s' is not a panel in '%s'; parentSlot must name a panel widget (get_widget_info lists them)."),
            *ParentSlot, *WidgetBP->GetName()), TEXT("PARENT_NOT_FOUND"));
        return false;
    }
    UWidget* CreatedRoot = nullptr;
    if (ParentSlot.IsEmpty() && !Tree->RootWidget)
    {
        // A composite seated as the root would stretch across the whole screen.
        Tree->RootWidget = CreatedRoot = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, Tree, TEXT("RootCanvas"));
    }
    FString Error;
    UWidget* Root = McpBuildWidgetSpec(WidgetBP, Spec, SlotName, OutCreated, Error);
    if (!Root || !SafeAddWidgetToTree(WidgetBP, Root, ParentSlot))
    {
        // The canvas made for this call goes too: left behind, the next save wrote a stray root.
        // Rollback runs last-to-first, so the root canvas, first in the list, goes after its children.
        TArray<UWidget*> Undo{CreatedRoot};
        Undo.Append(OutCreated);
        McpRollbackWidgetSpec(WidgetBP, Undo);
        OutCreated.Reset();
        Subsystem.SendAutomationError(Socket, RequestId, Error.IsEmpty()
            ? FString::Printf(TEXT("could not seat '%s' in '%s'"), *SlotName, *WidgetBP->GetName()) : Error,
            TEXT("TREE_ERROR"));
        return false;
    }
    const TSharedPtr<FJsonObject>* SlotSpec = nullptr;
    if (Spec->TryGetObjectField(TEXT("slot"), SlotSpec))
    {
        McpApplySpecSlot(Root, *SlotSpec);
    }
    ApplyCanvasSlotGeometry(Payload, Root);
    return true;
}

UWidgetBlueprint* McpCreateTemplateWidgetBlueprint(UMcpAutomationBridgeSubsystem& Subsystem,
                                                   const FString& RequestId,
                                                   TSharedPtr<FMcpBridgeWebSocket> Socket,
                                                   const TSharedPtr<FJsonObject>& Payload,
                                                   const TCHAR* DefaultName, UClass* ParentClass)
{
    const FString Name = GetJsonStringField(Payload, TEXT("name"), DefaultName);
    FString RawFolder = GetJsonStringField(Payload, TEXT("path"));
    if (RawFolder.IsEmpty())
    {
        RawFolder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI"));
    }
    const FString Folder = SanitizeProjectRelativePath(RawFolder);
    FText NameProblem;
    if (Folder.IsEmpty() || Name.IsEmpty() || !FName(*Name).IsValidObjectName(NameProblem))
    {
        Subsystem.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("'%s' in '%s' is not a usable asset name and folder."), *Name, *RawFolder),
            TEXT("INVALID_PATH"));
        return nullptr;
    }
    const FString PackagePath = Folder / Name;
    if (FindObject<UWidgetBlueprint>(nullptr, *(PackagePath + TEXT(".") + Name)) ||
        FPackageName::DoesPackageExist(PackagePath))
    {
        Subsystem.SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("'%s' already exists. Templates build a new Widget Blueprint; pick another name, or delete the old asset first."),
            *PackagePath), TEXT("ALREADY_EXISTS"));
        return nullptr;
    }
    UPackage* Package = CreatePackage(*PackagePath);
    UWidgetBlueprint* WidgetBP = Package ? Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
        ParentClass ? ParentClass : UUserWidget::StaticClass(), Package, FName(*Name), BPTYPE_Normal,
        UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass())) : nullptr;
    if (!WidgetBP || !WidgetBP->WidgetTree)
    {
        Subsystem.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not create '%s'."), *PackagePath),
                                      TEXT("CREATION_ERROR"));
        return nullptr;
    }
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(WidgetBP);
    return WidgetBP;
}

UWidgetAnimation* McpAddOpacityAnimation(UWidgetBlueprint* WidgetBP, const FString& AnimationName, UWidget* Target,
                                         const TArray<FVector2D>& Keys)
{
    if (!WidgetBP || !Target || Keys.Num() == 0 || FindWidgetAnimation(WidgetBP, AnimationName) ||
        FindObject<UObject>(WidgetBP, *AnimationName))
    {
        return nullptr;
    }
    UWidgetAnimation* Animation = NewObject<UWidgetAnimation>(WidgetBP, FName(*AnimationName), RF_Transactional);
    Animation->MovieScene = NewObject<UMovieScene>(Animation, FName(*AnimationName), RF_Transactional);
    UMovieScene* Scene = Animation->GetMovieScene();
    Scene->SetDisplayRate(FFrameRate(20, 1));
    const FFrameTime End = Keys.Last().X * Scene->GetTickResolution();
    Scene->SetPlaybackRange(TRange<FFrameNumber>(FFrameNumber(0), End.FrameNumber + 1));
    RegisterAnimationGuid(WidgetBP, Animation);
    for (const FVector2D& Key : Keys)
    {
        TSharedPtr<FJsonObject> KeyPayload = MakeShared<FJsonObject>();
        KeyPayload->SetStringField(TEXT("trackType"), TEXT("opacity"));
        KeyPayload->SetNumberField(TEXT("time"), Key.X);
        KeyPayload->SetNumberField(TEXT("propertyValue"), Key.Y);
        FMcpWidgetKeyResult KeyResult;
        FString KeyError;
        FString KeyErrorCode;
        // A key that failed left an animation with no keys that the reply still named.
        if (!McpAuthorWidgetAnimationKey(WidgetBP, Animation, Target, KeyPayload, KeyResult, KeyError, KeyErrorCode))
        {
            McpRemoveWidgetAnimation(WidgetBP, AnimationName);
            return nullptr;
        }
    }
    return Animation;
}

void McpRemoveWidgetAnimation(UWidgetBlueprint* WidgetBP, const FString& AnimationName)
{
    if (UWidgetAnimation* Animation = WidgetBP ? FindWidgetAnimation(WidgetBP, AnimationName) : nullptr)
    {
        WidgetBP->Animations.Remove(Animation);
        Animation->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
    }
}

void McpBindSpecSlot(const TSharedPtr<FJsonObject>& Spec, const FString& SlotName)
{
    if (!Spec.IsValid())
    {
        return;
    }
    Spec->SetStringField(TEXT("name"), GetJsonStringField(Spec, TEXT("name")).Replace(TEXT("{slot}"), *SlotName));
    const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
    if (Spec->TryGetArrayField(TEXT("children"), Children))
    {
        for (const TSharedPtr<FJsonValue>& Child : *Children)
        {
            McpBindSpecSlot(Child->AsObject(), SlotName);
        }
    }
}

void McpCopyPayloadColor(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field,
                         const TSharedPtr<FJsonObject>& Node, const TCHAR* SpecField)
{
    if (!Node.IsValid() || !Payload.IsValid() || !Payload->HasField(Field))
    {
        return;
    }
    const FLinearColor Color = ExtractLinearColorField(Payload, Field, FLinearColor::White);
    TArray<TSharedPtr<FJsonValue>> Channels;
    for (const float Channel : { Color.R, Color.G, Color.B, Color.A })
    {
        Channels.Add(MakeShared<FJsonValueNumber>(Channel));
    }
    Node->SetArrayField(SpecField, Channels);
}
}
