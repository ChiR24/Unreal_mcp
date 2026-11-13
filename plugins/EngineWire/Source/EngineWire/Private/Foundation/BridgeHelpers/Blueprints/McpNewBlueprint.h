#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "Factories/BlueprintFactory.h"
#include "Runtime/Launch/Resources/Version.h"

// A new Blueprint of ParentClass named Name in Package, made by the editor's Blueprint factory as the Content Browser
// makes one, or null when the factory refuses. Type Interface makes a Blueprint Interface where the factory takes a type
// (5.1 on); 5.0's factory makes a normal Blueprint, so a caller that needs the type also sets it on the result. Nothing
// is registered, dirtied, compiled or saved here: each caller does what its action promises. The caller makes the
// package and refuses an asset already at the path first: a create over an existing Blueprint asserts in the engine,
// and CreatePackage hands an unloaded package back empty.
inline UBlueprint* McpNewBlueprint(UPackage* Package, const FName Name, UClass* ParentClass,
                                   const EBlueprintType Type = BPTYPE_Normal)
{
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = ParentClass;
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
    Factory->BlueprintType = Type;
#else
    (void)Type;
#endif
    return Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, Name, RF_Public | RF_Standalone, nullptr, GWarn));
}
