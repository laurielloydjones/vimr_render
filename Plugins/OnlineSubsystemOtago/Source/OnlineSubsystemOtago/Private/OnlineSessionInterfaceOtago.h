#pragma once

#include "CoreMinimal.h"
#include "UObject/CoreOnline.h"
#include "Misc/ScopeLock.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSubsystemOtagoPackage.h"
#include "OnlineSubsystemOtagoTypes.h"
#include "LANBeacon.h"
#include "Http.h"
#include "atomic"

class FOnlineSubsystemOtago;

class FOnlineSessionOtago : public IOnlineSession
{
public:

	virtual ~FOnlineSessionOtago() {}

	void LogSession(FOnlineSession* Session)
	{
		UE_LOG(LogOSSO, Error, TEXT("Session Details"));
		UE_LOG(LogOSSO, Warning, TEXT("OwningUserId: %s"), *Session->OwningUserId->ToString());
		UE_LOG(LogOSSO, Warning, TEXT("OwningUserName: %s"), *Session->OwningUserName);
		UE_LOG(LogOSSO, Warning, TEXT("NumOpenPrivateConnections: %d"), Session->NumOpenPrivateConnections);
		UE_LOG(LogOSSO, Warning, TEXT("NumOpenPublicConnections: %d"), Session->NumOpenPublicConnections);

		UE_LOG(LogOSSO, Error, TEXT("Session Info"));
		UE_LOG(LogOSSO, Warning, TEXT("SessionId: %s"), *Session->SessionInfo->GetSessionId().ToString());
		UE_LOG(LogOSSO, Warning, TEXT("DebugString: %s"), *Session->SessionInfo->ToDebugString());
		FOnlineSessionInfoOtago* OtagoSessionInfo = (FOnlineSessionInfoOtago*)(Session->SessionInfo.Get());
		UE_LOG(LogOSSO, Warning, TEXT("Internal SessionId: %s"), *OtagoSessionInfo->SessionId.ToString());
		UE_LOG(LogOSSO, Warning, TEXT("HostAddr: %s"), *OtagoSessionInfo->HostAddr->ToString(true));

		UE_LOG(LogOSSO, Error, TEXT("Session Settings"));
		FOnlineSessionSettings& Settings = Session->SessionSettings;
		UE_LOG(LogOSSO, Warning, TEXT("bAllowInvites: %d"), Settings.bAllowInvites);
		UE_LOG(LogOSSO, Warning, TEXT("bAllowJoinInProgress: %d"), Settings.bAllowJoinInProgress);
		UE_LOG(LogOSSO, Warning, TEXT("bAllowJoinViaPresence: %d"), Settings.bAllowJoinViaPresence);
		UE_LOG(LogOSSO, Warning, TEXT("bAllowJoinViaPresenceFriendsOnly: %d"), Settings.bAllowJoinViaPresenceFriendsOnly);
		UE_LOG(LogOSSO, Warning, TEXT("bAntiCheatProtected: %d"), Settings.bAntiCheatProtected);
		UE_LOG(LogOSSO, Warning, TEXT("bIsDedicated: %d"), Settings.bIsDedicated);
		UE_LOG(LogOSSO, Warning, TEXT("bIsLANMatch: %d"), Settings.bIsLANMatch);
		UE_LOG(LogOSSO, Warning, TEXT("bShouldAdvertise: %d"), Settings.bShouldAdvertise);
		UE_LOG(LogOSSO, Warning, TEXT("bUsesPresence: %d"), Settings.bUsesPresence);
		UE_LOG(LogOSSO, Warning, TEXT("bUsesStats: %d"), Settings.bUsesStats);
		UE_LOG(LogOSSO, Warning, TEXT("NumPrivateConnections: %d"), Settings.NumPrivateConnections);
		UE_LOG(LogOSSO, Warning, TEXT("NumPublicConnections: %d"), Settings.NumPublicConnections);
		UE_LOG(LogOSSO, Warning, TEXT("BuildUniqueId: %d"), Settings.BuildUniqueId);
	}

	FNamedOnlineSession* GetNamedSession(FName SessionName) override
	{
		FScopeLock ScopeLock(&SessionLock);
		for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			if (Sessions[SearchIndex].SessionName == SessionName)
			{
				return &Sessions[SearchIndex];
			}
		}
		return nullptr;
	}

	virtual void RemoveNamedSession(FName SessionName) override
	{
		FScopeLock ScopeLock(&SessionLock);
		for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			if (Sessions[SearchIndex].SessionName == SessionName)
			{
				Sessions.RemoveAtSwap(SearchIndex);
				return;
			}
		}
	}

	virtual EOnlineSessionState::Type GetSessionState(FName SessionName) const override
	{
		FScopeLock ScopeLock(&SessionLock);
		for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			if (Sessions[SearchIndex].SessionName == SessionName)
			{
				return Sessions[SearchIndex].SessionState;
			}
		}

		return EOnlineSessionState::NoSession;
	}

	virtual bool HasPresenceSession() override
	{
		FScopeLock ScopeLock(&SessionLock);
		for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
		{
			if (Sessions[SearchIndex].SessionSettings.bUsesPresence)
			{
				return true;
			}
		}

		return false;
	}

	// IOnlineSession
	virtual bool CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool StartSession(FName SessionName) override;
	virtual bool UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData = true) override;
	virtual bool EndSession(FName SessionName) override;
	virtual bool DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate = FOnDestroySessionCompleteDelegate()) override;
	virtual bool IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId) override;
	virtual bool StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName) override;
	virtual bool CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName) override;
	virtual bool FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate) override;
	virtual bool CancelFindSessions() override;
	virtual bool PingSearchResults(const FOnlineSessionSearchResult& SearchResult) override;
	virtual bool JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList) override;
	virtual bool SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends) override;
	virtual bool SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends) override;
	virtual bool GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType) override;
	virtual bool GetResolvedConnectString(const FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo) override;
	virtual FOnlineSessionSettings* GetSessionSettings(FName SessionName) override;
	virtual bool RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited) override;
	virtual bool RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited = false) override;
	virtual bool UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId) override;
	virtual bool UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players) override;
	virtual void RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual void UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual int32 GetNumSessions() override;
	virtual void DumpSessionState() override;
	//virtual void TriggerOnFindSessionsCompleteDelegates(bool Param1) override;

PACKAGE_SCOPE:
	void PostSession();
	void OnPostSessionResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	FString getDataIDFromRequest(FHttpRequestPtr Request);
	FString getSessionIDFromRequest(FHttpRequestPtr Request);
	FString SessionType;
	FString mlocalIP;
	FString SessionDescription;

    bool HeartbeatWasSuccessful = false;
    bool runningHeartbeat = false;

	/** Critical sections for thread safe operation of session lists */
	mutable FCriticalSection SessionLock;

	/** Current session settings */
	TArray<FNamedOnlineSession> Sessions;

	/** Current search object */
	TSharedPtr<FOnlineSessionSearch> CurrentSessionSearch;

	/** Current search start time. */
	double SessionSearchStartInSeconds;

	FOnlineSessionOtago(class FOnlineSubsystemOtago* InSubsystem) :
		OtagoSubsystem(InSubsystem),
		CurrentSessionSearch(nullptr),
		SessionSearchStartInSeconds(0)
	{}

	/**
	 * Session tick for various background tasks
	 */
	void Tick(float DeltaTime);

	// IOnlineSession
	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings) override
	{
		FScopeLock ScopeLock(&SessionLock);
		return new (Sessions) FNamedOnlineSession(SessionName, SessionSettings);
	}

	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSession& Session) override
	{
		FScopeLock ScopeLock(&SessionLock);
		return new (Sessions) FNamedOnlineSession(SessionName, Session);
	}

	/**
	 * Parse the command line for invite/join information at launch
	 */
	void CheckPendingSessionInvite();

	/**
	* Registers and updates voice data for the given player id
	*
	* @param PlayerId player to register with the voice subsystem
	*/
	void RegisterVoice(const FUniqueNetId& PlayerId);

	/**
	* Unregisters a given player id from the voice subsystem
	*
	* @param PlayerId player to unregister with the voice subsystem
	*/
	void UnregisterVoice(const FUniqueNetId& PlayerId);

	/**
	 * Registers all local players with the current session
	 *
	 * @param Session the session that they are registering in
	 */
	void RegisterLocalPlayers(class FNamedOnlineSession* Session);

private:
	int32 Port;
	FString LocalIp;

	uint32 CreateLANSession(int32 HostingPlayerNum, FNamedOnlineSession* Session);
	uint32 CreateInternetSession(int32 HostingPlayerNum, FNamedOnlineSession* Session);

	void Heartbeat(FName SessionName);
	void OnHeartbeatResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	TPair<FString, int32> GetPublicAddress(int32 SourcePort);

	float lastHeartbeatDeltaSeconds;
	bool connectedToMaster;

	/** Reference to the main Otago subsystem */
	class FOnlineSubsystemOtago* OtagoSubsystem;

	/** Handles advertising sessions over LAN and client searches */
	FLANSession LANSessionManager;

	/** Hidden on purpose */
	FOnlineSessionOtago() :
		OtagoSubsystem(nullptr),
		CurrentSessionSearch(nullptr)
	{}

	/**
	 * Ticks any lan beacon background tasks
	 *
	 * @param DeltaTime the time since the last tick
	 */
	void TickLanTasks(float DeltaTime);

	/**
	 * Checks whether there are any sessions that need to be advertised (over LAN)
	 *
	 * @return true if there is at least one
	 */
	bool NeedsToAdvertise();

	/**
	 * Determines whether this particular session should be advertised (over LAN)
	 *
	 * @return true if yes
	 */
	bool NeedsToAdvertise( FNamedOnlineSession& Session );

	/**
	 * Determines whether this particular session is joinable.
	 *
	 * @return true if yes
	 */
	bool IsSessionJoinable( const FNamedOnlineSession& Session) const;

	/**
	 * Updates the status of LAN session (creates it if needed, shuts down if not)
	 *
	 * @return ERROR_SUCCESS if everything went successful, an error code otherwise
	 */
	uint32 UpdateLANStatus();

	/**
	 *	Join a LAN session
	 *
	 * @param PlayerNum local index of the user initiating the request
	 * @param Session newly allocated session with join information
	 * @param SearchSession the desired session to join
	 *
	 * @return ERROR_SUCCESS if successful, an error code otherwise
	 */
	uint32 JoinLANSession(int32 PlayerNum, class FNamedOnlineSession* Session, const class FOnlineSession* SearchSession);

	uint32 JoinInternetSession(int32 PlayerNum, FNamedOnlineSession* Session, const FOnlineSession* SearchSession);

	/**
	 * Builds a LAN search query and broadcasts it
	 *
	 * @return ERROR_SUCCESS if successful, an error code otherwise
	 */
	uint32 FindLANSession();

	uint32 FindInternetSession();

	void OnSessionDataRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	/**
	 * Finishes searching over LAN and returns to hosting (if needed)
	 *
	 * @return ERROR_SUCCESS if successful, an error code otherwise
	 */
	uint32 FinalizeLANSearch();

	/**
	 * Adds the game session data to the packet that is sent by the host
	 * in response to a server query
	 *
	 * @param Packet the writer object that will encode the data
	 * @param Session the session to add to the packet
	 */
	void AppendSessionToPacket(class FNboSerializeToBufferOtago& Packet, class FOnlineSession* Session);

	/**
	 * Adds the game settings data to the packet that is sent by the host
	 * in response to a server query
	 *
	 * @param Packet the writer object that will encode the data
	 * @param SessionSettings the session settings to add to the packet
	 */
	void AppendSessionSettingsToPacket(class FNboSerializeToBufferOtago& Packet, FOnlineSessionSettings* SessionSettings);

	/**
	 * Reads the settings data from the packet and applies it to the
	 * specified object
	 *
	 * @param Packet the reader object that will read the data
	 * @param SessionSettings the session settings to copy the data to
	 */
	void ReadSessionFromPacket(class FNboSerializeFromBufferOtago& Packet, class FOnlineSession* Session);

	/**
	 * Reads the settings data from the packet and applies it to the
	 * specified object
	 *
	 * @param Packet the reader object that will read the data
	 * @param SessionSettings the session settings to copy the data to
	 */
	void ReadSettingsFromPacket(class FNboSerializeFromBufferOtago& Packet, FOnlineSessionSettings& SessionSettings);

	/**
	 * Delegate triggered when the LAN beacon has detected a valid client request has been received
	 *
	 * @param PacketData packet data sent by the requesting client with header information removed
	 * @param PacketLength length of the packet not including header size
	 * @param ClientNonce the nonce returned by the client to return with the server packet
	 */
	void OnValidQueryPacketReceived(uint8* PacketData, int32 PacketLength, uint64 ClientNonce);

	/**
	 * Delegate triggered when the LAN beacon has detected a valid host response to a client request has been received
	 *
	 * @param PacketData packet data sent by the requesting client with header information removed
	 * @param PacketLength length of the packet not including header size
	 */
	void OnValidResponsePacketReceived(uint8* PacketData, int32 PacketLength);

	/**
	 * Delegate triggered when the LAN beacon has finished searching (some time after last received host packet)
	 */
	void OnLANSearchTimeout();

	/**
	 * Attempt to set the host port in the session info based on the actual port the netdriver is using.
	 */
	static void SetPortFromNetDriver(const FOnlineSubsystemOtago& Subsystem, const TSharedPtr<FOnlineSessionInfo>& SessionInfo);

	/**
	 * Returns true if the session owner is also the host.
	 */
	bool IsHost(const FNamedOnlineSession& Session) const;
};
