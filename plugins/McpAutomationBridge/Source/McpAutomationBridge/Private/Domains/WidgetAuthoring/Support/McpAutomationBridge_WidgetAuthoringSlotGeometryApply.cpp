#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"

namespace WidgetAuthoringHelpers
{
FString ResolveParentSlotName(const TSharedPtr<FJsonObject>& Payload)
{
    if (!Payload.IsValid())
    {
        return FString();
    }
    // The contract advertises both spellings; a caller that sent only parentName
    // used to be silently reparented to the root, which is why explicitly placed
    // widgets landed in the wrong panel.
    FString Parent = GetJsonStringField(Payload, TEXT("parentSlot"));
    if (Parent.IsEmpty())
    {
        Parent = GetJsonStringField(Payload, TEXT("parentName"));
    }
    return Parent;
}

void ApplyCanvasSlotGeometry(const TSharedPtr<FJsonObject>& Payload, UWidget* Widget)
{
    if (!Payload.IsValid() || !Widget)
    {
        return;
    }
    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
    if (!CanvasSlot)
    {
        // Non-canvas parents (boxes, grids) carry no pixel geometry of their own.
        return;
    }

    const bool bHasPosition =
        Payload->HasField(TEXT("positionX")) || Payload->HasField(TEXT("positionY"));
    const bool bHasSize = Payload->HasField(TEXT("sizeX")) || Payload->HasField(TEXT("sizeY"));
    if (!bHasPosition && !bHasSize)
    {
        return;
    }

    // Explicit pixel geometry only means what the caller wrote when the slot is
    // pinned to the top-left corner and aligned to its own origin. Without this
    // the default centre anchor reinterprets every coordinate as a fraction.
    CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
    CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));

    if (bHasPosition)
    {
        FVector2D Position = CanvasSlot->GetPosition();
        Position.X = static_cast<float>(GetJsonNumberField(Payload, TEXT("positionX"), Position.X));
        Position.Y = static_cast<float>(GetJsonNumberField(Payload, TEXT("positionY"), Position.Y));
        CanvasSlot->SetPosition(Position);
    }

    if (bHasSize)
    {
        FVector2D Size = CanvasSlot->GetSize();
        Size.X = static_cast<float>(GetJsonNumberField(Payload, TEXT("sizeX"), Size.X));
        Size.Y = static_cast<float>(GetJsonNumberField(Payload, TEXT("sizeY"), Size.Y));
        CanvasSlot->SetSize(Size);
        CanvasSlot->SetAutoSize(false);
    }
}
}
