// Copyright (c) 2024 EngineWire Contributors
#include "Domains/ControlEditor/Viewport/McpEditorShowFlags.h"

#include "Domains/ControlEditor/McpEditorSupport.h"
#include "EditorViewportClient.h"
#include "ShowFlags.h"

namespace
{
// Names the viewport's Show menu uses, offered when a name is not a show flag.
const TCHAR* const CommonFlags = TEXT("Grid, Collision, Bounds, Navigation, Fog, PostProcessing, Lighting, Particles, "
                                      "Decals, Volumes, Splines, Landscape, Foliage, StaticMeshes, SkeletalMeshes, "
                                      "Translucency, Atmosphere, BillboardSprites");
} // namespace

bool McpEditorShowFlags::Handle(UEngineWireSubsystem* Bridge, const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const TSharedPtr<FJsonObject>* Flags = nullptr;
    if (!Payload->TryGetObjectField(TEXT("flags"), Flags) || !Flags || (*Flags)->Values.Num() == 0)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("flags must name at least one show flag, as {\"Grid\": false}"),
                                    TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // Every name is read first, so a call naming one the engine does not have changes nothing.
    TArray<TPair<int32, bool>> Wanted;
    TArray<FString> Unknown;
    for (const auto& Entry : (*Flags)->Values)
    {
        const FString Name(*Entry.Key); // 5.8 keys are not FString
        const int32 Index = FEngineShowFlags::FindIndexByName(*Name);
        bool bOn = false;
        if (Index == INDEX_NONE || !Entry.Value.IsValid() || !Entry.Value->TryGetBool(bOn))
        {
            Unknown.Add(Name);
            continue;
        }
        Wanted.Emplace(Index, bOn);
    }
    if (Unknown.Num() > 0)
    {
        Bridge->SendAutomationError(
            Socket, RequestId,
            FString::Printf(TEXT("Not a show flag set to true or false: %s. Names as the viewport's Show menu has them: %s."),
                            *FString::Join(Unknown, TEXT(", ")), CommonFlags),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // While the player plays, the game viewport is on screen and the level viewport these flags belong to is behind it.
    if (GEditor && GEditor->PlayWorld && !GetEjectedPieViewportClientForMcp())
    {
        Bridge->SendAutomationError(Socket, RequestId,
                                    TEXT("Play In Editor is running and the game viewport is on screen; show flags are "
                                         "the level viewport's. Eject first (control_editor play control=eject)."),
                                    TEXT("PIE_VIEW_NOT_EJECTED"));
        return true;
    }
    FEditorViewportClient* Client = GetActiveEditorViewportClientForMcp();
    if (!Client)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("No level viewport is open"), TEXT("NO_VIEWPORT"));
        return true;
    }
    TSharedPtr<FJsonObject> State = MakeShared<FJsonObject>();
    int32 Changed = 0;
    for (const TPair<int32, bool>& Each : Wanted)
    {
        Changed += Client->EngineShowFlags.GetSingleFlag(Each.Key) != Each.Value ? 1 : 0;
        Client->EngineShowFlags.SetSingleFlag(Each.Key, Each.Value);
        State->SetBoolField(FEngineShowFlags::FindNameByIndex(Each.Key), Client->EngineShowFlags.GetSingleFlag(Each.Key));
    }
    Client->Invalidate();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetObjectField(TEXT("flags"), State);
    Result->SetNumberField(TEXT("changed"), Changed);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                                   Changed > 0 ? FString::Printf(TEXT("%d show flag(s) changed"), Changed)
                                               : FString(TEXT("The show flags were already so")),
                                   Result);
    return true;
}
