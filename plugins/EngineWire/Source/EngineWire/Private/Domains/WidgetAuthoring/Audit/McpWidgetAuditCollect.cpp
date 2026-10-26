// The visible widgets of a laid-out User Widget (audit_widget; see McpWidgetAudit.h).
#include "Domains/WidgetAuthoring/Audit/McpWidgetAudit.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/TextBlock.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"

namespace McpWidgetAudit
{
namespace
{
EKind AuditKind(const UWidget* Widget)
{
    // Before the panel test: a button is a content widget, which is a panel.
    if (Widget->IsA<UButton>() || Widget->IsA<UCheckBox>() || Widget->IsA<USlider>() || Widget->IsA<USpinBox>() ||
        Widget->IsA<UComboBoxString>() || Widget->IsA<UEditableTextBox>() || Widget->IsA<UEditableText>())
    {
        return EKind::Interactive;
    }
    if (Widget->IsA<UTextBlock>() || Widget->IsA<URichTextBlock>())
    {
        return EKind::Text;
    }
    if (Widget->IsA<UImage>() || Widget->IsA<UProgressBar>())
    {
        return EKind::Image;
    }
    return Widget->IsA<UPanelWidget>() ? EKind::Panel : EKind::Other;
}

// Font and colour through reflection: members in 5.0, behind getters from 5.1, the property the same in every 5.x.
template <typename T>
const T* AuditProperty(const UObject* Object, const TCHAR* Name)
{
    const FStructProperty* Property = CastField<FStructProperty>(Object->GetClass()->FindPropertyByName(Name));
    return Property && Property->Struct == T::StaticStruct() ? Property->ContainerPtrToValuePtr<T>(Object) : nullptr;
}

void AuditText(const UWidget* Widget, FItem& Item)
{
    if (const UTextLayoutWidget* Layout = Cast<UTextLayoutWidget>(Widget))
    {
        Item.bWraps = Layout->GetAutoWrapText();
    }
    if (const URichTextBlock* Rich = Cast<URichTextBlock>(Widget))
    {
        Item.Text = Rich->GetText().ToString(); // its look comes from a style table: judged from its pixels alone
        return;
    }
    const UTextBlock* Text = Cast<UTextBlock>(Widget);
    if (!Text)
    {
        return;
    }
    Item.Text = Text->GetText().ToString();
    if (const FSlateFontInfo* Font = AuditProperty<FSlateFontInfo>(Text, TEXT("Font")))
    {
        Item.FontSize = Font->Size;
        Item.bBold = Font->TypefaceFontName.ToString().Contains(TEXT("Bold"));
    }
    if (const FSlateColor* Color = AuditProperty<FSlateColor>(Text, TEXT("ColorAndOpacity")))
    {
        Item.bColorSet = Color->IsColorSpecified();
        Item.Color = Item.bColorSet ? Color->GetSpecifiedColor() : FLinearColor::White;
    }
}
} // namespace

TArray<FItem> Collect(UUserWidget& Widget)
{
    TArray<FItem> Items;
    TMap<const UWidget*, int32> Seen; // each widget's item, INDEX_NONE for one not shown
    if (!Widget.WidgetTree)
    {
        return Items;
    }
    // Parents come before their children (pre-order), so a child finds its parent's item.
    Widget.WidgetTree->ForEachWidget([&](UWidget* Child)
    {
        const int32* ParentItem = Child && Child->GetParent() ? Seen.Find(Child->GetParent()) : nullptr;
        const TSharedPtr<SWidget> Slate = Child ? Child->GetCachedWidget() : nullptr;
        if (!Child || !Slate.IsValid() || (ParentItem && *ParentItem == INDEX_NONE) || !Slate->GetVisibility().IsVisible() ||
            Child->GetRenderOpacity() <= 0.f)
        {
            Seen.Add(Child, INDEX_NONE);
            return;
        }
        FItem Item;
        Item.Name = Child->GetName();
        Item.Kind = AuditKind(Child);
        const FGeometry& Geometry = Child->GetPaintSpaceGeometry();
        const FSlateRect Box = Geometry.GetLayoutBoundingRect();
        Item.Rect = FBox2D(FVector2D(Box.Left, Box.Top), FVector2D(Box.Right, Box.Bottom));
        Item.Local = FVector2D(Geometry.GetLocalSize());
        Item.Desired = FVector2D(Slate->GetDesiredSize());
        if (ParentItem)
        {
            const FItem& Parent = Items[*ParentItem];
            Item.Ancestors.Add(*ParentItem);
            Item.Ancestors.Append(Parent.Ancestors);
            Item.bScrolled = Parent.bScrolled || Cast<UScrollBox>(Child->GetParent()) != nullptr;
        }
        AuditText(Child, Item);
        Seen.Add(Child, Items.Add(MoveTemp(Item)));
    });
    return Items;
}
} // namespace McpWidgetAudit
