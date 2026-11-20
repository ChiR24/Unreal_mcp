#include "Domains/ControlActor/Motion/McpMotionPlay.h"

#include "Containers/Ticker.h"
#include "Domains/ControlActor/Motion/McpMotionRun.h"
#include "Domains/Play/McpPlaySession.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Responses/McpResponses.h"

namespace
{
TSet<FString> GOwnPlay; // game thread only: the requests whose Play this server started
constexpr double PlayWaitSeconds = 20.0;
}

void McpSampleMotionInOwnPlay(UEngineWireSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              TFunction<void()> Run)
{
    McpPlaySession::RequestPlay();
    GOwnPlay.Add(RequestId);
    const double Deadline = FPlatformTime::Seconds() + PlayWaitSeconds;
    TWeakObjectPtr<UEngineWireSubsystem> Weak(Self);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, RequestId, Socket, Deadline, Run](float)
    {
        UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        const bool bStarted = World && World->HasBegunPlay();
        if (!Weak.IsValid() || (!bStarted && FPlatformTime::Seconds() < Deadline))
        {
            return Weak.IsValid();
        }
        if (!bStarted)
        {
            GOwnPlay.Remove(RequestId);
            SendStandardErrorResponse(Weak.Get(), Socket, RequestId, TEXT("PIE_START_TIMEOUT"),
                                      TEXT("startPlay asked for Play In Editor, but no play world began within 20 s."));
            return false;
        }
        McpPlaySession::NoteStarted(World);
        Run();
        // Still marked: the handler refused before its run took the Play over, so nothing else will end it.
        if (GOwnPlay.Remove(RequestId) > 0 && GEditor)
        {
            GEditor->RequestEndPlayMap();
        }
        return false;
    }), 0.0f);
}

bool McpMotionTakeOwnPlay(const FString& RequestId)
{
    return GOwnPlay.Remove(RequestId) > 0;
}

void McpEndOwnPlayThen(TFunction<void(bool)> Then)
{
    if (GEditor && GEditor->PlayWorld)
    {
        GEditor->RequestEndPlayMap();
    }
    const double Deadline = FPlatformTime::Seconds() + PlayWaitSeconds;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Deadline, Then](float)
    {
        const bool bEnded = !GEditor || !GEditor->PlayWorld;
        if (!bEnded && FPlatformTime::Seconds() < Deadline)
        {
            return true;
        }
        Then(bEnded);
        return false;
    }), 0.0f);
}
