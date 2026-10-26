#pragma once

#include "CoreMinimal.h"

class FJsonValue;
class UUserWidget;

// manage_blueprint audit_widget: a Widget Blueprint laid out offscreen at several screen sizes (the preview's draw) and
// checked the way a UI reviewer would: what leaves the screen or the safe area, text cut off by its box, widgets on top
// of each other, buttons too small to touch, text that does not stand out from what is behind it, and edges that
// almost line up. Reads only. Collecting is McpWidgetAuditCollect.cpp, the checks McpWidgetAuditChecks.cpp and
// McpWidgetAuditContrast.cpp.
namespace McpWidgetAudit
{
enum class EKind : uint8
{
    Panel,
    Text,
    Image,
    Interactive,
    Other
};

/** One visible widget as laid out. Rect is in screen pixels; Local and Desired in Slate units (before DPI scaling). */
struct FItem
{
    FString Name;
    EKind Kind = EKind::Other;
    FBox2D Rect = FBox2D(ForceInit);
    FVector2D Local = FVector2D::ZeroVector;
    FVector2D Desired = FVector2D::ZeroVector;
    TArray<int32> Ancestors; // indices of the visible widgets it sits in, nearest first
    bool bScrolled = false;  // inside a scroll box: its content may run past the screen on purpose
    bool bWraps = false;     // a text that wraps to its box
    FString Text;
    float FontSize = 0.f;
    bool bBold = false;
    bool bColorSet = false; // Color is the text's own (not inherited from its parent)
    FLinearColor Color = FLinearColor::White;
};

struct FFinding
{
    FString Kind; // off_screen, clipped_text, overlap, unsafe, small_target, low_contrast, misaligned
    int32 Severity = 0;
    FString Widget;
    FString Other;
    FBox2D Rect = FBox2D(ForceInit);
    FString Detail;
};

struct FOptions
{
    double SafeZone = 0.9;      // the title-safe share of each side of the screen
    double MinTarget = 0.0;     // touch: the smallest side an interactive widget may have, in Slate units (0: not checked)
    double NearMissPx = 4.0;    // edges this close but not equal read as misaligned
};

/** The visible widgets of Widget as it was last drawn, in tree order (a parent before its children). */
TArray<FItem> Collect(UUserWidget& Widget);

/** Layout findings for Items on a screen of Size pixels. */
void CheckLayout(const TArray<FItem>& Items, FIntPoint Size, const FOptions& Options, TArray<FFinding>& Out);

/** low_contrast findings: each text against the pixels around it (Pixels raw, alpha 0 where nothing drew). */
void CheckContrast(const TArray<FItem>& Items, const TArray<FColor>& Pixels, FIntPoint Size, TArray<FFinding>& Out);

/** WCAG 2.x contrast ratio of two sRGB colours, 1 to 21. */
double ContrastRatio(const FColor& A, const FColor& B);

/** Screen sizes from "WxH" strings (64-3840 by 64-2160, at most 6); no Values gives 1920x1080, 1280x720 and 3440x1440.
 *  False with OutError naming Field when a size cannot be read. */
bool ParseSizes(const TArray<TSharedPtr<FJsonValue>>* Values, const TCHAR* Field, TArray<FIntPoint>& Out, FString& OutError);
} // namespace McpWidgetAudit
