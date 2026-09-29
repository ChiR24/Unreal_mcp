#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringSpec.h"

#include "Blueprint/WidgetTree.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "WidgetBlueprint.h"

// The add_game_widget HUD pieces as widget specs (see ...Spec.h). The caller's knobs are
// written into a spec before it is built and seated through the checked add path.
namespace WidgetAuthoringHelpers
{
namespace
{
struct FHudElement
{
    const TCHAR* Action;
    const TCHAR* DefaultSlot;
    const TCHAR* Label;
    const TCHAR* Spec;
};

const FHudElement GHudElements[] = {
    { TEXT("add_health_bar"), TEXT("HealthBar"), TEXT("health bar"), TEXT(R"({"type":"HorizontalBox","name":"{slot}","slot":{"anchors":[0,0,0,0],"alignment":[0,0],"position":[24,24],"size":[320,28]},"children":[
        {"type":"TextBlock","name":"{slot}_Label","text":"HP","fontSize":18,"slot":{"vAlign":"center","padding":[0,0,8,0]}},
        {"type":"ProgressBar","name":"{slot}_Bar","percent":1,"color":[0.85,0.1,0.1,1],"slot":{"fill":1,"vAlign":"fill"}}]})") },
    { TEXT("add_ammo_counter"), TEXT("AmmoCounter"), TEXT("ammo counter"), TEXT(R"({"type":"TextBlock","name":"{slot}","text":"30 / 90","fontSize":28,"justify":"right",
        "slot":{"anchors":[1,1,1,1],"alignment":[1,1],"position":[-32,-32],"autoSize":true}})") },
    { TEXT("add_crosshair"), TEXT("Crosshair"), TEXT("crosshair"), TEXT(R"({"type":"TextBlock","name":"{slot}","text":"+","fontSize":32,"justify":"center","color":[1,1,1,0.9],
        "visibility":"HitTestInvisible","slot":{"anchors":[0.5,0.5,0.5,0.5],"alignment":[0.5,0.5],"position":[0,0],"autoSize":true}})") },
    { TEXT("add_minimap"), TEXT("Minimap"), TEXT("minimap"), TEXT(R"({"type":"Overlay","name":"{slot}","slot":{"anchors":[1,0,1,0],"alignment":[1,0],"position":[-24,24],"size":[220,220]},"children":[
        {"type":"Border","name":"{slot}_Frame","color":[0,0,0,0.55],"radius":10,"padding":6,"slot":{"hAlign":"fill","vAlign":"fill"},"children":[
            {"type":"Image","name":"{slot}_Map","color":[0.18,0.22,0.2,1],"slot":{"hAlign":"fill","vAlign":"fill"}}]},
        {"type":"Image","name":"{slot}_Player","color":[1,0.85,0.1,1],"imageSize":[12,12],"slot":{"hAlign":"center","vAlign":"center"}}]})") },
    { TEXT("add_compass"), TEXT("Compass"), TEXT("compass"), TEXT(R"({"type":"Border","name":"{slot}","color":[0,0,0,0.5],"radius":6,"padding":[12,4,12,4],
        "slot":{"anchors":[0.5,0,0.5,0],"alignment":[0.5,0],"position":[0,20],"size":[400,40]},"children":[
        {"type":"Overlay","name":"{slot}_Body","slot":{"hAlign":"fill","vAlign":"fill"},"children":[
            {"type":"Image","name":"{slot}_Strip","color":[1,1,1,0.25],"imageSize":[32,2],"slot":{"hAlign":"fill","vAlign":"center"}},
            {"type":"TextBlock","name":"{slot}_Heading","text":"N","fontSize":20,"justify":"center","slot":{"hAlign":"center","vAlign":"center"}}]}]})") },
    { TEXT("add_damage_indicator"), TEXT("DamageIndicator"), TEXT("damage indicator"), TEXT(R"({"type":"Overlay","name":"{slot}","visibility":"HitTestInvisible",
        "slot":{"anchors":[0,0,1,1],"offsets":[0,0,0,0]},"children":[
        {"type":"Image","name":"{slot}_Vignette","color":[0.8,0,0,0.45],"opacity":0,"slot":{"hAlign":"fill","vAlign":"fill"}},
        {"type":"CanvasPanel","name":"{slot}_Directional","slot":{"hAlign":"fill","vAlign":"fill"},"children":[
            {"type":"Image","name":"{slot}_Top","visibility":"Hidden","color":[0.9,0.1,0.1,0.8],"imageSize":[160,12],"slot":{"anchors":[0.5,0,0.5,0],"alignment":[0.5,0],"position":[0,8],"autoSize":true}},
            {"type":"Image","name":"{slot}_Bottom","visibility":"Hidden","color":[0.9,0.1,0.1,0.8],"imageSize":[160,12],"slot":{"anchors":[0.5,1,0.5,1],"alignment":[0.5,1],"position":[0,-8],"autoSize":true}},
            {"type":"Image","name":"{slot}_Left","visibility":"Hidden","color":[0.9,0.1,0.1,0.8],"imageSize":[12,160],"slot":{"anchors":[0,0.5,0,0.5],"alignment":[0,0.5],"position":[8,0],"autoSize":true}},
            {"type":"Image","name":"{slot}_Right","visibility":"Hidden","color":[0.9,0.1,0.1,0.8],"imageSize":[12,160],"slot":{"anchors":[1,0.5,1,0.5],"alignment":[1,0.5],"position":[-8,0],"autoSize":true}}]}]})") },
    { TEXT("add_interaction_prompt"), TEXT("InteractionPrompt"), TEXT("interaction prompt"), TEXT(R"({"type":"Border","name":"{slot}","color":[0,0,0,0.6],"radius":8,"padding":[14,8,14,8],
        "slot":{"anchors":[0.5,0.72,0.5,0.72],"alignment":[0.5,0.5],"autoSize":true},"children":[
        {"type":"HorizontalBox","name":"{slot}_Row","children":[
            {"type":"Border","name":"{slot}_KeyBadge","color":[1,1,1,0.9],"radius":4,"padding":[8,2,8,2],"slot":{"vAlign":"center","padding":[0,0,10,0]},"children":[
                {"type":"TextBlock","name":"{slot}_Key","text":"E","fontSize":18,"color":[0.05,0.05,0.05,1]}]},
            {"type":"TextBlock","name":"{slot}_Text","text":"Interact","fontSize":18,"slot":{"vAlign":"center"}}]}]})") },
    { TEXT("add_objective_tracker"), TEXT("ObjectiveTracker"), TEXT("objective tracker"), TEXT(R"({"type":"Border","name":"{slot}","color":[0,0,0,0.45],"radius":8,"padding":[12,8,12,8],
        "slot":{"anchors":[1,0,1,0],"alignment":[1,0],"position":[-24,264],"autoSize":true},"children":[
        {"type":"VerticalBox","name":"{slot}_Box","children":[
            {"type":"TextBlock","name":"{slot}_Title","text":"OBJECTIVES","fontSize":14,"color":[1,0.8,0.2,1],"slot":{"padding":[0,0,0,4]}},
            {"type":"VerticalBox","name":"{slot}_List"}]}]})") },
    { TEXT("add_quest_tracker"), TEXT("QuestTracker"), TEXT("quest tracker"), TEXT(R"({"type":"Border","name":"{slot}","color":[0,0,0,0.45],"radius":8,"padding":[12,8,12,8],
        "slot":{"anchors":[0,0,0,0],"alignment":[0,0],"position":[24,72],"autoSize":true},"children":[
        {"type":"VerticalBox","name":"{slot}_Box","children":[
            {"type":"TextBlock","name":"{slot}_Header","text":"ACTIVE QUEST","fontSize":12,"color":[1,0.8,0.2,1]},
            {"type":"TextBlock","name":"{slot}_Title","text":"Quest Name","fontSize":20,"slot":{"padding":[0,2,0,6]}},
            {"type":"VerticalBox","name":"{slot}_Objectives"}]}]})") },
};

TSharedPtr<FJsonObject> HudNode(const TSharedPtr<FJsonObject>& Spec, const TCHAR* Suffix)
{
    return McpFindSpecNode(Spec, FString(TEXT("{slot}")) + Suffix);
}

void CopyString(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, const TSharedPtr<FJsonObject>& Target)
{
    FString Value;
    if (Target.IsValid() && Payload->TryGetStringField(Field, Value))
    {
        Target->SetStringField(TEXT("text"), Value);
    }
}

void CopyNumber(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, const TSharedPtr<FJsonObject>& Target,
                const TCHAR* SpecField)
{
    double Value = 0.0;
    if (Target.IsValid() && Payload->TryGetNumberField(Field, Value))
    {
        Target->SetNumberField(SpecField, Value);
    }
}

// Objective and quest rows: the caller's `items` (at most MaxRows), or numbered placeholders.
void AddRows(const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& List, const TCHAR* RowName,
             int32 MaxRows, int32 Placeholders)
{
    TArray<FString> Items = McpGetStringListField(Payload, TEXT("items"), TEXT("items"));
    const int32 Count = Items.Num() > 0 ? FMath::Min(Items.Num(), MaxRows) : Placeholders;
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("type"), TEXT("TextBlock"));
        Row->SetStringField(TEXT("name"), FString::Printf(TEXT("{slot}_%s%d"), RowName, Index + 1));
        Row->SetStringField(TEXT("text"), TEXT("- ") + (Items.IsValidIndex(Index) ? Items[Index] : FString::Printf(TEXT("Objective %d"), Index + 1)));
        Row->SetNumberField(TEXT("fontSize"), 16);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    List->SetArrayField(TEXT("children"), Rows);
}

// Writes the caller's knobs into the spec. Returns an error for an input that would
// otherwise be dropped (a texture that does not load).
FString ApplyHudParams(const FString& Action, const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& Spec)
{
    const FString Texture = GetJsonStringField(Payload, TEXT("texturePath"));
    if (!Texture.IsEmpty() && !McpLoadSpecTexture(Texture))
    {
        return FString::Printf(TEXT("texturePath '%s' is not a Texture2D that loads"), *Texture);
    }
    const TSharedPtr<FJsonObject> Root = HudNode(Spec, TEXT(""));
    if (Action == TEXT("add_health_bar"))
    {
        CopyNumber(Payload, TEXT("percent"), HudNode(Spec, TEXT("_Bar")), TEXT("percent"));
        McpCopyPayloadColor(Payload, TEXT("fillColorAndOpacity"), HudNode(Spec, TEXT("_Bar")));
        CopyString(Payload, TEXT("text"), HudNode(Spec, TEXT("_Label")));
    }
    else if (Action == TEXT("add_ammo_counter") || Action == TEXT("add_crosshair"))
    {
        CopyString(Payload, TEXT("text"), Root);
        CopyNumber(Payload, TEXT("fontSize"), Root, TEXT("fontSize"));
        McpCopyPayloadColor(Payload, TEXT("colorAndOpacity"), Root);
    }
    else if (Action == TEXT("add_minimap") || Action == TEXT("add_compass"))
    {
        const TSharedPtr<FJsonObject> Image = HudNode(Spec, Action == TEXT("add_minimap") ? TEXT("_Map") : TEXT("_Strip"));
        if (!Texture.IsEmpty() && Image.IsValid())
        {
            Image->SetStringField(TEXT("texture"), Texture);
            Image->SetArrayField(TEXT("color"), { MakeShared<FJsonValueNumber>(1), MakeShared<FJsonValueNumber>(1),
                                                  MakeShared<FJsonValueNumber>(1), MakeShared<FJsonValueNumber>(1) });
        }
        CopyString(Payload, TEXT("text"), HudNode(Spec, TEXT("_Heading")));
        double Size = 0.0;
        if (Action == TEXT("add_minimap") && Payload->TryGetNumberField(TEXT("mapSize"), Size))
        {
            Root->GetObjectField(TEXT("slot"))->SetArrayField(TEXT("size"),
                { MakeShared<FJsonValueNumber>(Size), MakeShared<FJsonValueNumber>(Size) });
        }
    }
    else if (Action == TEXT("add_damage_indicator"))
    {
        McpCopyPayloadColor(Payload, TEXT("colorAndOpacity"), HudNode(Spec, TEXT("_Vignette")));
    }
    else if (Action == TEXT("add_interaction_prompt"))
    {
        CopyString(Payload, TEXT("text"), HudNode(Spec, TEXT("_Text")));
        CopyString(Payload, TEXT("keyLabel"), HudNode(Spec, TEXT("_Key")));
    }
    else if (Action == TEXT("add_objective_tracker") || Action == TEXT("add_quest_tracker"))
    {
        CopyString(Payload, TEXT("title"), HudNode(Spec, TEXT("_Title")));
        if (Action == TEXT("add_quest_tracker"))
        {
            AddRows(Payload, HudNode(Spec, TEXT("_Objectives")), TEXT("Objective"), 12, 3);
        }
        else
        {
            const int32 MaxRows = FMath::Clamp(static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxVisibleObjectives"), 3.0)), 1, 12);
            AddRows(Payload, HudNode(Spec, TEXT("_List")), TEXT("Item"), MaxRows, MaxRows);
        }
    }
    return FString();
}
}

bool McpResolveHudElement(const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FJsonObject>& OutSpec,
                          FString& OutDefaultSlot, FString& OutLabel, FString& OutError)
{
    for (const FHudElement& Candidate : GHudElements)
    {
        if (Action.Equals(Candidate.Action, ESearchCase::IgnoreCase))
        {
            OutSpec = McpParseWidgetSpec(Candidate.Spec);
            OutDefaultSlot = Candidate.DefaultSlot;
            OutLabel = Candidate.Label;
            OutError = ApplyHudParams(Candidate.Action, Payload, OutSpec);
            return true;
        }
    }
    return false;
}

FString McpFinishHudElement(UWidgetBlueprint* WidgetBP, const FString& Action, const FString& SlotName,
                            const TSharedPtr<FJsonObject>& Payload)
{
    if (!Action.Equals(TEXT("add_damage_indicator"), ESearchCase::IgnoreCase))
    {
        return FString();
    }
    // The vignette starts invisible; this animation is what a damage event plays.
    const double Fade = FMath::Max(GetJsonNumberField(Payload, TEXT("fadeTime"), 0.6), 0.05);
    UWidget* Vignette = WidgetBP->WidgetTree->FindWidget(FName(*(SlotName + TEXT("_Vignette"))));
    const FString AnimName = SlotName + TEXT("_Flash");
    return McpAddOpacityAnimation(WidgetBP, AnimName, Vignette,
        { FVector2D(0.0, 0.0), FVector2D(Fade * 0.2, 1.0), FVector2D(Fade, 0.0) }) ? AnimName : FString();
}
}
