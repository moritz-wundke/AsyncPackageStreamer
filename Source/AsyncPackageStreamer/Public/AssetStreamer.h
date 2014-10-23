// Copyright (c) 2014 Moritz Wundke & Ruben Avalos Elvira

#pragma once

class IPlatformFile;
class FStreamingNetworkPlatformFile;
struct FStringAssetReference;
class FPakPlatformFile;

namespace EAssetStreamingMode
{
    enum Type
    {
        Local,
        Remote
    };
}

/**
* Interface
*/
class ASYNCPACKAGESTREAMER_API IAssetStreamerListener
{
public:
    /** Must have the same signature as FStreamableDelegate. Called when the streaming has completed. */
    virtual void OnAssetStreamComplete() = 0;

    /** Called before the streaming is initiated, used to inform which assets will be requested */
    virtual void OnPrepareAssetStreaming(const TArray<FStringAssetReference>& StreamedAssets) = 0;
};

/**
 * The asset streamer is capable of loading and mounting the content of a PAK file from the local FS
 * or a remote one.
 */
class ASYNCPACKAGESTREAMER_API FAssetStreamer
{
public:
    FAssetStreamer();

    /** Unlock the streamer when destroyed */
    ~FAssetStreamer()
    {
        Unlock();
    }

    /** Initialize the FAssetStreamer and point to the using host:port */
    bool Initialize(const FString& ServerHost);

    /**
     * Stream assets form a PAK file remotely or from the local file system
     */
    bool StreamPackage(const FString& PakFileName, IAssetStreamerListener* AssetStreamerListener, EAssetStreamingMode::Type DesiredMode, const TCHAR* CmdLine);

    /** Check if the streamed has been initialized or not */
    uint32 bInitialized : 1;

    /** The mode that is currently set for streaming */
    EAssetStreamingMode::Type CurrentMode;

    bool IsStreaming() const
    {
        return LockValue != 0;
    }

    /** Blocks this thread until we have stopped streaming */
    void BlockUntilStreamingFinished(float SleepTime = 0.1f) const
    {
        while (LockValue != 0)
        {
            FPlatformProcess::Sleep(SleepTime);
        }
    }

private:

    /** Set the  streaming to be using the remote FS*/
    bool UseRemote(const TCHAR* CmdLine);

    /** Set the  streaming to be using the local FS*/
    bool UseLocal(const TCHAR* CmdLine);

    /** Resolve a remote path to a given hosted PAK file */
    FString ResolveRemotePath(const FString& FileName);

    /** Resolves a local path to a given PAK file */
    FString ResolveLocalPath(const FString& FileName);

    /** Locks the primitive so no one can proceed past a block point */
    void Lock()
    {
        FPlatformAtomics::InterlockedExchange(&LockValue, 1);
    }

    /** Unlocks the primitive so other threads can proceed past a block point */
    void Unlock()
    {
        FPlatformAtomics::InterlockedExchange(&LockValue, 0);
    }

    /** Called once streaming has finished, will call our listener and unlock the streamer again */
    void OnStreamingCompleteDelegate();

    /** Thread safe lock value */
    volatile int32 LockValue;

    // The platform file instances that will help us perform local and remote loading
    IPlatformFile* LocalPlatformFile;
    TSharedPtr<FStreamingNetworkPlatformFile> NetPlatform;
    TSharedPtr<FPakPlatformFile> PakPlatform;

    /** The current listener for the current streaming task */
    IAssetStreamerListener* Listener;

    /** The assets that we are goind to stream */
    TArray<FStringAssetReference> StreamedAssets;

};