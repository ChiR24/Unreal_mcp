#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#if WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "UObject/UObjectIterator.h"

namespace {
// The user widgets a PIE player can actually see: in the play world, and either
// added to the viewport or nested in a panel of one that is.
TArray<UUserWidget *> LiveUserWidgetsForMcp() {
  TArray<UUserWidget *> Widgets;
  UWorld *PlayWorld = GEditor ? GEditor->PlayWorld.Get() : nullptr;
  if (!PlayWorld) {
    return Widgets;
  }
  for (TObjectIterator<UUserWidget> It; It; ++It) {
    UUserWidget *Widget = *It;
    if (IsValid(Widget) && Widget->WidgetTree &&
        !Widget->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) &&
        Widget->GetWorld() == PlayWorld &&
        (Widget->IsInViewport() || Widget->GetParent() != nullptr)) {
      Widgets.Add(Widget);
    }
  }
  return Widgets;
}

bool IsDrivableWidgetForMcp(const UWidget *Widget) {
  return Widget->IsA<UButton>() || Widget->IsA<UCheckBox>() ||
         Widget->IsA<USlider>();
}

// "WBP_MainMenu" names the widget whose class is WBP_MainMenu_C, so the owner
// half of Owner.Name reads the way the asset is named.
FString OwnerLabelForMcp(const UUserWidget *Owner) {
  FString Label = Owner->GetClass()->GetName();
  Label.RemoveFromEnd(TEXT("_C"));
  return Label;
}

TSharedPtr<FJsonObject> DescribeWidgetForMcp(UWidget *Widget) {
  TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
  Entry->SetStringField(TEXT("name"), Widget->GetName());
  Entry->SetStringField(TEXT("type"), Widget->GetClass()->GetName());
  Entry->SetBoolField(TEXT("enabled"), Widget->GetIsEnabled());
  Entry->SetBoolField(TEXT("visible"), Widget->IsVisible());
  if (Widget->HasKeyboardFocus()) {
    Entry->SetBoolField(TEXT("focused"), true);
  }
  if (UButton *Button = Cast<UButton>(Widget)) {
    if (UTextBlock *Label = Cast<UTextBlock>(Button->GetContent())) {
      Entry->SetStringField(TEXT("text"), Label->GetText().ToString());
    }
  } else if (USlider *Slider = Cast<USlider>(Widget)) {
    Entry->SetNumberField(TEXT("value"), Slider->GetValue());
  } else if (UCheckBox *Box = Cast<UCheckBox>(Widget)) {
    Entry->SetBoolField(TEXT("checked"), Box->IsChecked());
  }
  return Entry;
}

bool ListLiveWidgetsForMcp(const TSharedPtr<FJsonObject> &Resp,
                           FString &Message) {
  TArray<TSharedPtr<FJsonValue>> Owners;
  for (UUserWidget *Owner : LiveUserWidgetsForMcp()) {
    TArray<TSharedPtr<FJsonValue>> Children;
    Owner->WidgetTree->ForEachWidget([&Children](UWidget *Child) {
      if (Child && IsDrivableWidgetForMcp(Child)) {
        Children.Add(MakeShared<FJsonValueObject>(DescribeWidgetForMcp(Child)));
      }
    });
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("userWidget"), OwnerLabelForMcp(Owner));
    Entry->SetStringField(TEXT("object"), Owner->GetName());
    Entry->SetBoolField(TEXT("inViewport"), Owner->IsInViewport());
    Entry->SetArrayField(TEXT("children"), Children);
    Owners.Add(MakeShared<FJsonValueObject>(Entry));
  }
  Resp->SetArrayField(TEXT("widgets"), Owners);
  if (!GEditor || !GEditor->PlayWorld) {
    Message = TEXT("No PIE session is running, so there is no live UI to list.");
    return false;
  }
  Message = FString::Printf(TEXT("%d live user widget(s) in the PIE session."),
                            Owners.Num());
  return true;
}

// Press / toggle / set exactly as the widget's own events would fire for a
// player, so the Blueprint handlers bound to them run -- without the OS cursor.
bool DriveWidgetForMcp(UWidget *Widget, const TSharedPtr<FJsonObject> &Payload,
                       FString &Message) {
  if (UButton *Button = Cast<UButton>(Widget)) {
    Button->OnPressed.Broadcast();
    Button->OnReleased.Broadcast();
    Button->OnClicked.Broadcast();
    Message = TEXT("pressed");
    return true;
  }
  if (UCheckBox *Box = Cast<UCheckBox>(Widget)) {
    const bool bChecked = !Box->IsChecked();
    Box->SetIsChecked(bChecked);
    Box->OnCheckStateChanged.Broadcast(bChecked);
    Message = bChecked ? TEXT("checked") : TEXT("unchecked");
    return true;
  }
  if (USlider *Slider = Cast<USlider>(Widget)) {
    double Value = 0.0;
    if (!Payload->TryGetNumberField(TEXT("value"), Value)) {
      Message = TEXT("a Slider needs `value`, the value to drag it to");
      return false;
    }
    Slider->SetValue(static_cast<float>(Value));
    Slider->OnValueChanged.Broadcast(Slider->GetValue());
    Message = FString::Printf(TEXT("set to %g"), Slider->GetValue());
    return true;
  }
  Message = FString::Printf(
      TEXT("is a %s; widget_click drives a Button, CheckBox or Slider"),
      *Widget->GetClass()->GetName());
  return false;
}

bool ClickLiveWidgetForMcp(const TSharedPtr<FJsonObject> &Payload,
                           const TSharedPtr<FJsonObject> &Resp,
                           FString &Message) {
  FString Target;
  Payload->TryGetStringField(TEXT("widget"), Target);
  FString OwnerFilter;
  FString WidgetName = Target;
  Target.Split(TEXT("."), &OwnerFilter, &WidgetName);
  if (WidgetName.IsEmpty()) {
    Message = TEXT("widget_click needs `widget`: a widget name, or Owner.Name "
                   "when several live widgets share it (widget_list shows both).");
    return false;
  }

  TArray<TPair<UUserWidget *, UWidget *>> Matches;
  for (UUserWidget *Owner : LiveUserWidgetsForMcp()) {
    if (!OwnerFilter.IsEmpty() && OwnerLabelForMcp(Owner) != OwnerFilter &&
        Owner->GetName() != OwnerFilter) {
      continue;
    }
    if (UWidget *Found = Owner->WidgetTree->FindWidget(FName(*WidgetName))) {
      Matches.Add({Owner, Found});
    }
  }
  if (Matches.Num() != 1) {
    TArray<FString> Names;
    for (const TPair<UUserWidget *, UWidget *> &Match : Matches) {
      Names.Add(OwnerLabelForMcp(Match.Key) + TEXT(".") + Match.Value->GetName());
    }
    Message = Matches.Num() == 0
                  ? FString::Printf(TEXT("No live widget named '%s' in the PIE "
                                         "session; call widget_list to see what is on screen."),
                                    *Target)
                  : FString::Printf(TEXT("'%s' is ambiguous: %s. Name one as Owner.Name."),
                                    *Target, *FString::Join(Names, TEXT(", ")));
    return false;
  }

  UUserWidget *Owner = Matches[0].Key;
  UWidget *Widget = Matches[0].Value;
  const FString Label = OwnerLabelForMcp(Owner) + TEXT(".") + Widget->GetName();
  Resp->SetStringField(TEXT("widget"), Label);
  Resp->SetStringField(TEXT("widgetType"), Widget->GetClass()->GetName());
  if (!Widget->GetIsEnabled() || !Widget->IsVisible()) {
    Message = FString::Printf(TEXT("%s is %s, so a player could not press it."),
                              *Label, Widget->GetIsEnabled() ? TEXT("hidden") : TEXT("disabled"));
    return false;
  }
  FString Outcome;
  const bool bDriven = DriveWidgetForMcp(Widget, Payload, Outcome);
  Message = FString::Printf(TEXT("%s %s"), *Label, *Outcome);
  return bDriven;
}
} // namespace

bool SimulateLiveWidgetInputForMcp(const FString &InputType,
                                   const TSharedPtr<FJsonObject> &Payload,
                                   const TSharedPtr<FJsonObject> &Resp,
                                   FString &Message) {
  if (InputType == TEXT("widget_list")) {
    return ListLiveWidgetsForMcp(Resp, Message);
  }
  return ClickLiveWidgetForMcp(Payload, Resp, Message);
}
#endif
