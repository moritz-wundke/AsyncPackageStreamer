// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AsyncPackageStreamerPrivatePCH.h"

DEFINE_LOG_CATEGORY(LogAsyncPackageStreamer);

class FAsyncPackageStreamer: public IAsyncPackageStreamer
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
};

IMPLEMENT_MODULE( FAsyncPackageStreamer, AsyncPackageStreamer )

void FAsyncPackageStreamer::StartupModule()
{
}


void FAsyncPackageStreamer::ShutdownModule()
{
}



