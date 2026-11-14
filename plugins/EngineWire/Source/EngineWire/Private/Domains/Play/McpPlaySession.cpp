#include "Domains/Play/McpPlaySession.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Solvers/McpPratt.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace McpPlaySession
{
namespace
{
// The session's game instance, not its world: a level change inside the session (a restart, an OpenLevel) makes a new
// world, while the game instance lives from Play's start to its end.
TWeakObjectPtr<UGameInstance> GStarted;
}

void NoteStarted(UWorld* PlayWorld)
{
    GStarted = PlayWorld ? PlayWorld->GetGameInstance() : nullptr;
}

bool StartedByMcp(const UWorld* PlayWorld)
{
    return PlayWorld && PlayWorld->GetGameInstance() && GStarted.Get() == PlayWorld->GetGameInstance();
}

APawn* ResolvePawn(UWorld* World, const FString& Name)
{
    if (!Name.IsEmpty())
    {
        return Cast<APawn>(FindActorByNameInWorldForMcp(World, Name, false));
    }
    const APlayerController* Player = World->GetFirstPlayerController();
    return Player ? Player->GetPawn() : nullptr;
}

bool ParseAssertions(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field, TArray<TSharedPtr<McpPratt::FNode>>& Roots,
                     TArray<FString>& Texts, FString& OutError)
{
    constexpr int32 MaxAssertions = 16;
    for (const FString& Text : McpHandlerUtils::GetStringArrayField(Payload, Field))
    {
        TSharedPtr<McpPratt::FNode> Root;
        FString Why;
        if (Roots.Num() >= MaxAssertions || !McpPratt::Parse(Text, Root, Why))
        {
            OutError = Roots.Num() >= MaxAssertions ? FString::Printf(TEXT("%s takes %d assertions at most."), Field, MaxAssertions)
                                                    : FString::Printf(TEXT("%s '%s': %s"), Field, *Text, *Why);
            return false;
        }
        Roots.Add(Root);
        Texts.Add(Text);
    }
    return true;
}
} // namespace McpPlaySession
