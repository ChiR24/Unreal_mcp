#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "GameFramework/SaveGame.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Paths.h"

// A game's save slots were out of reach: a test run that raised the player's best score could not be read back or
// put right without a game function written for it. These load a slot the way the game does (UGameplayStatics),
// read or set its saved properties, and delete it.
namespace McpSystemControlHandlers {
namespace {

// The desktop save system writes slot X to Saved/SaveGames/X.sav (FGenericSaveGameSystem).
FString SaveGamesDir() {
  return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames"));
}

// The slot name becomes that file name, so a separator or ".." would reach a .sav outside SaveGames.
bool ReadSaveSlot(const TSharedPtr<FJsonObject>& Payload, FString& OutSlot, int32& OutUser, FString& OutError) {
  double User = 0;
  Payload->TryGetStringField(TEXT("slotName"), OutSlot);
  Payload->TryGetNumberField(TEXT("userIndex"), User);
  OutSlot.TrimStartAndEndInline();
  OutUser = FMath::Max(0, static_cast<int32>(User));
  if (OutSlot.IsEmpty() || OutSlot.Contains(TEXT("..")) || OutSlot.Contains(TEXT("/")) ||
      OutSlot.Contains(TEXT("\\")) || OutSlot.Contains(TEXT(":"))) {
    OutError = FString::Printf(
        TEXT("`slotName` must be a slot as list_save_games shows it (its file name without .sav), not `%s`."), *OutSlot);
    return false;
  }
  return true;
}

// What SaveGameToSlot writes: every property of the object that is not transient.
bool IsSavedProperty(const FProperty* Property) {
  return Property && !Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated);
}

TSharedPtr<FJsonObject> SavedProperties(USaveGame* Save) {
  TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
  for (TFieldIterator<FProperty> It(Save->GetClass()); It; ++It) {
    const TSharedPtr<FJsonValue> Value =
        IsSavedProperty(*It) ? McpPropertyReflection::ExportPropertyToJsonValue(Save, *It) : nullptr;
    if (Value.IsValid()) {
      Props->SetField(It->GetName(), Value);
    }
  }
  return Props;
}

void SetSlotFields(const TSharedPtr<FJsonObject>& Resp, const FString& Slot, USaveGame* Save) {
  Resp->SetStringField(TEXT("slotName"), Slot);
  Resp->SetStringField(TEXT("saveGameClass"), Save->GetClass()->GetPathName());
  Resp->SetObjectField(TEXT("properties"), SavedProperties(Save));
}

// Loads an existing slot; false with the error sent when it is missing or no longer loads (its class is gone).
bool LoadSaveSlot(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, FSystemControlSocket Socket,
                  const FString& Slot, int32 User, USaveGame*& OutSave) {
  if (!UGameplayStatics::DoesSaveGameExist(Slot, User)) {
    Self->SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("No save slot `%s` (user %d); list_save_games without slotName lists the slots."), *Slot, User),
        TEXT("SAVE_SLOT_NOT_FOUND"));
    return false;
  }
  OutSave = UGameplayStatics::LoadGameFromSlot(Slot, User);
  if (!OutSave) {
    Self->SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Save slot `%s` exists but does not load: its SaveGame class is missing or the file is damaged."), *Slot),
        TEXT("SAVE_SLOT_UNREADABLE"));
    return false;
  }
  return true;
}

void SendSaveSlotList(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, FSystemControlSocket Socket) {
  struct FSaveSlotFile {
    FString Name;
    FDateTime Modified;
    int64 Size = 0;
  };
  const FString Dir = SaveGamesDir();
  TArray<FString> Files;
  IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.sav")), true, false);
  TArray<FSaveSlotFile> Slots;
  for (const FString& File : Files) {
    const FString Full = Dir / File;
    Slots.Add({FPaths::GetBaseFilename(File), IFileManager::Get().GetTimeStamp(*Full), IFileManager::Get().FileSize(*Full)});
  }
  Slots.Sort([](const FSaveSlotFile& A, const FSaveSlotFile& B) {
    return A.Modified != B.Modified ? A.Modified > B.Modified : A.Name < B.Name;
  });
  TArray<TSharedPtr<FJsonValue>> Entries;
  for (const FSaveSlotFile& Slot : Slots) {
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("slotName"), Slot.Name);
    Entry->SetNumberField(TEXT("sizeBytes"), static_cast<double>(Slot.Size));
    Entry->SetStringField(TEXT("modified"), Slot.Modified.ToIso8601());
    Entries.Add(MakeShared<FJsonValueObject>(Entry));
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetArrayField(TEXT("slots"), Entries);
  Resp->SetNumberField(TEXT("count"), Entries.Num());
  Self->SendAutomationResponse(Socket, RequestId, true,
                               FString::Printf(TEXT("%d save slot(s) in Saved/SaveGames, newest first"), Entries.Num()), Resp);
}

// Sets every requested property on Save, all or none: false (error sent) when a name is not a saved property or
// a value does not convert. The slot on disk is only written after this succeeds.
bool ApplySavedProperties(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, FSystemControlSocket Socket,
                          USaveGame* Save, const TSharedPtr<FJsonObject>& Props) {
  for (const auto& Pair : Props->Values) {
    const FString Key(Pair.Key.Len(), *Pair.Key);
    FProperty* Property = Save->GetClass()->FindPropertyByName(FName(*Key));
    if (!IsSavedProperty(Property)) {
      TArray<FString> Known;
      for (TFieldIterator<FProperty> It(Save->GetClass()); It; ++It) {
        if (IsSavedProperty(*It)) {
          Known.Add(It->GetName());
        }
      }
      Self->SendAutomationError(Socket, RequestId,
                                FString::Printf(TEXT("`%s` is not a saved property of %s; it saves: %s."), *Key,
                                                *Save->GetClass()->GetName(), *FString::Join(Known, TEXT(", "))),
                                TEXT("UNKNOWN_PROPERTY"));
      return false;
    }
    FString Error;
    if (!McpPropertyReflection::ApplyJsonValueToProperty(Save, Property, Pair.Value, Error)) {
      Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("`%s`: %s"), *Key, *Error),
                                TEXT("INVALID_PROPERTY_VALUE"));
      return false;
    }
  }
  return true;
}

}  // namespace

bool HandleListSaveGames(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, FSystemControlSocket RequestingSocket) {
  if (!Payload->HasField(TEXT("slotName"))) {
    SendSaveSlotList(Self, RequestId, RequestingSocket);
    return true;
  }
  FString Slot;
  FString Error;
  int32 User = 0;
  USaveGame* Save = nullptr;
  if (!ReadSaveSlot(Payload, Slot, User, Error)) {
    Self->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!LoadSaveSlot(Self, RequestId, RequestingSocket, Slot, User, Save)) {
    return true;
  }
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  SetSlotFields(Resp, Slot, Save);
  Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               FString::Printf(TEXT("Read save slot %s (%s)"), *Slot, *Save->GetClass()->GetName()), Resp);
  return true;
}

bool HandleEditSaveGame(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                        const TSharedPtr<FJsonObject>& Payload, FSystemControlSocket RequestingSocket) {
  FString Slot;
  FString Error;
  int32 User = 0;
  if (!ReadSaveSlot(Payload, Slot, User, Error)) {
    Self->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  const TArray<TSharedPtr<FJsonValue>> Changed{MakeShared<FJsonValueString>(Slot)};
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  bool bDelete = false;
  Payload->TryGetBoolField(TEXT("deleteSlot"), bDelete);
  USaveGame* Save = nullptr;
  if (bDelete) {
    // Existence only: a slot whose SaveGame class is gone no longer loads, and is exactly one to delete.
    if (!UGameplayStatics::DoesSaveGameExist(Slot, User)) {
      Self->SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("No save slot `%s` (user %d); list_save_games without slotName lists the slots."), *Slot, User),
          TEXT("SAVE_SLOT_NOT_FOUND"));
      return true;
    }
    UGameplayStatics::DeleteGameInSlot(Slot, User);
    const bool bExistsAfter = UGameplayStatics::DoesSaveGameExist(Slot, User);
    Resp->SetStringField(TEXT("slotName"), Slot);
    Resp->SetBoolField(TEXT("deleted"), !bExistsAfter);
    Resp->SetBoolField(TEXT("existsAfter"), bExistsAfter);
    Resp->SetArrayField(TEXT("changedEntities"), Changed);
    Self->SendAutomationResponse(RequestingSocket, RequestId, !bExistsAfter,
                                 bExistsAfter ? FString::Printf(TEXT("Could not delete save slot %s"), *Slot)
                                              : FString::Printf(TEXT("Deleted save slot %s"), *Slot),
                                 Resp, bExistsAfter ? TEXT("DELETE_FAILED") : FString());
    return true;
  }
  const TSharedPtr<FJsonObject>* Props = nullptr;
  Payload->TryGetObjectField(TEXT("properties"), Props);
  const bool bHasProps = Props && (*Props).IsValid() && (*Props)->Values.Num() > 0;
  // A new slot may be written with its class defaults alone; an existing one needs something to change.
  const bool bCreate = !UGameplayStatics::DoesSaveGameExist(Slot, User);
  if (!bHasProps && !bCreate) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Pass `properties` to set (name to value), or `deleteSlot`: true."),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString ClassPath;
  Payload->TryGetStringField(TEXT("saveGameClass"), ClassPath);
  UClass* NewClass = bCreate && !ClassPath.IsEmpty() ? LoadClass<USaveGame>(nullptr, *ClassPath) : nullptr;
  // SaveGame itself is abstract: CreateSaveGameObject refuses it, and so does any abstract subclass.
  if (bCreate && (!NewClass || NewClass->HasAnyClassFlags(CLASS_Abstract))) {
    FString Why = TEXT("none was given");
    if (NewClass) {
      Why = FString::Printf(TEXT("`%s` is abstract"), *ClassPath);
    } else if (!ClassPath.IsEmpty()) {
      Why = FString::Printf(TEXT("`%s` is not one"), *ClassPath);
    }
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("No save slot `%s`; pass saveGameClass, a SaveGame subclass, to create it (%s)."), *Slot, *Why),
        ClassPath.IsEmpty() ? TEXT("SAVE_SLOT_NOT_FOUND") : TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (bCreate) {
    Save = UGameplayStatics::CreateSaveGameObject(NewClass);
    if (!Save) {
      Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Could not create a %s to save."), *NewClass->GetName()),
                                TEXT("SAVE_FAILED"));
      return true;
    }
  } else if (!LoadSaveSlot(Self, RequestId, RequestingSocket, Slot, User, Save)) {
    return true;
  }
  if (bHasProps && !ApplySavedProperties(Self, RequestId, RequestingSocket, Save, *Props)) {
    return true;
  }
  const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, Slot, User);
  // Read back from disk, so the reply shows what the game will load.
  USaveGame* Reloaded = bSaved ? UGameplayStatics::LoadGameFromSlot(Slot, User) : nullptr;
  SetSlotFields(Resp, Slot, Reloaded ? Reloaded : Save);
  Resp->SetBoolField(TEXT("created"), bCreate);
  Resp->SetBoolField(TEXT("saved"), bSaved);
  Resp->SetArrayField(TEXT("changedEntities"), Changed);
  Self->SendAutomationResponse(RequestingSocket, RequestId, bSaved,
                               bSaved ? FString::Printf(TEXT("Saved slot %s"), *Slot)
                                      : FString::Printf(TEXT("Could not write save slot %s"), *Slot),
                               Resp, bSaved ? FString() : TEXT("SAVE_FAILED"));
  return true;
}

}  // namespace McpSystemControlHandlers
