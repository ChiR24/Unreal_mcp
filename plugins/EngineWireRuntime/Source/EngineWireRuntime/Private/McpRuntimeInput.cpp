// runtime.press_key: a key press in the running game, held for holdSeconds (default 0.1) and released, through the
// game viewport as a real key would come (Enhanced Input sees it), else the first player controller. The cursor never
// moves and no window takes focus.
#include "McpRuntimeReads.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#if MCP_RT_INPUT_DEVICE_MAPPER
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#endif

namespace McpRuntimeInputDetail
{
bool Route(UWorld& World, const FKey& Key, EInputEvent Event)
{
    const float Amount = Event == IE_Released ? 0.0f : 1.0f;
    UGameViewportClient* Client = World.GetGameViewport();
    FViewport* Viewport = Client ? Client->GetGameViewport() : nullptr;
#if MCP_RT_INPUT_DEVICE_MAPPER
    const FInputDeviceId Device = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
#endif
    if (Viewport)
    {
#if MCP_RT_INPUT_ARGS_SIMULATED
        const FInputKeyEventArgs Args(Viewport, Device, Key, Event, Amount, false, FPlatformTime::Cycles64());
#elif MCP_RT_INPUT_DEVICE_MAPPER
        const FInputKeyEventArgs Args(Viewport, Device, Key, Event, Amount, false);
#else
        const FInputKeyEventArgs Args(Viewport, 0, Key, Event, Amount, false);
#endif
        if (Client->InputKey(Args))
        {
            return true;
        }
    }
    APlayerController* Player = World.GetFirstPlayerController();
    if (!Player)
    {
        return false;
    }
#if MCP_RT_INPUT_ARGS_SIMULATED
    return Player->InputKey(FInputKeyEventArgs::CreateSimulated(Key, Event, Amount, 1, Device));
#elif MCP_RT_INPUT_DEVICE_MAPPER
    return Player->InputKey(FInputKeyParams(Key, Event, static_cast<double>(Amount), Key.IsGamepadKey(), Device));
#else
    return Player->InputKey(FInputKeyParams(Key, Event, static_cast<double>(Amount), Key.IsGamepadKey()));
#endif
}
}

TSharedPtr<FJsonObject> McpRuntimePressKey(UWorld& World, const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
    using namespace McpRuntimeInputDetail;
    FString Name;
    double Hold = 0.1;
    Params->TryGetStringField(TEXT("key"), Name);
    Params->TryGetNumberField(TEXT("holdSeconds"), Hold);
    const FKey Key(*Name);
    if (!Key.IsValid())
    {
        OutError = FString::Printf(TEXT("INVALID_KEY: '%s' is no key name (SpaceBar, W, LeftMouseButton, Gamepad_FaceButton_Bottom)."), *Name);
        return nullptr;
    }
    const bool bHandled = Route(World, Key, IE_Pressed);
    TWeakObjectPtr<UWorld> Weak(&World);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, Key](float)
    {
        if (UWorld* Still = Weak.Get())
        {
            Route(*Still, Key, IE_Released);
        }
        return false;
    }), static_cast<float>(FMath::Clamp(Hold, 0.0, 10.0)));
    const TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("key"), Key.GetFName().ToString());
    Out->SetBoolField(TEXT("handled"), bHandled);
    Out->SetNumberField(TEXT("holdSeconds"), FMath::Clamp(Hold, 0.0, 10.0));
    return Out;
}
