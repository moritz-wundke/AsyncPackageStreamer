// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AsyncPackageStreamerPrivatePCH.h"

DEFINE_LOG_CATEGORY(LogAsyncPackageStreamer);

class FAsyncPackageStreamer: public IAsyncPackageStreamer
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

    /** Access the streamable manager */
    virtual FStreamableManager& GetStreamableManager() const override;

private:
    /** Streamable manager used internally */
    FStreamableManager* StreamableManager;
};

IMPLEMENT_MODULE( FAsyncPackageStreamer, AsyncPackageStreamer )

FStreamableManager& FAsyncPackageStreamer::GetStreamableManager() const
{
    check(StreamableManager);
    return *StreamableManager;
}

void FAsyncPackageStreamer::StartupModule()
{
    StreamableManager = new FStreamableManager();
}


void FAsyncPackageStreamer::ShutdownModule()
{
    delete StreamableManager;
    StreamableManager = NULL;
}



