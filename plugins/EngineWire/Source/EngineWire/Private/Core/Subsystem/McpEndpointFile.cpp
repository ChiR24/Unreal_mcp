// McpEndpointFile.cpp — see the header. The file sits beside the capability token (Saved/MCP), which the proxy reads
// from the same project folder.

#include "Core/Subsystem/McpEndpointFile.h"
#include "Core/Module/McpOwnPlugin.h"
#include "EngineWireLog.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpEndpointFileState
{
// ponytail: one native server per editor, so the last bind is the one the file names.
int32 Port = 0;
FString StartedAtUtc;

void Write(const TCHAR* State)
{
    if (Port <= 0)
    {
        return;
    }
    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("pid"), static_cast<double>(FPlatformProcess::GetCurrentProcessId()));
    Json->SetNumberField(TEXT("port"), Port);
    const TSharedPtr<IPlugin> Plugin = McpOwnPlugin();
    if (Plugin.IsValid())
    {
        Json->SetStringField(TEXT("pluginVersion"), Plugin->GetDescriptor().VersionName);
    }
    Json->SetStringField(TEXT("startedAtUtc"), StartedAtUtc);
    Json->SetStringField(TEXT("state"), State);
    McpEndpointFile::WriteWhole(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MCP"), TEXT("endpoint.json")), Json);
}
} // namespace McpEndpointFileState

namespace McpEndpointFile
{
bool WriteWhole(const FString& Path, const TSharedRef<FJsonObject>& Json)
{
    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    FJsonSerializer::Serialize(Json, Writer);
    const FString Temp = Path + TEXT(".tmp");
    if (FFileHelper::SaveStringToFile(Text, *Temp) && IFileManager::Get().Move(*Path, *Temp, true))
    {
        return true;
    }
    UE_LOG(LogEngineWireSubsystem, Warning, TEXT("Could not write %s; the stdio proxy reads it to find this editor and its calls."), *Path);
    return false;
}

void WriteListening(int32 Port)
{
    McpEndpointFileState::Port = Port;
    McpEndpointFileState::StartedAtUtc = FDateTime::UtcNow().ToIso8601();
    McpEndpointFileState::Write(TEXT("listening"));
}

void WriteClosed()
{
    McpEndpointFileState::Write(TEXT("closed_cleanly"));
}

int32 ProjectPort()
{
    const FString Project = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath()).ToLower();
    return 49152 + static_cast<int32>(FCrc::StrCrc32(*Project) % 16384u);
}
} // namespace McpEndpointFile
