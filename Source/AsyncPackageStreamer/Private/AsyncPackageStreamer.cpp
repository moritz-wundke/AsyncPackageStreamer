// Copyright (c) 2014 Moritz Wundke & Ruben Avalos Elvira

#include "AsyncPackageStreamerPrivatePCH.h"

DEFINE_LOG_CATEGORY(LogAsyncPackageStreamer);

class FAsyncPackageStreamer: public IAsyncPackageStreamer
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

    /** Access the streamer */
    virtual FAssetStreamer& GetStreamer() const override;

    /** Access the streamable manager */
    virtual FStreamableManager& GetStreamableManager() const override;

private:
    FAssetStreamer* AssetStreamer;

    FStreamableManager* StreamableManager;
};

IMPLEMENT_MODULE( FAsyncPackageStreamer, AsyncPackageStreamer )

FAssetStreamer& FAsyncPackageStreamer::GetStreamer() const
{
    check(AssetStreamer);
    return *AssetStreamer;
}

FStreamableManager& FAsyncPackageStreamer::GetStreamableManager() const
{
    check(StreamableManager);
    return *StreamableManager;
}

void FAsyncPackageStreamer::StartupModule()
{
    AssetStreamer = new FAssetStreamer();
    StreamableManager = new FStreamableManager();
}


void FAsyncPackageStreamer::ShutdownModule()
{
    delete AssetStreamer;
    AssetStreamer = NULL;

    delete StreamableManager;
    StreamableManager = NULL;
}



