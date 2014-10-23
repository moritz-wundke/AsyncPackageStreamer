// Copyright (c) 2014 Moritz Wundke & Ruben Avalos Elvira

#include "AsyncPackageStreamerPrivatePCH.h"
#include "IPlatformFilePak.h"
#include "StreamingNetworkPlatformFile.h"
#include "FileManagerGeneric.h"

FAssetStreamer::FAssetStreamer()
    : NetPlatform(new FStreamingNetworkPlatformFile())
    , PakPlatform(new FPakPlatformFile())
{
    
}

bool FAssetStreamer::Initialize(const FString& ServerHost)
{
    if (!bInitialized)
    {
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

    // We must now if the files we are about to stream are signed or not, it MUST be in sync with the platform file
    bool bSigned = false;
    // TODO: Address signed PAKs, Add -Signed to commandline?
#if 0
#if !USING_SIGNED_CONTENT
    bSigned = FParse::Param(CmdLine, TEXT("Signedpak")) || FParse::Param(CmdLine, TEXT("Signed"));
    if (!bSigned)
    {
        // Even if -signed is not provided in the command line, use signed reader if the hardcoded key is non-zero.
        FEncryptionKey DecryptionKey;
        DecryptionKey.Exponent.Parse(DECRYPTION_KEY_EXPONENT);
        DecryptionKey.Modulus.Parse(DECYRPTION_KEY_MODULUS);
        bSigned = !DecryptionKey.Exponent.IsZero() && !DecryptionKey.Modulus.IsZero();
    }
#else
    bSigned = true;
#endif
#endif

    // Now Get the path and start the streaming
    const FString FilePath = bRemote ? ResolveRemotePath(PakFileName) : ResolveLocalPath(PakFileName);

    // Make sure the Pak file is actually there
    FPakFile PakFile(*FilePath, bSigned);
    if (!PakFile.IsValid())
    {
        Unlock();
        return false;
    }

    // TODO: Do we need to mount it into the engine dir? Creare a DLC dir instead?
    PakFile.SetMountPoint(*FPaths::EngineContentDir());
    const int32 PakOrder = 0;
    if (!PakPlatform->Mount(*FilePath, PakOrder, *FPaths::EngineContentDir()))
    {
        Unlock();
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

    //// Tell the listsner which assets we are about to stream
    if (Listener)
    {
        Listener->OnPrepareAssetStreaming(StreamedAssets);
    }

    // TODO: Where should we get the FStreamableManager from?
    FStreamableManager& Streamable = FModuleManager::LoadModuleChecked<IAsyncPackageStreamer>("AsyncPackageStreamer").GetStreamableManager();
    Streamable.RequestAsyncLoad(StreamedAssets, FStreamableDelegate::CreateRaw(this, &FAssetStreamer::OnStreamingCompleteDelegate));

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