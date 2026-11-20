// McpUiConfigLayers.cpp — see header.
#include "Domains/Ui/McpUiConfigLayers.h"

#include "Dom/JsonValue.h"
#include "HAL/PlatformProperties.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
const TCHAR* const Redacted = TEXT("<redacted>");

// The INI files of one config family, lowest layer first. ponytail: the engine's common layers only (no plugin or
// custom layer); a value one of those sets reads as attributionCertain false rather than a wrong layer.
TArray<FString> LayerFiles(const FString& Name)
{
    const FString Platform = ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName());
    return {
        FPaths::EngineConfigDir() / TEXT("Base.ini"),
        FPaths::EngineConfigDir() / FString::Printf(TEXT("Base%s.ini"), *Name),
        FPaths::EngineConfigDir() / Platform / FString::Printf(TEXT("Base%s%s.ini"), *Platform, *Name),
        FPaths::EngineConfigDir() / Platform / FString::Printf(TEXT("%s%s.ini"), *Platform, *Name),
        FPaths::ProjectConfigDir() / FString::Printf(TEXT("Default%s.ini"), *Name),
        FPaths::ProjectConfigDir() / Platform / FString::Printf(TEXT("%s%s.ini"), *Platform, *Name),
        FPaths::GeneratedConfigDir() / (Platform + TEXT("Editor")) / FString::Printf(TEXT("%s.ini"), *Name),
    };
}

// Engine/Config/BaseEngine.ini or Config/DefaultEngine.ini: where it sits, never the machine's own folders.
FString Shown(const FString& Path)
{
    const FString Full = FPaths::ConvertRelativePathToFull(Path);
    const FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    const FString Engine = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
    return Full.StartsWith(Project) ? Full.RightChop(Project.Len()) : Full.StartsWith(Engine) ? TEXT("Engine/") + Full.RightChop(Engine.Len()) : FPaths::GetCleanFilename(Full);
}
} // namespace

namespace McpConfigLayers
{
bool Apply(const FString& Text, const FString& Section, const FString& Key, TArray<FString>& Current)
{
    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, true);
    bool bInSection = false;
    bool bChanged = false;
    for (FString Line : Lines)
    {
        Line.TrimStartAndEndInline();
        if (Line.IsEmpty() || Line.StartsWith(TEXT(";")) || Line.StartsWith(TEXT("#")))
        {
            continue;
        }
        if (Line.StartsWith(TEXT("[")) && Line.EndsWith(TEXT("]")))
        {
            bInSection = Line.Mid(1, Line.Len() - 2).TrimStartAndEnd().Equals(Section, ESearchCase::IgnoreCase);
            continue;
        }
        if (!bInSection)
        {
            continue;
        }
        // +Key=V adds V once, .Key=V adds it even when present, -Key=V removes it, !Key clears the key, Key=V sets it.
        const TCHAR Action = Line[0] == TEXT('+') || Line[0] == TEXT('-') || Line[0] == TEXT('!') || Line[0] == TEXT('.') ? Line[0] : TEXT('=');
        FString Name;
        FString Value;
        if (!(Action == TEXT('=') ? Line : Line.Mid(1)).Split(TEXT("="), &Name, &Value))
        {
            Name = Action == TEXT('!') ? Line.Mid(1) : FString();
        }
        if (!Name.TrimStartAndEnd().Equals(Key, ESearchCase::IgnoreCase))
        {
            continue;
        }
        Value = Value.TrimStartAndEnd().TrimQuotes();
        switch (Action)
        {
        case TEXT('+'): if (!Current.Contains(Value)) Current.Add(Value); break;
        case TEXT('.'): Current.Add(Value); break;
        case TEXT('-'): Current.Remove(Value); break;
        case TEXT('!'): Current.Reset(); break;
        default: Current = {Value}; break;
        }
        bChanged = true;
    }
    return bChanged;
}

bool IsSecretKey(const FString& Key)
{
    const FString Lower = Key.ToLower();
    for (const TCHAR* Word : {TEXT("secret"), TEXT("password"), TEXT("passwd"), TEXT("token"), TEXT("apikey"), TEXT("api_key"), TEXT("accesskey"),
                              TEXT("access_key"), TEXT("credential"), TEXT("privatekey"), TEXT("private_key"), TEXT("authkey"), TEXT("auth_key")})
    {
        if (Lower.Contains(Word)) return true;
    }
    return false;
}

void AddTo(const FString& Section, const FString& Key, const FString& ConfigName, const FString& ConfigFile, FJsonObject& Resp)
{
    const bool bSecret = IsSecretKey(Key);
    TArray<FString> Current;
    FString SetBy;
    TArray<TSharedPtr<FJsonValue>> Layers;
    for (const FString& File : LayerFiles(ConfigName))
    {
        FString Text;
        if (!FPaths::FileExists(File) || !FFileHelper::LoadFileToString(Text, *File) || !Apply(Text, Section, Key, Current)) continue;
        SetBy = Shown(File);
        const TSharedPtr<FJsonObject> Layer = MakeShared<FJsonObject>();
        Layer->SetStringField(TEXT("file"), SetBy);
        TArray<TSharedPtr<FJsonValue>> Values;
        for (const FString& Value : Current) Values.Add(MakeShared<FJsonValueString>(bSecret ? Redacted : *Value));
        Layer->SetArrayField(TEXT("values"), Values);
        Layers.Add(MakeShared<FJsonValueObject>(Layer));
    }
    TArray<FString> Held;
    GConfig->GetArray(*Section, *Key, Held, ConfigFile);
    Resp.SetArrayField(TEXT("layers"), Layers);
    Resp.SetStringField(TEXT("setBy"), Layers.Num() > 0 ? SetBy : FString(TEXT("the class default (no config file sets it)")));
    Resp.SetBoolField(TEXT("attributionCertain"), Held == Current);
}
} // namespace McpConfigLayers
