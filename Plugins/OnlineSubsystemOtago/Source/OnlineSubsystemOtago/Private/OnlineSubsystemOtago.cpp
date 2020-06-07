#include "OnlineSubsystemOtago.h"
#include "OnlineSessionInterfaceOtago.h"
#include "OnlineAsyncTaskManagerOtago.h"
#include "HAL/RunnableThread.h"
#include "VoiceInterfaceImpl.h"
#include "ConfigCacheIni.h"

#include "OnlineIdentityOtago.h"


#define LOCTEXT_NAMESPACE "FOnlineSubsystemOtagoModule"
#define OTAGO_SUBSYSTEM FName(TEXT("OTAGO"))

DEFINE_LOG_CATEGORY(LogOSSO);

class FOnlineFactoryOtago : public IOnlineFactory
{
public:

	FOnlineFactoryOtago() {}
	virtual ~FOnlineFactoryOtago() {}

	virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName)
	{
		FOnlineSubsystemOtagoPtr OnlineSub = MakeShareable(new FOnlineSubsystemOtago(InstanceName));
		if (OnlineSub->IsEnabled())
		{
			if(!OnlineSub->Init())
			{
				UE_LOG_ONLINE(Warning, TEXT("Otago Network API failed to initialize!"));
				OnlineSub->Shutdown();
				OnlineSub = NULL;
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Otago Network API disabled!"));
			OnlineSub->Shutdown();
			OnlineSub = NULL;
		}

		return OnlineSub;
	}
};

void FOnlineSubsystemOtagoModule::StartupModule()
{
	OtagoFactory = new FOnlineFactoryOtago();

	// Create and register our singleton factory with the main online subsystem for easy access
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.RegisterPlatformService(OTAGO_SUBSYSTEM, OtagoFactory);
}

void FOnlineSubsystemOtagoModule::ShutdownModule()
{
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.UnregisterPlatformService(OTAGO_SUBSYSTEM);

	delete OtagoFactory;
	OtagoFactory = NULL;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FOnlineSubsystemOtagoModule, OnlineSubsystemOtago)


FThreadSafeCounter FOnlineSubsystemOtago::TaskCounter;

IOnlineSessionPtr FOnlineSubsystemOtago::GetSessionInterface() const
{
	return SessionInterface;
}

IOnlineFriendsPtr FOnlineSubsystemOtago::GetFriendsInterface() const
{
	return nullptr;
}

IOnlinePartyPtr FOnlineSubsystemOtago::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemOtago::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemOtago::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemOtago::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemOtago::GetEntitlementsInterface() const
{
	return nullptr;
};

IOnlineLeaderboardsPtr FOnlineSubsystemOtago::GetLeaderboardsInterface() const
{
	// TODO?: return LeaderboardsInterface;
	return nullptr;
}

IOnlineVoicePtr FOnlineSubsystemOtago::GetVoiceInterface() const
{
	if (VoiceInterface.IsValid() && !bVoiceInterfaceInitialized)
	{
		if (!VoiceInterface->Init())
		{
			VoiceInterface = nullptr;
		}

		bVoiceInterfaceInitialized = true;
	}

	return VoiceInterface;
}

IOnlineExternalUIPtr FOnlineSubsystemOtago::GetExternalUIInterface() const
{
	return nullptr;
}

IOnlineTimePtr FOnlineSubsystemOtago::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemOtago::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineTitleFilePtr FOnlineSubsystemOtago::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineStorePtr FOnlineSubsystemOtago::GetStoreInterface() const
{
	return nullptr;
}

IOnlineEventsPtr FOnlineSubsystemOtago::GetEventsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemOtago::GetAchievementsInterface() const
{
	// TODO?: return AchievementsInterface;
	return nullptr;
}

IOnlineSharingPtr FOnlineSubsystemOtago::GetSharingInterface() const
{
	return nullptr;
}

IOnlineUserPtr FOnlineSubsystemOtago::GetUserInterface() const
{
	return nullptr;
}

IOnlineMessagePtr FOnlineSubsystemOtago::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemOtago::GetPresenceInterface() const
{
	return nullptr;
}

IOnlineChatPtr FOnlineSubsystemOtago::GetChatInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemOtago::GetTurnBasedInterface() const
{
    return nullptr;
}

bool FOnlineSubsystemOtago::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	if (OnlineAsyncTaskThreadRunnable)
	{
		OnlineAsyncTaskThreadRunnable->GameTick();
	}

 	if (SessionInterface.IsValid())
 	{
 		SessionInterface->Tick(DeltaTime);
 	}

	if (VoiceInterface.IsValid() && bVoiceInterfaceInitialized)
	{
		VoiceInterface->Tick(DeltaTime);
	}

	return true;
}

bool FOnlineSubsystemOtago::Init()
{
	// Create the online async task thread
	OnlineAsyncTaskThreadRunnable = new FOnlineAsyncTaskManagerOtago(this);
	check(OnlineAsyncTaskThreadRunnable);
	OnlineAsyncTaskThread = FRunnableThread::Create(OnlineAsyncTaskThreadRunnable, *FString::Printf(TEXT("OnlineAsyncTaskThreadOtago %s(%d)"), *InstanceName.ToString(), TaskCounter.Increment()), 128 * 1024, TPri_Normal);
	check(OnlineAsyncTaskThread);
	UE_LOG_ONLINE(Verbose, TEXT("Created thread (ID:%d)."), OnlineAsyncTaskThread->GetThreadID());


	SessionInterface = MakeShareable(new FOnlineSessionOtago(this));
	IdentityInterface = MakeShareable(new FOnlineIdentityOtago(this));
	VoiceInterface = MakeShareable(new FOnlineVoiceImpl(this));

	GConfig->GetString(TEXT("OnlineSubsystemOtago"), TEXT("SessionServer"), SessionServer, GGameIni);
	GConfig->GetString(TEXT("OnlineSubsystemOtago"), TEXT("ProjectName"), ProjectName, GGameIni);
	return true;
}

bool FOnlineSubsystemOtago::Shutdown()
{
	UE_LOG_ONLINE(Display, TEXT("FOnlineSubsystemOtago::Shutdown()"));

	FOnlineSubsystemImpl::Shutdown();

	if (OnlineAsyncTaskThread)
	{
		// Destroy the online async task thread
		delete OnlineAsyncTaskThread;
		OnlineAsyncTaskThread = nullptr;
	}

	if (OnlineAsyncTaskThreadRunnable)
	{
		delete OnlineAsyncTaskThreadRunnable;
		OnlineAsyncTaskThreadRunnable = nullptr;
	}

	if (VoiceInterface.IsValid() && bVoiceInterfaceInitialized)
	{
		VoiceInterface->Shutdown();
	}

#define DESTRUCT_INTERFACE(Interface) \
 	if (Interface.IsValid()) \
 	{ \
 		ensure(Interface.IsUnique()); \
 		Interface = nullptr; \
 	}

 	// Destruct the interfaces
	DESTRUCT_INTERFACE(VoiceInterface);
	DESTRUCT_INTERFACE(AchievementsInterface);
	DESTRUCT_INTERFACE(IdentityInterface);
	DESTRUCT_INTERFACE(LeaderboardsInterface);
 	DESTRUCT_INTERFACE(SessionInterface);

#undef DESTRUCT_INTERFACE

	return true;
}

FString FOnlineSubsystemOtago::GetAppId() const
{
	return TEXT("");
}

bool FOnlineSubsystemOtago::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar);
}

FText FOnlineSubsystemOtago::GetOnlineServiceName() const
{
	return NSLOCTEXT("OnlineSubsystemOtago", "OnlineServiceName", "Otago");
}

bool FOnlineSubsystemOtago::IsEnabled() const
{
	return true;
}


