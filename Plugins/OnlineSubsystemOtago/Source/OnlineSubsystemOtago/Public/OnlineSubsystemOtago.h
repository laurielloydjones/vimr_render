#pragma once

#include "CoreMinimal.h"
#include "ModuleManager.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemOtagoPackage.h"
#include "HAL/ThreadSafeCounter.h"

/** sed for Logging Online Subsystem Otago messages */
DECLARE_LOG_CATEGORY_EXTERN(LogOSSO, Log, All);

/**
* Online Subsystem Otago module implements  the OnlinIModuleInterface class. 
* This is used to initialize a module after it's been loaded, and also to clean
* it up before the module is unloaded. All modules classes should derive from this.
*/
class FOnlineSubsystemOtagoModule : public IModuleInterface
{
public:

	FOnlineSubsystemOtagoModule() :
		OtagoFactory(NULL)
	{}

	// IModuleInterface
	virtual ~FOnlineSubsystemOtagoModule() {}
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}
	virtual bool SupportsAutomaticShutdown() override
	{
		return false;
	}
	//===============================================

private:
	class FOnlineFactoryOtago* OtagoFactory;
};

class FOnlineAchievementsOtago;
class FOnlineIdentityOtago;
class FOnlineLeaderboardsOtago;
class FOnlineSessionOtago;
class FOnlineVoiceImpl;

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineSessionOtago, ESPMode::ThreadSafe> FOnlineSessionOtagoPtr;
typedef TSharedPtr<class FOnlineProfileOtago, ESPMode::ThreadSafe> FOnlineProfileOtagoPtr;
typedef TSharedPtr<class FOnlineFriendsOtago, ESPMode::ThreadSafe> FOnlineFriendsOtagoPtr;
typedef TSharedPtr<class FOnlineUserCloudOtago, ESPMode::ThreadSafe> FOnlineUserCloudOtagoPtr;
typedef TSharedPtr<class FOnlineLeaderboardsOtago, ESPMode::ThreadSafe> FOnlineLeaderboardsOtagoPtr;
typedef TSharedPtr<class FOnlineVoiceImpl, ESPMode::ThreadSafe> FOnlineVoiceImplPtr;
typedef TSharedPtr<class FOnlineExternalUIOtago, ESPMode::ThreadSafe> FOnlineExternalUIOtagoPtr;
typedef TSharedPtr<class FOnlineIdentityOtago, ESPMode::ThreadSafe> FOnlineIdentityOtagoPtr;
typedef TSharedPtr<class FOnlineAchievementsOtago, ESPMode::ThreadSafe> FOnlineAchievementsOtagoPtr;

/**
 *	OnlineSubsystemOtago - Implementation of the online subsystem for Otago services
 */
class ONLINESUBSYSTEMOTAGO_API FOnlineSubsystemOtago :
	public FOnlineSubsystemImpl
{
public:

	virtual ~FOnlineSubsystemOtago() {}

	// IOnlineSubsystem

	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IOnlinePartyPtr GetPartyInterface() const override;
	virtual IOnlineGroupsPtr GetGroupsInterface() const override;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
	virtual IOnlineVoicePtr GetVoiceInterface() const override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;
	virtual IOnlineTimePtr GetTimeInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
	virtual IOnlineStorePtr GetStoreInterface() const override;
	virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override { return nullptr; }
	virtual IOnlinePurchasePtr GetPurchaseInterface() const override { return nullptr; }
	virtual IOnlineEventsPtr GetEventsInterface() const override;
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
	virtual IOnlineSharingPtr GetSharingInterface() const override;
	virtual IOnlineUserPtr GetUserInterface() const override;
	virtual IOnlineMessagePtr GetMessageInterface() const override;
	virtual IOnlinePresencePtr GetPresenceInterface() const override;
	virtual IOnlineChatPtr GetChatInterface() const override;
	virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override;

	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual FText GetOnlineServiceName() const override;

	// FTickerObjectBase

	virtual bool Tick(float DeltaTime) override;

	// FOnlineSubsystemOtago

	/**
	 * Is the Otago API available for use
	 * @return true if Otago functionality is available, false otherwise
	 */
	bool IsEnabled() const;

	FString const& GetSessionServer() const
	{
		return SessionServer;
	}
	FString const& GetProjectName() const
	{
		return ProjectName;
	}

	FString  GetOnlineSessionID()
	{
		return OnlineSessionID;
	}
	void  SetOnlineSessionID(FString nSessionID)
	{
		OnlineSessionID = nSessionID;
	}


PACKAGE_SCOPE:

	/** Only the factory makes instances */
	FOnlineSubsystemOtago(FName InInstanceName) :
		FOnlineSubsystemImpl(NULL_SUBSYSTEM, InInstanceName),
		SessionInterface(nullptr),
		VoiceInterface(nullptr),
		bVoiceInterfaceInitialized(false),
		LeaderboardsInterface(nullptr),
		IdentityInterface(nullptr),
		AchievementsInterface(nullptr),
		OnlineAsyncTaskThreadRunnable(nullptr),
		OnlineAsyncTaskThread(nullptr)
	{}

	FOnlineSubsystemOtago() :
		SessionInterface(nullptr),
		VoiceInterface(nullptr),
		bVoiceInterfaceInitialized(false),
		LeaderboardsInterface(nullptr),
		IdentityInterface(nullptr),
		AchievementsInterface(nullptr),
		OnlineAsyncTaskThreadRunnable(nullptr),
		OnlineAsyncTaskThread(nullptr)
	{}

private:

	/** Interface to the session services */
	FOnlineSessionOtagoPtr SessionInterface;

	/** Interface for voice communication */
	mutable FOnlineVoiceImplPtr VoiceInterface;

	/** Interface for voice communication */
	mutable bool bVoiceInterfaceInitialized;

	/** Interface to the leaderboard services */
	FOnlineLeaderboardsOtagoPtr LeaderboardsInterface;

	/** Interface to the identity registration/auth services */
	FOnlineIdentityOtagoPtr IdentityInterface;

	/** Interface for achievements */
	FOnlineAchievementsOtagoPtr AchievementsInterface;

	/** Online async task runnable */
	class FOnlineAsyncTaskManagerOtago* OnlineAsyncTaskThreadRunnable;

	/** Online async task thread */
	class FRunnableThread* OnlineAsyncTaskThread;

	// task counter, used to generate unique thread names for each task
	static FThreadSafeCounter TaskCounter;

	FString SessionServer;
	FString ProjectName;

	FString OnlineSessionID;
};

typedef TSharedPtr<FOnlineSubsystemOtago, ESPMode::ThreadSafe> FOnlineSubsystemOtagoPtr;