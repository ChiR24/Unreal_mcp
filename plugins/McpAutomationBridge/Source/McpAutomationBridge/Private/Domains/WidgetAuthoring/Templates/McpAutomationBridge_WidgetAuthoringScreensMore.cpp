#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringScreens.h"

#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringSpec.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"

// The panel screens of create_widget_template: dialog, inventory, radial menu, credits, shop.
namespace WidgetAuthoringScreens
{
using namespace WidgetAuthoringHelpers;

namespace
{
bool ReadCount(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, int32 Default, int32 Min, int32 Max,
               int32& Out, FString& OutError)
{
    Out = static_cast<int32>(GetJsonNumberField(Payload, Field, Default));
    if (Out < Min || Out > Max)
    {
        OutError = FString::Printf(TEXT("%s must be %d to %d"), Field, Min, Max);
        return false;
    }
    return true;
}

TSharedPtr<FJsonObject> AddText(const TSharedPtr<FJsonObject>& Parent, const FString& Name, const FString& Value, double Size)
{
    TSharedPtr<FJsonObject> Result = Append(Parent, Node(TEXT("TextBlock"), Name, Value));
    Result->SetNumberField(TEXT("fontSize"), Size);
    return Result;
}

TSharedPtr<FJsonObject> Gold(const TSharedPtr<FJsonObject>& Target)
{
    return Numbers(Target, TEXT("color"), { 1.0, 0.8, 0.2, 1.0 });
}

// A dark rounded panel centred on the screen.
TSharedPtr<FJsonObject> CenterPanel(const TSharedPtr<FJsonObject>& Root, const FString& Name)
{
    TSharedPtr<FJsonObject> Panel = SetSlot(Append(Root, Numbers(Node(TEXT("Border"), Name), TEXT("color"), { 0.06, 0.06, 0.08, 0.95 })),
        TEXT(R"({"anchors":[0.5,0.5,0.5,0.5],"alignment":[0.5,0.5],"autoSize":true})"));
    Panel->SetNumberField(TEXT("radius"), 12);
    Panel->SetNumberField(TEXT("padding"), 24);
    return Panel;
}

TSharedPtr<FJsonObject> BuildDialog(const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
    int32 Responses = 0;
    if (!ReadCount(Payload, TEXT("responseCount"), 3, 0, 6, Responses, OutError))
    {
        return nullptr;
    }
    TSharedPtr<FJsonObject> Root = Node(TEXT("CanvasPanel"), TEXT("RootCanvas"));
    TSharedPtr<FJsonObject> Panel = SetSlot(Append(Root, Numbers(Node(TEXT("Border"), TEXT("DialogPanel")), TEXT("color"), { 0.04, 0.04, 0.06, 0.92 })),
        TEXT(R"({"anchors":[0.5,1,0.5,1],"alignment":[0.5,1],"position":[0,-40],"size":[960,300]})"));
    Panel->SetNumberField(TEXT("radius"), 12);
    Numbers(Panel, TEXT("padding"), { 28, 20, 28, 20 });
    TSharedPtr<FJsonObject> Box = Append(Panel, Node(TEXT("VerticalBox"), TEXT("DialogBox")));
    TSharedPtr<FJsonObject> Speaker = SetSlot(Gold(AddText(Box, TEXT("SpeakerName"), TEXT("Speaker"), 22)), TEXT(R"({"padding":[0,0,0,8]})"));
    if (!GetJsonBoolField(Payload, TEXT("showSpeakerName"), true))
    {
        Speaker->SetStringField(TEXT("visibility"), TEXT("Collapsed"));
    }
    TSharedPtr<FJsonObject> Line = SetSlot(AddText(Box, TEXT("DialogText"), TEXT("Dialog line goes here."), 20), TEXT(R"({"fill":1})"));
    Line->SetBoolField(TEXT("autoWrap"), true);
    TSharedPtr<FJsonObject> List = SetSlot(Append(Box, Node(TEXT("VerticalBox"), TEXT("ResponseList"))), TEXT(R"({"padding":[0,8,0,0]})"));
    for (int32 Index = 1; Index <= Responses; ++Index)
    {
        Append(List, Button(FString::Printf(TEXT("Response%d"), Index), FString::Printf(TEXT("Response option %d"), Index), 18));
    }
    TSharedPtr<FJsonObject> Hint = SetSlot(AddText(Box, TEXT("ContinueHint"), TEXT("Press Space to continue"), 14), TEXT(R"({"hAlign":"right"})"));
    Numbers(Hint, TEXT("color"), { 0.7, 0.7, 0.7, 1.0 });
    return Root;
}

TSharedPtr<FJsonObject> GridCell(const TSharedPtr<FJsonObject>& Grid, const FString& Name, int32 Row, int32 Column)
{
    TSharedPtr<FJsonObject> Cell = Append(Grid, Numbers(Node(TEXT("Border"), Name), TEXT("color"), { 1.0, 1.0, 1.0, 0.07 }));
    Cell->SetNumberField(TEXT("radius"), 6);
    Cell->SetNumberField(TEXT("padding"), 4);
    TSharedPtr<FJsonObject> Slot = MakeShared<FJsonObject>();
    Slot->SetNumberField(TEXT("row"), Row);
    Slot->SetNumberField(TEXT("column"), Column);
    Cell->SetObjectField(TEXT("slot"), Slot);
    return Cell;
}

TSharedPtr<FJsonObject> BuildInventory(const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
    int32 Columns = 0;
    int32 Rows = 0;
    if (!ReadCount(Payload, TEXT("columns"), 6, 1, 12, Columns, OutError) || !ReadCount(Payload, TEXT("rows"), 4, 1, 12, Rows, OutError))
    {
        return nullptr;
    }
    TSharedPtr<FJsonObject> Root = RootWithBackdrop(0.7);
    TSharedPtr<FJsonObject> Box = Append(CenterPanel(Root, TEXT("InventoryPanel")), Node(TEXT("VerticalBox"), TEXT("InventoryBox")));
    SetSlot(AddText(Box, TEXT("InventoryTitle"), TEXT("Inventory"), 32), TEXT(R"({"padding":[0,0,0,12]})"));
    TSharedPtr<FJsonObject> Grid = Numbers(Append(Box, Node(TEXT("UniformGridPanel"), TEXT("InventoryGrid"))), TEXT("slotPadding"), { 4, 4, 4, 4 });
    for (int32 Row = 0; Row < Rows; ++Row)
    {
        for (int32 Column = 0; Column < Columns; ++Column)
        {
            const FString Name = FString::Printf(TEXT("InvSlot_%d_%d"), Row, Column);
            TSharedPtr<FJsonObject> Icon = Append(GridCell(Grid, Name, Row, Column), Node(TEXT("Image"), Name + TEXT("_Icon")));
            Numbers(Numbers(Icon, TEXT("color"), { 1.0, 1.0, 1.0, 0.0 }), TEXT("imageSize"), { 64, 64 });
        }
    }
    return Root;
}

TSharedPtr<FJsonObject> BuildRadial(const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
    int32 Segments = 0;
    if (!ReadCount(Payload, TEXT("segmentCount"), 8, 2, 12, Segments, OutError))
    {
        return nullptr;
    }
    TSharedPtr<FJsonObject> Root = RootWithBackdrop(0.35);
    TSharedPtr<FJsonObject> Ring = SetSlot(Append(Root, Node(TEXT("Overlay"), TEXT("RadialMenu"))),
        TEXT(R"({"anchors":[0.5,0.5,0.5,0.5],"alignment":[0.5,0.5],"size":[440,440]})"));
    TSharedPtr<FJsonObject> Back = SetSlot(Append(Ring, Numbers(Node(TEXT("Border"), TEXT("RadialBackground")), TEXT("color"), { 0.0, 0.0, 0.0, 0.55 })),
        TEXT(R"({"hAlign":"fill","vAlign":"fill"})"));
    Back->SetNumberField(TEXT("radius"), 220);
    TSharedPtr<FJsonObject> Canvas = SetSlot(Append(Ring, Node(TEXT("CanvasPanel"), TEXT("SegmentCanvas"))), TEXT(R"({"hAlign":"fill","vAlign":"fill"})"));
    for (int32 Index = 0; Index < Segments; ++Index)
    {
        const double Angle = 2.0 * PI * Index / Segments;
        TSharedPtr<FJsonObject> Segment = Append(Canvas, Button(FString::Printf(TEXT("Segment_%d"), Index), FString::FromInt(Index + 1), 20));
        Segment->SetNumberField(TEXT("radius"), 38);
        TSharedPtr<FJsonObject> Slot = McpParseWidgetSpec(TEXT(R"({"anchors":[0.5,0.5,0.5,0.5],"alignment":[0.5,0.5],"size":[76,76]})"));
        Segment->SetObjectField(TEXT("slot"), Numbers(Slot, TEXT("position"), { 165.0 * FMath::Sin(Angle), -165.0 * FMath::Cos(Angle) }));
    }
    SetSlot(AddText(Ring, TEXT("SelectionLabel"), TEXT("Select"), 22), TEXT(R"({"hAlign":"center","vAlign":"center"})"));
    return Root;
}

TSharedPtr<FJsonObject> BuildCredits(const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
    TArray<TPair<FString, FString>> Entries;
    const TArray<TSharedPtr<FJsonValue>>* Listed = nullptr;
    if (Payload->TryGetArrayField(TEXT("entries"), Listed))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Listed)
        {
            const TSharedPtr<FJsonObject>* Entry = nullptr;
            FString Title, Name;
            if (!Value->TryGetObject(Entry) || !(*Entry)->TryGetStringField(TEXT("title"), Title) || !(*Entry)->TryGetStringField(TEXT("name"), Name))
            {
                OutError = TEXT("every entries item needs a title and a name string, e.g. {\"title\":\"Music\",\"name\":\"Ada\"}");
                return nullptr;
            }
            Entries.Emplace(Title, Name);
        }
    }
    if (Entries.Num() == 0)
    {
        for (const TCHAR* Title : { TEXT("Lead Developer"), TEXT("Art Director"), TEXT("Sound Design"), TEXT("Special Thanks") })
        {
            Entries.Emplace(Title, TEXT("Your Name Here"));
        }
    }
    TSharedPtr<FJsonObject> Root = RootWithBackdrop(1.0);
    // On a point-x anchor the Right offset IS the width (a SetSize then zero offsets drew nothing).
    TSharedPtr<FJsonObject> Scroll = SetSlot(Append(Root, Node(TEXT("ScrollBox"), TEXT("CreditsScroll"))),
        TEXT(R"({"anchors":[0.5,0,0.5,1],"alignment":[0.5,0],"offsets":[0,60,640,140]})"));
    TSharedPtr<FJsonObject> Content = Append(Scroll, Node(TEXT("VerticalBox"), TEXT("CreditsContent")));
    TSharedPtr<FJsonObject> Title = SetSlot(AddText(Content, TEXT("CreditsTitle"), GetJsonStringField(Payload, TEXT("title"), TEXT("Credits")), 48),
                                            TEXT(R"({"padding":[0,0,0,16]})"));
    Title->SetStringField(TEXT("justify"), TEXT("center"));
    TSet<FString> Used;
    for (int32 Index = 0; Index < Entries.Num(); ++Index)
    {
        FString Stem = NameStem(Entries[Index].Key);
        Stem = Used.Contains(Stem) ? FString::Printf(TEXT("%s%d"), *Stem, Index + 1) : Stem;
        Used.Add(Stem);
        TSharedPtr<FJsonObject> Role = SetSlot(Gold(AddText(Content, Stem + TEXT("_Title"), Entries[Index].Key, 20)), TEXT(R"({"padding":[0,24,0,4]})"));
        Role->SetStringField(TEXT("justify"), TEXT("center"));
        AddText(Content, Stem + TEXT("_Name"), Entries[Index].Value, 26)->SetStringField(TEXT("justify"), TEXT("center"));
    }
    SetSlot(Append(Root, Button(TEXT("BackButton"), TEXT("Back"))), TEXT(R"({"anchors":[0.5,1,0.5,1],"alignment":[0.5,1],"position":[0,-48],"size":[240,56]})"));
    return Root;
}

TSharedPtr<FJsonObject> BuildShop(const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
    int32 Columns = 0;
    int32 Items = 0;
    if (!ReadCount(Payload, TEXT("columns"), 4, 1, 8, Columns, OutError) || !ReadCount(Payload, TEXT("itemCount"), 8, 1, 48, Items, OutError))
    {
        return nullptr;
    }
    TSharedPtr<FJsonObject> Root = RootWithBackdrop(0.7);
    TSharedPtr<FJsonObject> Box = Append(CenterPanel(Root, TEXT("ShopPanel")), Node(TEXT("VerticalBox"), TEXT("ShopBox")));
    TSharedPtr<FJsonObject> Header = Append(Box, Node(TEXT("HorizontalBox"), TEXT("ShopHeader")));
    SetSlot(AddText(Header, TEXT("ShopTitle"), TEXT("Shop"), 32), TEXT(R"({"fill":1})"));
    SetSlot(Gold(AddText(Header, TEXT("CurrencyDisplay"), TEXT("Gold: 0"), 22)), TEXT(R"({"vAlign":"center"})"));
    const int32 Rows = FMath::DivideAndRoundUp(Items, Columns);
    TSharedPtr<FJsonObject> Area = SetSlot(Append(Box, Node(TEXT("SizeBox"), TEXT("ItemsArea"))), TEXT(R"({"padding":[0,12,0,12]})"));
    Area->SetNumberField(TEXT("height"), FMath::Min(Rows * 200, 480));
    TSharedPtr<FJsonObject> Grid = Append(Append(Area, Node(TEXT("ScrollBox"), TEXT("ItemsScroll"))), Node(TEXT("UniformGridPanel"), TEXT("ItemsGrid")));
    Numbers(Grid, TEXT("slotPadding"), { 6, 6, 6, 6 });
    for (int32 Index = 0; Index < Items; ++Index)
    {
        const FString Name = FString::Printf(TEXT("Item_%d"), Index + 1);
        TSharedPtr<FJsonObject> Card = Append(GridCell(Grid, Name, Index / Columns, Index % Columns), Node(TEXT("VerticalBox"), Name + TEXT("_Box")));
        TSharedPtr<FJsonObject> Icon = SetSlot(Append(Card, Node(TEXT("Image"), Name + TEXT("_Icon"))), TEXT(R"({"hAlign":"center"})"));
        Numbers(Numbers(Icon, TEXT("color"), { 0.3, 0.3, 0.35, 1.0 }), TEXT("imageSize"), { 72, 72 });
        AddText(Card, Name + TEXT("_Name"), FString::Printf(TEXT("Item %d"), Index + 1), 18)->SetStringField(TEXT("justify"), TEXT("center"));
        Gold(AddText(Card, Name + TEXT("_Price"), TEXT("100"), 16))->SetStringField(TEXT("justify"), TEXT("center"));
        Append(Card, Button(Name + TEXT("_Buy"), TEXT("Buy"), 16));
    }
    Append(Box, Button(TEXT("CloseButton"), TEXT("Close")));
    return Root;
}
}

const TArray<FScreenTemplate>& PanelScreens()
{
    static const TArray<FScreenTemplate> Screens = {
        { TEXT("create_dialog_widget"), TEXT("WBP_Dialog"), TEXT("dialog"), &BuildDialog, nullptr },
        { TEXT("create_inventory_ui"), TEXT("WBP_Inventory"), TEXT("inventory"), &BuildInventory, nullptr },
        { TEXT("create_radial_menu"), TEXT("WBP_RadialMenu"), TEXT("radial menu"), &BuildRadial, nullptr },
        { TEXT("create_credits_screen"), TEXT("WBP_Credits"), TEXT("credits screen"), &BuildCredits, nullptr },
        { TEXT("create_shop_ui"), TEXT("WBP_Shop"), TEXT("shop"), &BuildShop, nullptr },
    };
    return Screens;
}
}
