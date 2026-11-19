#pragma once

#include "CoreMinimal.h"

class UObject;

// What an asset refers to that will load as nothing (McpMissingReferences.cpp): soft references at any depth through
// structs and lists on every object of its package, and a Blueprint's object and class pin defaults. Each finding is
// logged as a warning naming the path and what holds it, so the call that compiled or saved the asset reports it in its
// reply's warnings: an asset that looks right but references what the engine cannot resolve fails only at runtime.
enum class EMcpMissingReferenceCheck : uint8
{
    Compile, // a path no package anywhere answers to (a typo, a deleted asset)
    Save,    // that, and a reference into a package with no file on disk (made this session and never saved)
};

void McpWarnMissingReferences(UObject* Asset, EMcpMissingReferenceCheck Check);
