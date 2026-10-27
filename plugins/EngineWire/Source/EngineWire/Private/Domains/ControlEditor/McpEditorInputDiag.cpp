#include "Domains/ControlEditor/McpEditorSupport.h"

// The pawn a PIE input drives, by the name actor calls take: the reply comes back while a held key is still down, so
// this is what to read once the hold is over. It replaced a snapshot taken before the game had ticked (legacy axis
// bindings, MoveForward/MoveRight mapping counts, a key value and movement vectors) that read 0 for a key that was
// moving the pawn.
void AddInputPawnForMcp(const TSharedPtr<FJsonObject> &Resp) {
  UWorld *PlayWorld = GEditor ? GEditor->PlayWorld.Get() : nullptr;
  APlayerController *Controller = PlayWorld ? PlayWorld->GetFirstPlayerController() : nullptr;
  if (const APawn *Pawn = Controller ? Controller->GetPawn() : nullptr) {
    Resp->SetStringField(TEXT("pawn"), McpActorRef(Pawn));
  }
}
