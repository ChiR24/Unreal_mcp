#include "Foundation/BridgeHelpers/Security/McpProjectFileWrite.h"

#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"
#include "HAL/FileManager.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
const TCHAR* const LinkOnTheWay = TEXT("a link on the way leads somewhere else");
const TCHAR* const NotOpened = TEXT("it could not be opened for writing");

#if PLATFORM_WINDOWS
// The path Windows opened, every link and junction on the way followed, with forward slashes; empty when unknown.
FString OpenedPath(HANDLE Handle)
{
	TCHAR Buffer[1024];
	const DWORD Length =
		GetFinalPathNameByHandleW(Handle, Buffer, static_cast<DWORD>(UE_ARRAY_COUNT(Buffer)), FILE_NAME_NORMALIZED);
	FString Path = Length > 0 && Length < UE_ARRAY_COUNT(Buffer) ? FString(Buffer) : FString();
	Path.ReplaceInline(TEXT("\\"), TEXT("/"));
	return Path;
}

FString OpenedFolder(const FString& Folder)
{
	const HANDLE Handle = CreateFileW(*Folder, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		return FString();
	}
	const FString Path = OpenedPath(Handle);
	CloseHandle(Handle);
	return Path;
}

// The project folder may itself sit behind a link, so the file is compared with the opened project folder plus the
// part under it, which must have none. ponytail: the folders are made by path, so a link swapped in between can leave
// an empty folder outside; the file is checked as opened and never lands there.
const TCHAR* Write(const FString& Root, const FString& Relative, TArrayView<const uint8> Bytes)
{
	const FString File = Root + Relative;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
	const FString RootOpened = OpenedFolder(Root);
	const HANDLE Handle = CreateFileW(*File, GENERIC_WRITE | DELETE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (Handle == INVALID_HANDLE_VALUE)
	{
		return NotOpened;
	}
	const bool bCreated = GetLastError() != ERROR_ALREADY_EXISTS;
	if (RootOpened.IsEmpty() || !OpenedPath(Handle).Equals(RootOpened / Relative, ESearchCase::IgnoreCase))
	{
		FILE_DISPOSITION_INFO Remove = {1}; // the file this open made out there goes with the handle
		if (bCreated)
		{
			SetFileInformationByHandle(Handle, FileDispositionInfo, &Remove, static_cast<DWORD>(sizeof(Remove)));
		}
		CloseHandle(Handle);
		return LinkOnTheWay;
	}
	DWORD Written = 0;
	const DWORD Size = static_cast<DWORD>(Bytes.Num());
	const bool bWritten = SetEndOfFile(Handle) && WriteFile(Handle, Bytes.GetData(), Size, &Written, nullptr)
		&& Written == Size;
	CloseHandle(Handle);
	return bWritten ? nullptr : TEXT("the write failed");
}
#else
// Each folder is opened (or made) inside the one before it without following a link, so a link put there after the
// resolver's check fails the open instead of carrying the write elsewhere.
const TCHAR* Write(const FString& Root, const FString& Relative, TArrayView<const uint8> Bytes)
{
	TArray<FString> Names;
	Relative.ParseIntoArray(Names, TEXT("/"), true);
	if (Names.Num() == 0)
	{
		return NotOpened;
	}
	int Folder = open(TCHAR_TO_UTF8(*Root), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	int Error = Folder < 0 ? errno : 0;
	for (int32 Index = 0; Folder >= 0 && Index + 1 < Names.Num(); ++Index)
	{
		mkdirat(Folder, TCHAR_TO_UTF8(*Names[Index]), 0775); // there already is fine; a link there fails the open
		const int Inner = openat(Folder, TCHAR_TO_UTF8(*Names[Index]), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		Error = Inner < 0 ? errno : 0;
		close(Folder);
		Folder = Inner;
	}
	int File = -1;
	if (Folder >= 0)
	{
		File = openat(Folder, TCHAR_TO_UTF8(*Names.Last()), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0664);
		Error = File < 0 ? errno : 0;
		close(Folder);
	}
	if (File < 0)
	{
		return Error == ELOOP || Error == ENOTDIR ? LinkOnTheWay : NotOpened;
	}
	const uint8* Next = Bytes.GetData();
	int64 Left = Bytes.Num();
	while (Left > 0)
	{
		const ssize_t Wrote = write(File, Next, Left);
		if (Wrote <= 0)
		{
			break;
		}
		Next += Wrote;
		Left -= Wrote;
	}
	close(File);
	return Left == 0 ? nullptr : TEXT("the write failed");
}
#endif
} // namespace

bool McpWriteProjectFile(const FString& AbsolutePath, TArrayView<const uint8> Bytes, FString& OutError)
{
	FString File = FPaths::ConvertRelativePathToFull(AbsolutePath);
	FPaths::NormalizeFilename(File);
	const FString Root = McpProjectRootDir();
	// The part under the project, as the opened file's path spells it (ProjectDir() / "/Saved/x" doubles a slash).
	FString Relative = File.StartsWith(Root, McpFileNameCase) ? File.RightChop(Root.Len()) : FString();
	while (Relative.ReplaceInline(TEXT("//"), TEXT("/")) > 0)
	{
	}
	Relative.RemoveFromStart(TEXT("/"));
	const TCHAR* Why = Relative.IsEmpty() ? TEXT("it is not a file inside the project") : Write(Root, Relative, Bytes);
	if (Why)
	{
		OutError = FString::Printf(TEXT("'%s' was not written: %s."), *AbsolutePath, Why);
	}
	return Why == nullptr;
}

bool McpWriteProjectText(const FString& AbsolutePath, const FString& Text, FString& OutError)
{
	const FTCHARToUTF8 Utf8(*Text, Text.Len());
	return McpWriteProjectFile(AbsolutePath,
		TArrayView<const uint8>(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length()), OutError);
}
