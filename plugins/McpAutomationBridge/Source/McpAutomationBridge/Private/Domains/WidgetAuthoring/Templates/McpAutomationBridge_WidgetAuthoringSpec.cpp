#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringSpec.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringValidation.h"
#include "Engine/Texture2D.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
namespace
{
FString SpecName(const TSharedPtr<FJsonObject>& Node, const FString& SlotName)
{
    return GetJsonStringField(Node, TEXT("name")).Replace(TEXT("{slot}"), *SlotName);
}

const TArray<TSharedPtr<FJsonValue>>* SpecChildren(const TSharedPtr<FJsonObject>& Node)
{
    const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
    return Node.IsValid() && Node->TryGetArrayField(TEXT("children"), Children) ? Children : nullptr;
}

UWidget* BuildNode(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Node, const FString& SlotName,
                   TArray<UWidget*>& OutCreated, FString& OutError)
{
    const FString Type = GetJsonStringField(Node, TEXT("type"));
    UClass* Class = FindObject<UClass>(nullptr, *(TEXT("/Script/UMG.") + Type));
    if (!Class || !Class->IsChildOf(UWidget::StaticClass()))
    {
        OutError = FString::Printf(TEXT("widget spec names an unknown UMG type '%s'"), *Type);
        return nullptr;
    }
    const FString Name = SpecName(Node, SlotName);
    // The subtree is seated only after it is built, so FindWidget cannot see its own nodes: two
    // spec nodes folding to one name ("New Game", "NewGame") made ConstructWidget replace the
    // first in place and the panel list one widget twice.
    if (OutCreated.ContainsByPredicate([&Name](const UWidget* Made) { return Made && Made->GetFName() == FName(*Name); }))
    {
        OutError = FString::Printf(TEXT("two widgets in this build would both be named '%s'; give them distinct names"), *Name);
        return nullptr;
    }
    const FString NameConflict = McpWidgetNameConflict(WidgetBP, FName(*Name), Class);
    if (!NameConflict.IsEmpty())
    {
        OutError = NameConflict;
        return nullptr;
    }
    UWidget* Widget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(Class, FName(*Name));
    if (!Widget)
    {
        OutError = FString::Printf(TEXT("could not construct a %s"), *Type);
        return nullptr;
    }
    Widget->bIsVariable = true;
    RegisterWidgetGuid(WidgetBP, Widget);
    OutCreated.Add(Widget);
    const FString PropError = McpApplySpecWidgetProps(Widget, Node);
    if (!PropError.IsEmpty())
    {
        OutError = FString::Printf(TEXT("'%s': %s"), *Name, *PropError);
        return nullptr;
    }
    const TArray<TSharedPtr<FJsonValue>>* Children = SpecChildren(Node);
    if (!Children)
    {
        return Widget;
    }
    UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
    for (const TSharedPtr<FJsonValue>& ChildValue : *Children)
    {
        const TSharedPtr<FJsonObject> ChildNode = ChildValue->AsObject();
        UWidget* Child = BuildNode(WidgetBP, ChildNode, SlotName, OutCreated, OutError);
        if (!Child)
        {
            return nullptr;
        }
        if (!Panel || !Panel->AddChild(Child))
        {
            OutError = FString::Printf(TEXT("'%s' cannot hold child '%s'"), *Widget->GetName(), *Child->GetName());
            return nullptr;
        }
        const TSharedPtr<FJsonObject>* SlotSpec = nullptr;
        if (ChildNode->TryGetObjectField(TEXT("slot"), SlotSpec))
        {
            McpApplySpecSlot(Child, *SlotSpec);
        }
    }
    return Widget;
}
}

UTexture2D* McpLoadSpecTexture(const FString& TexturePath)
{
    return TexturePath.IsEmpty() ? nullptr
        : Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *TexturePath));
}

TSharedPtr<FJsonObject> McpParseWidgetSpec(const TCHAR* Json)
{
    TSharedPtr<FJsonObject> Spec;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Spec);
    return Spec;
}

TSharedPtr<FJsonObject> McpFindSpecNode(const TSharedPtr<FJsonObject>& Spec, const FString& Name)
{
    if (!Spec.IsValid())
    {
        return nullptr;
    }
    if (GetJsonStringField(Spec, TEXT("name")) == Name)
    {
        return Spec;
    }
    if (const TArray<TSharedPtr<FJsonValue>>* Children = SpecChildren(Spec))
    {
        for (const TSharedPtr<FJsonValue>& Child : *Children)
        {
            if (TSharedPtr<FJsonObject> Found = McpFindSpecNode(Child->AsObject(), Name))
            {
                return Found;
            }
        }
    }
    return nullptr;
}

void McpCollectSpecNames(const TSharedPtr<FJsonObject>& Spec, const FString& SlotName, TArray<FString>& OutNames)
{
    if (!Spec.IsValid())
    {
        return;
    }
    OutNames.Add(SpecName(Spec, SlotName));
    if (const TArray<TSharedPtr<FJsonValue>>* Children = SpecChildren(Spec))
    {
        for (const TSharedPtr<FJsonValue>& Child : *Children)
        {
            McpCollectSpecNames(Child->AsObject(), SlotName, OutNames);
        }
    }
}

UWidget* McpBuildWidgetSpec(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Spec,
                            const FString& SlotName, TArray<UWidget*>& OutCreated, FString& OutError)
{
    if (!WidgetBP || !WidgetBP->WidgetTree || !Spec.IsValid())
    {
        OutError = TEXT("no widget tree to build into");
        return nullptr;
    }
    return BuildNode(WidgetBP, Spec, SlotName, OutCreated, OutError);
}

void McpRollbackWidgetSpec(UWidgetBlueprint* WidgetBP, const TArray<UWidget*>& Created)
{
    for (int32 Index = Created.Num() - 1; Index >= 0; --Index)
    {
        UWidget* Widget = Created[Index];
        if (!Widget)
        {
            continue;
        }
        UnregisterWidgetGuid(WidgetBP, Widget);
        WidgetBP->WidgetTree->RemoveWidget(Widget);
        // Freeing the name keeps a retry from re-initialising an orphan the compiler would trip on.
        Widget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
    }
}
}
