// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "EngineWireSubsystem.h"

#include "Dom/JsonObject.h"

class UMaterialInstanceConstant;
class UMaterialInterface;

// A material instance's parent, set as the material instance editor sets it. An instance with shaders of its own (a
// static switch or a base property override, usage flags included) keeps them compiled from its old parent when only
// the Parent property is written, so it went on drawing the old parent while Parent read the new one.
// SetParentEditorOnly compiles them for the new parent, and an undo record does the same when the change is undone or
// redone (the instance's saved state alone restores the property, never the shaders).
namespace McpMaterialParent
{
// The parent Object holds when it is a material instance constant, else null.
UMaterialInterface* HeldParent(const UObject* Object);

// Why To cannot be Instance's parent (not a Material or Material Instance asset, or Instance itself or one of its
// children), or empty when it can. A null To is allowed: the instance then draws the default material.
FString Refusal(const UMaterialInstanceConstant* Instance, const UMaterialInterface* To);

// Makes To the parent of Instance inside the open transaction (Instance already Modify'd): its own shaders compiled for
// To, what draws it redrawn, and an undo record that rebuilds it for Old on undo and for To on redo.
void SetParent(UMaterialInstanceConstant* Instance, UMaterialInterface* Old, UMaterialInterface* To);

// set_property wrote PropertyPath on Object. When that is a material instance constant's Parent, the written parent is
// applied through SetParent, or Old is put back and OutError says why (false). True when there was nothing to refuse.
bool ApplyWrittenParent(UObject* Object, const FString& PropertyPath, UMaterialInterface* Old, FString& OutError);

// manage_asset reparent_instance (edit_material_instance edit=reparent).
bool HandleSetParent(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                     TSharedPtr<FMcpBridgeWebSocket> Socket);
}
