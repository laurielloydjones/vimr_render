#pragma once

#include "CoreMinimal.h"
#include "OnlineAsyncTaskManager.h"

/**
 *	Register subsystem callbacks with the engine
 */
class FOnlineAsyncTaskManagerOtago : public FOnlineAsyncTaskManager
{
protected:

	/** Cached reference to the main online subsystem */
	class FOnlineSubsystemOtago* OtagoSubsystem;

public:

	FOnlineAsyncTaskManagerOtago(class FOnlineSubsystemOtago* InOnlineSubsystem)
		: OtagoSubsystem(InOnlineSubsystem)
	{
	}

	~FOnlineAsyncTaskManagerOtago()
	{
	}

	// FOnlineAsyncTaskManager
	virtual void OnlineTick() override;
};
