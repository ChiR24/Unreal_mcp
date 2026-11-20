#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Item 15: which config layer set a value. The layers are the engine's INI hierarchy read as text, lowest first
// (Base.ini, Base<Name>.ini, the platform's, the project's Default<Name>.ini and its platform file, then the editor's
// saved user file), each one's + . - ! line actions applied in order, so a value is attributed to the last layer that
// changed it. Reading files keeps it the same on every 5.x. A plugin's own config layer is not read, so when the merge here
// disagrees with what GConfig holds the reply says the attribution is uncertain and gives GConfig's value, never a guess.
namespace McpConfigLayers
{
/** Current after Text (an INI file's text) acted on Section.Key; true when a line changed it. */
bool Apply(const FString& Text, const FString& Section, const FString& Key, TArray<FString>& Current);

/** A key whose value is withheld from every reply: a secret, password, token, API or access key, credential or private key. */
bool IsSecretKey(const FString& Key);

/** layers (each file that sets Section.Key, with the values after it), setBy and attributionCertain onto Resp, the merge
 *  checked against GConfig's values in ConfigFile (the settings class's own config filename). */
void AddTo(const FString& Section, const FString& Key, const FString& ConfigName, const FString& ConfigFile, FJsonObject& Resp);
} // namespace McpConfigLayers
