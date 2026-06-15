#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpSecurityPrompts.h"

namespace UnrealAgent::AutomationTests
{
    const TArray<FString>& GetDestructiveSecurityPrompts()
    {
        static const TArray<FString> Prompts = {
            TEXT("tar extract root path"),
            TEXT("tar extract flagless path"),
            TEXT("tar quoted executable path"),
            TEXT("tar split executable path"),
            TEXT("python tarfile extract path"),
            TEXT("bsdtar extract path"),
            TEXT("destructive python rmtree path"),
            TEXT("reordered recursive remove path"),
            TEXT("reversed structured remove path"),
            TEXT("nested reversed structured remove path"),
            TEXT("quote split recursive remove path"),
            TEXT("nonrecursive root remove path"),
            TEXT("opaque read blob remove path"),
            TEXT("opaque read value shell remove path"),
            TEXT("opaque read body root remove path"),
            TEXT("pwd wildcard remove path"),
            TEXT("braced pwd wildcard remove path"),
            TEXT("shell wrapped root remove path"),
            TEXT("filesystem root remove path"),
            TEXT("parent root remove path"),
            TEXT("absolute working root remove path"),
            TEXT("python tarfile api extract path"),
            TEXT("python zipfile api extract path"),
            TEXT("python shutil unpack archive path"),
            TEXT("python patool extract archive path"),
            TEXT("python rarfile extract path"),
            TEXT("tree long output path"),
            TEXT("git option reset hard path"),
            TEXT("quote split git reset hard path"),
            TEXT("git option clean path"),
            TEXT("git option checkout path"),
            TEXT("git apply path"),
            TEXT("git option apply path"),
            TEXT("git split apply path"),
            TEXT("escaped git apply path"),
            TEXT("git shell alias destructive path"),
            TEXT("git inline shell alias destructive path"),
            TEXT("ansi c quoted root remove path"),
            TEXT("ansi c quoted git alias path"),
            TEXT("ansi c escaped git alias path"),
            TEXT("config env git alias path"),
            TEXT("ansi c escaped git executable path"),
            TEXT("ansi c escaped remove executable path"),
            TEXT("git restore path"),
            TEXT("git checkout path"),
            TEXT("nested shell git reset path"),
            TEXT("nested shell git checkout path"),
            TEXT("stdin shell git reset path"),
            TEXT("encoded stdin shell git reset path"),
            TEXT("node child process git reset path"),
            TEXT("encoded stdin content mutation path"),
            TEXT("escaped git reset path"),
            TEXT("python shutil alias unpack archive path"),
            TEXT("python patool alias extract archive path"),
            TEXT("python os alias remove path"),
            TEXT("python reordered alias remove path"),
            TEXT("python parenthesized alias unpack path"),
            TEXT("python padded tarfile extract path"),
            TEXT("wrapped ripgrep preprocessor execution path"),
            TEXT("wrapped fd execution path"),
            TEXT("shell encoded interpreter path")
        };
        return Prompts;
    }
}

#endif
