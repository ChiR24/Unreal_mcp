// Anchors of arrange_widgets (see McpWidgetArrange.h): which edge or centre of the panel each widget holds on to, and
// its Canvas Panel slot written from its box with them.
#include "Domains/WidgetAuthoring/Arrange/McpWidgetArrange.h"

#include "Components/CanvasPanelSlot.h"

namespace McpWidgetArrange
{
namespace
{
constexpr uint8 ArrangeStartPin = 1;
constexpr uint8 ArrangeEndPin = 2;
constexpr uint8 ArrangeMiddlePin = 4;

// Relations that tie two widgets together along an axis: what holds one on to the panel holds the other.
bool ArrangeTies(const FString& Type, int32 Axis)
{
    static const TArray<FString> X = {TEXT("leftOf"), TEXT("rightOf"), TEXT("alignLeft"), TEXT("alignRight"), TEXT("alignCenterX")};
    static const TArray<FString> Y = {TEXT("above"), TEXT("below"), TEXT("alignTop"), TEXT("alignBottom"), TEXT("alignCenterY")};
    return (Axis == 0 ? X : Y).Contains(Type);
}

uint8 ArrangePin(const FRelation& R, int32 Axis)
{
    if (R.Type == TEXT("inset"))
    {
        const bool bX = R.Edge == TEXT("left") || R.Edge == TEXT("right");
        return bX != (Axis == 0) ? 0 : (R.Edge == TEXT("left") || R.Edge == TEXT("top") ? ArrangeStartPin : ArrangeEndPin);
    }
    return R.Type == TEXT("center") && R.Edge != (Axis == 0 ? TEXT("y") : TEXT("x")) ? ArrangeMiddlePin : 0;
}

EAnchor ArrangeFromPins(uint8 Pins, bool bOwn)
{
    if (Pins & ArrangeMiddlePin) return EAnchor::Middle;
    if ((Pins & ArrangeStartPin) && (Pins & ArrangeEndPin)) return bOwn ? EAnchor::Stretch : EAnchor::Middle;
    return Pins & ArrangeEndPin ? EAnchor::End : EAnchor::Start;
}
} // namespace

TArray<TPair<EAnchor, EAnchor>> InferAnchors(const TArray<FRelation>& Relations, const TArray<FArrangeBox>& Boxes, const FVector2D& Panel)
{
    TArray<TPair<EAnchor, EAnchor>> Out;
    Out.SetNum(Boxes.Num());
    for (int32 Axis = 0; Axis < 2; ++Axis)
    {
        TArray<uint8> Pins;
        Pins.Init(0, Boxes.Num());
        TArray<int32> Group;
        for (int32 Index = 0; Index < Boxes.Num(); ++Index)
        {
            Group.Add(Index);
        }
        const auto Root = [&Group](int32 Index)
        {
            while (Group[Index] != Index)
            {
                Index = Group[Index] = Group[Group[Index]];
            }
            return Index;
        };
        for (const FRelation& R : Relations)
        {
            Pins[R.Widgets[0]] |= ArrangePin(R, Axis);
            for (int32 Member = 1; ArrangeTies(R.Type, Axis) && Member < R.Widgets.Num(); ++Member)
            {
                Group[Root(R.Widgets[Member])] = Root(R.Widgets[0]);
            }
        }
        TMap<int32, uint8> GroupPins;
        for (int32 Index = 0; Index < Boxes.Num(); ++Index)
        {
            GroupPins.FindOrAdd(Root(Index)) |= Pins[Index];
        }
        for (int32 Index = 0; Index < Boxes.Num(); ++Index)
        {
            const uint8 Shared = GroupPins[Root(Index)];
            const bool bOneKind = Shared == ArrangeStartPin || Shared == ArrangeEndPin || Shared == ArrangeMiddlePin;
            // Held by nothing, or by a group pulled both ways: the third of the panel its centre sits in decides.
            const double Start = Axis == 0 ? Boxes[Index].Left : Boxes[Index].Top;
            const double Size = Axis == 0 ? Boxes[Index].Width : Boxes[Index].Height;
            const double Share = (Start + 0.5 * Size) / FMath::Max(1.0, Axis == 0 ? Panel.X : Panel.Y);
            const EAnchor ByPlace = Share < 1.0 / 3.0 ? EAnchor::Start : Share > 2.0 / 3.0 ? EAnchor::End : EAnchor::Middle;
            const EAnchor Anchor = Pins[Index] ? ArrangeFromPins(Pins[Index], true) : bOneKind ? ArrangeFromPins(Shared, false) : ByPlace;
            (Axis == 0 ? Out[Index].Key : Out[Index].Value) = Anchor;
        }
    }
    return Out;
}

void WriteSlot(UCanvasPanelSlot& Slot, const FArrangeBox& Box, const TPair<EAnchor, EAnchor>& Anchors, const FVector2D& Panel)
{
    FAnchorData Layout = Slot.GetLayout();
    const double Starts[2] = {Box.Left, Box.Top};
    const double Sizes[2] = {Box.Width, Box.Height};
    double Min[2];
    double Max[2];
    double Near[2];
    double Far[2];
    double Align[2];
    for (int32 Axis = 0; Axis < 2; ++Axis)
    {
        const EAnchor Anchor = Axis == 0 ? Anchors.Key : Anchors.Value;
        const double PanelSize = Axis == 0 ? Panel.X : Panel.Y;
        if (Anchor == EAnchor::Stretch)
        {
            // Both edges: the offsets are the distances in from each side.
            Min[Axis] = 0.0;
            Max[Axis] = 1.0;
            Align[Axis] = 0.0;
            Near[Axis] = Starts[Axis];
            Far[Axis] = PanelSize - Starts[Axis] - Sizes[Axis];
            continue;
        }
        // One point: the offset is where the widget's own matching point sits from it, then the size.
        const double Point = Anchor == EAnchor::Start ? 0.0 : Anchor == EAnchor::Middle ? 0.5 : 1.0;
        Min[Axis] = Point;
        Max[Axis] = Point;
        Align[Axis] = Point;
        Near[Axis] = Starts[Axis] + Point * Sizes[Axis] - Point * PanelSize;
        Far[Axis] = Sizes[Axis];
    }
    // Floats: FAnchors takes them in every 5.x, FMargin in 5.0 (doubles from 5.1).
    Layout.Anchors = FAnchors(static_cast<float>(Min[0]), static_cast<float>(Min[1]), static_cast<float>(Max[0]), static_cast<float>(Max[1]));
    Layout.Offsets = FMargin(static_cast<float>(Near[0]), static_cast<float>(Near[1]), static_cast<float>(Far[0]), static_cast<float>(Far[1]));
    Layout.Alignment = FVector2D(Align[0], Align[1]);
    Slot.SetLayout(Layout);
    Slot.SetAutoSize(false);
}
} // namespace McpWidgetArrange
