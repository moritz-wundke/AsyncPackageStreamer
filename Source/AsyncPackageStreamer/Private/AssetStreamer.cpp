// Copyright (c) 2014 Moritz Wundke & Ruben Avalos Elvira

#include "AsyncPackageStreamerPrivatePCH.h"
#include "IPlatformFilePak.h"
#include "StreamingNetworkPlatformFile.h"
#include "FileManagerGeneric.h"
#include "AssetStreamer.h"

FAssetStreamer::FAssetStreamer()
    : NetPlatform(new FStreamingNetworkPlatformFile())
    , PakPlatform(new FPakPlatformFile())
{
    
}

bool FAssetStreamer::Initialize(FStreamableManager* StreamableManager)
{
    check(StreamableManager);
    if (!bInitialized)
    {
        // Load config values
        if (!GConfig->GetString(TEXT("AssetStreamer"), TEXT("ServerHost"), ServerHost, GEngineIni))
        {
            ServerHost = TEXT("127.0.0.1:8081");
        }
        if (!GConfig->GetBool(TEXT("AssetStreamer"), TEXT("bSigned"), bSigned, GEngineIni))
        {
            bSigned = false;
        }        

        LocalPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();
        if (LocalPlatformFile != nullptr)
        {
            IPlatformFile* PakPlatformFile = FPlatformFileManager::Get().GetPlatformFile(TEXT("PakFile"));
            IPlatformFile* NetPlatformFile = FPlatformFileManager::Get().GetPlatformFile(TEXT("NetworkFile"));
            if (PakPlatformFile != nullptr && NetPlatformFile != nullptr)
            {
                if (NetPlatform.IsValid())
                {
                    bInitialized = NetPlatform->Initialize(LocalPlatformFile, *FString::Printf(TEXT("-FileHostIP=%s"), *ServerHost));
                }
            }
        }
    }

    if (!bInitialized)
    {
        UE_LOG(LogAsyncPackageStreamer, Error, TEXT("Failed to initialize AssetStreamer using %s for remote file streaming"), *ServerHost);
    }
    else
    {
        // We aquire the StreamableManager pointer only on successfull initialization
        this->StreamableManager = StreamableManager;
    }

    return bInitialized;
}

bool FAssetStreamer::StreamPackage(const FString& PakFileName, IAssetStreamerListener* AssetStreamerListener, EAssetStreamingMode::Type DesiredMode, const TCHAR* CmdLine)
{
    Lock();
    Listener = NULL;

    const bool bRemote = (DesiredMode == EAssetStreamingMode::Remote);
    if (!(bRemote && UseRemote(CmdLine) || !bRemote && UseLocal(CmdLine)))
    {
        Unlock();
        return false;
    }
    CurrentMode = DesiredMode;
    FPlatformFileManager::Get().SetPlatformFile(*PakPlatform);

    // Now Get the path and start the streaming
    const FString FilePath = bRemote ? ResolveRemotePath(PakFileName) : ResolveLocalPath(PakFileName);

    // Make sure the Pak file is actually there
    FPakFile PakFile(*FilePath, bSigned);
    if (!PakFile.IsValid())
    {
        Unlock();
        UE_LOG(LogAsyncPackageStreamer, Error, TEXT("Invalid pak file: %s"), *FilePath);
        return false;
    }

    // TODO: Do we need to mount it into the engine dir? Creare a DLC dir instead?
    PakFile.SetMountPoint(*FPaths::EngineContentDir());
    const int32 PakOrder = 0;
    if (!PakPlatform->Mount(*FilePath, PakOrder, *FPaths::EngineContentDir()))
    {
        Unlock();
        UE_LOG(LogAsyncPackageStreamer, Error, TEXT("Failed to mount pak file: %s"), *FilePath);
        return false;
    }

    // Load all assets contained in this Pak file
    TSet<FString> FileList;
    PakFile.FindFilesAtPath(FileList, *PakFile.GetMountPoint(), true, false, true);

    // Discover assets within the PakFile
    StreamedAssets.Empty();
    for (TSet<FString>::TConstIterator FileItem(FileList); FileItem; ++FileItem)
    {
        FString AssetName = *FileItem;
        if (AssetName.EndsWith(FPackageName::GetAssetPackageExtension()) ||
            AssetName.EndsWith(FPackageName::GetMapPackageExtension()))
        {
            // TODO: Set path relative to mountpoint for remote streaming?
            StreamedAssets.Add(AssetName);
        }
    }

    // Once we started the async work assign listener
    Listener = AssetStreamerListener;

    // Tell the listener which assets we are about to stream
    if (Listener)
    {
        Listener->OnPrepareAssetStreaming(StreamedAssets);
    }

    // IF we have not yet a StreamableManager setup (Arrr...) abort
    if (StreamableManager == nullptr)
    {
        Unlock();
        UE_LOG(LogAsyncPackageStreamer, Error, TEXT("No StreamableManager registered, did you missed to call initialize?"));
        return false;
    }

    StreamableManager->RequestAsyncLoad(StreamedAssets, FStreamableDelegate::CreateRaw(this, &FAssetStreamer::OnStreamingCompleteDelegate));
    return true;
}


bool FAssetStreamer::UseRemote(const TCHAR* CmdLine)
{
    if (bInitialized && PakPlatform.IsValid())
    {
        IPlatformFile* InnerPlatform = NetPlatform.Get();
        return InnerPlatform && PakPlatform->Initialize(InnerPlatform, CmdLine);
    }
    UE_LOG(LogAsyncPackageStreamer, Error, TEXT("Failed to initialize remote file system! ou wont be able to load PAK files!"));
    return false;
}


bool FAssetStreamer::UseLocal(const TCHAR* CmdLine)
{
    if (bInitialized && PakPlatform.IsValid())
    {
        IPlatformFile* InnerPlatform = LocalPlatformFile;
        return InnerPlatform && PakPlatform->Initialize(InnerPlatform, CmdLine);
    }
    UE_LOG(LogAsyncPackageStreamer, Error, TEXT("Failed to initialize local file system! ou wont be able to load PAK files!"));
    return false;
}

/** Resolve a remote path to a given hosted PAK file */
FString FAssetStreamer::ResolveRemotePath(const FString& FileName)
{
    // TODO: We need to setup where the remote host will host the files based on the root of the file server!
    return FileName + TEXT(".pak");
}

/** Resolves a local path to a given PAK file */
FString FAssetStreamer::ResolveLocalPath(const FString& FileName)
{
    return FileName + TEXT(".pak");
}

void FAssetStreamer::OnStreamingCompleteDelegate()
{
    // Call listener!
    if (Listener != NULL)
    {
        Listener->OnAssetStreamComplete();
        Listener = NULL;
    }

    // Finally unlock the streamer
    Unlock();
}