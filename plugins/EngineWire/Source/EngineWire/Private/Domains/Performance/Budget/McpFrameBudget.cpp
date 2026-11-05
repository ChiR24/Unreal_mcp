// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Performance/Budget/McpFrameBudget.h"

#include "Containers/Ticker.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpFrameBudget
{
namespace
{
const FText& BudgetOverrideText()
{
    static const FText Text = NSLOCTEXT("EngineWire", "McpFrameBudget", "EngineWire frame budget");
    return Text;
}
} // namespace

double FRun::BaselineMs() const
{
    TArray<double> Sorted = Bases;
    Sorted.Sort();
    return Sorted.Num() > 0 ? Sorted[Sorted.Num() / 2] : 0.0;
}

bool ClientAlive(const FRun& Run)
{
    return GEditor && GEditor->GetAllViewportClients().Contains(Run.Client);
}

void RestoreView(FRun& Run)
{
    for (FKnob& Knob : Run.Knobs)
    {
        if (Knob.bOn)
        {
            SetKnob(Knob, false);
        }
    }
    if (ClientAlive(Run))
    {
        Run.Client->SetGameView(Run.bWasGameView);
        Run.Client->RemoveRealtimeOverride(BudgetOverrideText(), false);
        Run.Client->Invalidate();
    }
    EndEditorRunForMcp(Run.Hold);
}

bool Handle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const auto Refuse = [&](const TCHAR* Code, const FString& Message) {
        Self->SendAutomationResponse(Socket, RequestId, false, Message, nullptr, Code);
        return true;
    };
    double Budget = 0.0;
    if (!Payload->TryGetNumberField(TEXT("budgetMs"), Budget) || Budget < 2.0 || Budget > 100.0)
    {
        return Refuse(TEXT("INVALID_ARGUMENT"), TEXT("budgetMs is the GPU frame time to fit, 2 to 100 milliseconds (16.7 is 60 frames a second)."));
    }
    const TArray<FString> KnobKinds = McpHandlerUtils::GetStringArrayField(Payload, TEXT("knobs"));
    const bool bShadows = KnobKinds.Num() == 0 || KnobKinds.Contains(TEXT("shadows"));
    const bool bDistance = KnobKinds.Num() == 0 || KnobKinds.Contains(TEXT("draw_distance"));
    if (!bShadows && !bDistance)
    {
        return Refuse(TEXT("INVALID_ARGUMENT"), TEXT("knobs takes shadows and draw_distance."));
    }
    if (GEditor->PlayWorld)
    {
        return Refuse(TEXT("PIE_ACTIVE"), TEXT("fit_frame_budget measures the level viewport, which Play covers: stop Play first."));
    }
    FEditorViewportClient* Client = GetActiveEditorViewportClientForMcp();
    if (!Client || !Client->Viewport || Client->Viewport->GetSizeXY().X <= 0 || Client->Viewport->GetSizeXY().Y <= 0)
    {
        return Refuse(TEXT("NO_VIEWPORT"), TEXT("No level viewport is open to measure."));
    }
    BringLevelEditorTabToFrontForMcp();
    PlaceViewCameraForMcp(Client, Payload);

    const TSharedRef<FRun> Run = MakeShared<FRun>();
    Run->Self = Self;
    Run->RequestId = RequestId;
    Run->Payload = Payload;
    Run->Socket = Socket;
    Run->Client = Client;
    Run->BudgetMs = Budget;
    Payload->TryGetBoolField(TEXT("dryRun"), Run->bDryRun);
    FindKnobs(*Run, bShadows, bDistance);
    if (Run->Knobs.Num() == 0)
    {
        TArray<FString> Missing;
        if (bShadows)
        {
            Missing.Add(Run->bNoDynamicShadows ? FString(TEXT("the editor draws no dynamic shadows at its shadow quality (scalability "
                                                              "Low), so no light's shadows cost anything to turn off"))
                                               : FString(TEXT("no movable light casting shadows reaches the view")));
        }
        if (bDistance)
        {
            Missing.Add(TEXT("no small mesh in it lies beyond 20 m"));
        }
        return Refuse(TEXT("NOTHING_TO_TRY"), TEXT("Nothing here to turn down: ") + FString::Join(Missing, TEXT(", and ")) +
                                                  TEXT(". Move the view (location, rotation) to where the frame is slow."));
    }
    // A minimized editor draws nothing and a background one a few frames a second: held on screen, unthrottled, realtime.
    Run->Hold = BeginEditorRunForMcp();
    Run->bWasGameView = Client->IsInGameView();
    Client->SetGameView(true);
    Client->AddRealtimeOverride(true, BudgetOverrideText());
    Client->Invalidate();
    Run->Started = FPlatformTime::Seconds();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float) { return Tick(Run); }));
    return true;
}
} // namespace McpFrameBudget
