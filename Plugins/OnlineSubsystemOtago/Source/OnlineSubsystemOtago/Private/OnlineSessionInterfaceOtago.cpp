#include "OnlineSessionInterfaceOtago.h"
#include "Misc/Guid.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemOtago.h"
#include "OnlineSubsystemOtagoTypes.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineAsyncTaskManager.h"
#include "SocketSubsystem.h"
#include "IPv4Subnet.h"
#include "NboSerializerOtago.h"
#include "Http.h"
#include "Json.h"
#include "Base64.h"
#include "Engine/World.h"
#include "thread"
#include "chrono"
#include "errno.h"

#include "Runtime/Json/Public/Dom/JsonObject.h"
#include "Runtime/Engine/Classes/GameFramework/GameStateBase.h"
#include "Runtime/Engine/Classes/GameFramework/PlayerState.h"
#include "Runtime/Engine/Classes/Kismet/GameplayStatics.h"

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "winsock2.h"
#include "WS2tcpip.h"

FOnlineSessionInfoOtago::FOnlineSessionInfoOtago() :
	HostAddr(NULL),
	SessionId(TEXT("INVALID"))
{
}

void FOnlineSessionInfoOtago::Init(const FOnlineSubsystemOtago& Subsystem)
{
	// Read the IP from the system
	bool bCanBindAll;
	HostAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll);

	//ISocketSubsystem::GetLocalAdapterAddresses();

	// The below is a workaround for systems that set hostname to a distinct address from 127.0.0.1 on a loopback interface.
	// See e.g. https://www.debian.org/doc/manuals/debian-reference/ch05.en.html#_the_hostname_resolution
	// and http://serverfault.com/questions/363095/why-does-my-hostname-appear-with-the-address-127-0-1-1-rather-than-127-0-0-1-in
	// Since we bind to 0.0.0.0, we won't answer on 127.0.1.1, so we need to advertise ourselves as 127.0.0.1 for any other loopback address we may have.
	uint32 HostIp = 0;
	HostAddr->GetIp(HostIp); // will return in host order

	// if this address is on loopback interface, advertise it as 127.0.0.1
	if ((HostIp & 0xff000000) == 0x7f000000)
	{
		HostAddr->SetIp(0x7f000001);	// 127.0.0.1
	}

	// Now set the port that was configured
	HostAddr->SetPort(GetPortFromNetDriver(Subsystem.GetInstanceName()));

	FGuid OwnerGuid;
	FPlatformMisc::CreateGuid(OwnerGuid);
	SessionId = FUniqueNetIdString(OwnerGuid.ToString());
}

/**
 *	Async task for ending a Otago online session
 */
class FOnlineAsyncTaskOtagoEndSession : public FOnlineAsyncTaskBasic<FOnlineSubsystemOtago>
{
private:
	/** Name of session ending */
	FName SessionName;

public:
	FOnlineAsyncTaskOtagoEndSession(class FOnlineSubsystemOtago* InSubsystem, FName InSessionName) :
		FOnlineAsyncTaskBasic(InSubsystem),
		SessionName(InSessionName)
	{
	}

	~FOnlineAsyncTaskOtagoEndSession()
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskOtagoEndSession bWasSuccessful: %d SessionName: %s"), bWasSuccessful, *SessionName.ToString());
	}

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override
	{
		// TODO: Tell the server session is over
		UE_LOG(LogOnline, Warning, TEXT("FOnlineAsyncTaskOtagoEndSession::Tick"));
		bIsComplete = true;
		bWasSuccessful = true;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() override
	{
		IOnlineSessionPtr SessionInt = Subsystem->GetSessionInterface();
		FNamedOnlineSession* Session = SessionInt->GetNamedSession(SessionName);
		if (Session)
		{
			Session->SessionState = EOnlineSessionState::Ended;
		}
	}

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override
	{
		IOnlineSessionPtr SessionInt = Subsystem->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			SessionInt->TriggerOnEndSessionCompleteDelegates(SessionName, bWasSuccessful);
		}
	}
};

/**
 *	Async task for destroying a Otago online session
 */
class FOnlineAsyncTaskOtagoDestroySession : public FOnlineAsyncTaskBasic<FOnlineSubsystemOtago>
{
private:
	/** Name of session ending */
	FName SessionName;

public:
	FOnlineAsyncTaskOtagoDestroySession(class FOnlineSubsystemOtago* InSubsystem, FName InSessionName) :
		FOnlineAsyncTaskBasic(InSubsystem),
		SessionName(InSessionName)
	{
	}

	~FOnlineAsyncTaskOtagoDestroySession()
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskOtagoDestroySession bWasSuccessful: %d SessionName: %s"), bWasSuccessful, *SessionName.ToString());
	}

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override
	{
		bIsComplete = true;
		bWasSuccessful = true;
	}

	/**
	 * Give the async task a chance to marshal its data back to the game thread
	 * Can only be called on the game thread by the async task manager
	 */
	virtual void Finalize() override
	{
		IOnlineSessionPtr SessionInt = Subsystem->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			FNamedOnlineSession* Session = SessionInt->GetNamedSession(SessionName);
			if (Session)
			{
				SessionInt->RemoveNamedSession(SessionName);
			}
		}
	}

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override
	{
		IOnlineSessionPtr SessionInt = Subsystem->GetSessionInterface();
		if (SessionInt.IsValid())
		{
			SessionInt->TriggerOnDestroySessionCompleteDelegates(SessionName, bWasSuccessful);
		}
	}
};

bool FOnlineSessionOtago::CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	uint32 Result = E_FAIL;

	// Check for an existing session
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session == NULL)
	{
		// Create a new session and deep copy the game settings
		Session = AddNamedSession(SessionName, NewSessionSettings);
		check(Session);
		Session->SessionState = EOnlineSessionState::Creating;
		Session->NumOpenPrivateConnections = NewSessionSettings.NumPrivateConnections;
		Session->NumOpenPublicConnections = NewSessionSettings.NumPublicConnections;	// always start with full public connections, local player will register later

		Session->HostingPlayerNum = HostingPlayerNum;

		check(OtagoSubsystem);
		IOnlineIdentityPtr Identity = OtagoSubsystem->GetIdentityInterface();
		if (Identity.IsValid())
		{
			Session->OwningUserId = Identity->GetUniquePlayerId(HostingPlayerNum);
			Session->OwningUserName = Identity->GetPlayerNickname(HostingPlayerNum);
		}

		// if did not get a valid one, use just something
		if (!Session->OwningUserId.IsValid())
		{
			Session->OwningUserId = MakeShareable(new FUniqueNetIdString(FString::Printf(TEXT("%d"), HostingPlayerNum)));
			Session->OwningUserName = FString(TEXT("OtagoUser"));
		}

		// Unique identifier of this build for compatibility
		Session->SessionSettings.BuildUniqueId = GetBuildUniqueId();

		if (NewSessionSettings.bIsLANMatch)
		{
			Result = CreateLANSession(HostingPlayerNum, Session);
			UE_LOG(LogOSSO, Display, TEXT("LAN Session Created!"));
		}
		else
		{
			Result = CreateInternetSession(HostingPlayerNum, Session);
			UE_LOG(LogOSSO, Display, TEXT("Internet Session Created!"));
		}

		if (Result != ERROR_IO_PENDING)
		{
			// Set the game state as pending (not started)
			Session->SessionState = EOnlineSessionState::Pending;

			if (Result != ERROR_SUCCESS)
			{
				// Clean up the session info so we don't get into a confused state
				RemoveNamedSession(SessionName);
			}
			else
			{
				RegisterLocalPlayers(Session);
			}
		}
	}
	else
	{
		UE_LOG(LogOSSO, Warning, TEXT("Cannot create session '%s': session already exists."), *SessionName.ToString());
	}

	if (Result != ERROR_IO_PENDING)
	{
		TriggerOnCreateSessionCompleteDelegates(SessionName, (Result == ERROR_SUCCESS) ? true : false);
	}

	return Result == ERROR_IO_PENDING || Result == ERROR_SUCCESS;
}

bool FOnlineSessionOtago::CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	return CreateSession(0, SessionName, NewSessionSettings);
}

uint32 FOnlineSessionOtago::CreateLANSession(int32 HostingPlayerNum, FNamedOnlineSession* Session)
{
	// Setup the host session info
	FOnlineSessionInfoOtago* NewSessionInfo = new FOnlineSessionInfoOtago();
	NewSessionInfo->Init(*OtagoSubsystem);
	Session->SessionInfo = MakeShareable(NewSessionInfo);

	return UpdateLANStatus();
}

TPair<FString, int32> FOnlineSessionOtago::GetPublicAddress(int32 SourcePort)
{
	static const uint16 BindRequestMsg = 0x0001;
	static const uint16 AttrMappedAddress = 0x0001;
	static const uint32 StunMaxResponseSize = 2048;

	// TODO: Ini configurable STUN server+port?
	hostent* Host = gethostbyname("stun.l.google.com");
	uint32 StunServerIp = ntohl(((in_addr*)Host->h_addr)->s_addr);
	uint16 StunServerPort = 19302;

	// Generate UDP Socket
	SOCKET SocketFd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (SocketFd == INVALID_SOCKET)
	{
		return TPair<FString, int32>(FString::Printf(TEXT("Could not create a UDP socket, errno %d"), WSAGetLastError()), 0);
	}

	sockaddr_in AddrIn;
	memset(&AddrIn, 0, sizeof(sockaddr_in));
	AddrIn.sin_family = AF_INET;
	AddrIn.sin_addr.s_addr = htonl(INADDR_ANY);
	AddrIn.sin_port = htons(SourcePort);
	if (bind(SocketFd, (sockaddr*)&AddrIn, sizeof(sockaddr_in)) != 0)
	{
		return TPair<FString, int32>(FString::Printf(TEXT("Could not bind socket, errno %d"), WSAGetLastError()), 0);
	}

	// Header is 2 bytes Type, 2 bytes BodySize, 16 bytes ID
	const size_t HeaderSize = sizeof(uint16) + sizeof(uint16) + sizeof(FInt128);
	char Header[HeaderSize];
	uint16 MessageTypeNs = htons(BindRequestMsg);
	FInt128 HeaderId;
	// TODO: Newer versions of Unreal support RandRang<int64>(0, INT64_MAX)
	HeaderId.Low = ((int64)FMath::RandRange(0, INT32_MAX) >> 32) + FMath::RandRange(0, INT32_MAX);
	HeaderId.High = ((int64)FMath::RandRange(0, INT32_MAX) >> 32) + FMath::RandRange(0, INT32_MAX);

	memcpy(Header, &MessageTypeNs, sizeof(uint16));
	Header[2] = 0; // There's no body required for a basic STUN request
	Header[3] = 0;
	memcpy(&Header[4], &HeaderId, sizeof(FInt128));

	sockaddr_in AddrTo;
	memset(&AddrTo, 0, sizeof(sockaddr_in));
	AddrTo.sin_family = AF_INET;
	AddrTo.sin_port = htons(StunServerPort);
	AddrTo.sin_addr.s_addr = htonl(StunServerIp);

	int SendResult = sendto(SocketFd, Header, HeaderSize, 0, (sockaddr*)&AddrTo, sizeof(sockaddr_in));
	if (SendResult == SOCKET_ERROR || SendResult == 0 || SendResult != HeaderSize)
	{
		return TPair<FString, int32>(FString::Printf(TEXT("Could not bind socket, errno %d"), WSAGetLastError()), 0);
	}

	char Response[StunMaxResponseSize];
	sockaddr_in AddrFrom;
	int AddrFromSize = sizeof(sockaddr_in);
	int ResponseSize = recvfrom(SocketFd, Response, StunMaxResponseSize, 0, (sockaddr*)&AddrFrom, &AddrFromSize);
	if (ResponseSize == SOCKET_ERROR || ResponseSize == 0)
	{
		return TPair<FString, int32>(FString::Printf(TEXT("Error in recvfrom, errno %d"), WSAGetLastError()), 0);
	}

	char* ResponsePtr = Response;
	uint16 ResponseType = ntohs(*(uint16*)ResponsePtr); // TODO: Check this is expected type
	ResponsePtr += sizeof(uint16);
	uint16 ResponseBodySize = ntohs(*(uint16*)ResponsePtr);
	ResponsePtr += sizeof(uint16);
	ResponsePtr += sizeof(FInt128); // TODO: Check this id matches the one we sent

	FString WanIpStr;
	uint32 WanIp = 0;
	uint16 WanPort;

	char* ResponseBodyEnd = ResponsePtr + ResponseBodySize;
	while (ResponsePtr < ResponseBodyEnd)
	{
		uint16 AttrType = ntohs(*(uint16*)ResponsePtr);
		ResponsePtr += sizeof(uint16);
		uint16 AttrSize = ntohs(*(uint16*)ResponsePtr);
		ResponsePtr += sizeof(uint16);

		if (AttrType == AttrMappedAddress)
		{
			*ResponsePtr++; // 1 byte padding
			*ResponsePtr++; // 1 byte family, will always be IPv4
			WanPort = ntohs(*(uint16*)ResponsePtr);
			ResponsePtr += sizeof(uint16);

			WanIp = ntohl(*(uint32*)ResponsePtr);
			ResponsePtr += sizeof(uint32);
		}
		else
		{
			// No other attributes are useful to us
			ResponsePtr += AttrSize;
		}
	}

	if (WanIp == 0)
	{
		return TPair<FString, int32>(FString("MappedAddress attribute not present in STUN response"), 0);
	}

	in_addr ip_in;
	ip_in.s_addr = htonl(WanIp);
	WanIpStr = FString(inet_ntoa(ip_in));

	closesocket(SocketFd);

	return TPair<FString, int32>(WanIpStr, WanPort);
}

uint32 FOnlineSessionOtago::CreateInternetSession(int32 HostingPlayerNum, FNamedOnlineSession* Session)
{
	// Setup the host session info
	FOnlineSessionInfoOtago* NewSessionInfo = new FOnlineSessionInfoOtago();
	NewSessionInfo->Init(*OtagoSubsystem);
	Session->SessionInfo = MakeShareable(NewSessionInfo);

	TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject);
	RequestJson->SetStringField("name", Session->SessionInfo->GetSessionId().ToString());
	RequestJson->SetStringField("project", OtagoSubsystem->GetProjectName());
	RequestJson->SetStringField("session_id", Session->SessionInfo->GetSessionId().ToString());
	RequestJson->SetStringField("owning_user", Session->OwningUserId->ToString());
	RequestJson->SetNumberField("build_unique_id", Session->SessionSettings.BuildUniqueId);
	RequestJson->SetNumberField("num_private_connections", Session->SessionSettings.NumPrivateConnections);
	RequestJson->SetNumberField("num_public_connections", Session->SessionSettings.NumPublicConnections);

	// There are numerous ways to get the active port here that DON'T ALWAYS WORK, namely:
	//  NewSessionInfo->HostAddr->GetPort()
	//  ((FOnlineSessionInfoOtago*)(Session->SessionInfo.Get()))->HostAddr->GetPort()
	//	GetPortFromNetDriver(OtagoSubsystem->GetInstanceName())
	//	All of the above rely on a '?listen' level already being loaded, which can't be guaranteed (ie user might be in the startup menu)
	Port = NewSessionInfo->HostAddr->GetPort();
	if (Port == 0)
	{
		FString AddressURL = GetWorldForOnline(OtagoSubsystem->GetInstanceName())->GetAddressURL();
		int32 PortCharIndex;
		if (AddressURL.FindChar(':', PortCharIndex))
		{
			Port = FCString::Atoi(*AddressURL.RightChop(PortCharIndex + 1));
			UE_LOG(LogOSSO, Display, TEXT("Using active port: %d!"), Port);
		}
		else
		{
			Port = 7777;
			UE_LOG(LogOSSO, Warning, TEXT("Couldn't find port number, assuming port 7777"));
		}
	}

	bool bCanBind = false;
	TSharedRef<FInternetAddr> LocalAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBind);
	if (LocalAddr->IsValid()) {
		LocalIp = LocalAddr->ToString(false);
		RequestJson->SetStringField("local_ip", LocalIp);
		RequestJson->SetNumberField("local_port", Port);
	}

	bool bUseStun = false;
	GConfig->GetBool(TEXT("OnlineSubsystemOtago"), TEXT("bUseStun"), bUseStun, GGameIni);
	if (bUseStun)
	{
		TPair<FString, int32> PublicAddress = GetPublicAddress(Port);
		if (PublicAddress.Value != 0)
		{
			RequestJson->SetStringField("wan_ip", PublicAddress.Key);
			RequestJson->SetNumberField("wan_port", PublicAddress.Value);
			UE_LOG(LogOSSO, Display, TEXT("Obtained Public Address: %s:%d"), *PublicAddress.Key, PublicAddress.Value);
		}
		else
		{
			UE_LOG(LogOSSO, Error, TEXT("Public Address %s is unresolved!"), *PublicAddress.Key);
		}
	}

	FString RequestBody;
	TSharedRef<TJsonWriter<TCHAR>> JsonWriter = TJsonWriterFactory<TCHAR>::Create(&RequestBody);
	FJsonSerializer::Serialize(RequestJson.ToSharedRef(), JsonWriter);

	if (OtagoSubsystem->GetSessionServer().IsEmpty())
	{
		return E_FAIL;
	}

	// Post session information randevu server
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindRaw(this, &FOnlineSessionOtago::OnPostSessionResponseReceived);
	Request->SetURL(OtagoSubsystem->GetSessionServer() + "/sessions");
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");
	Request->SetHeader("Content-Type", TEXT("application/json;charset=utf-8"));
	Request->SetContentAsString(RequestBody);
	Request->ProcessRequest();
	UE_LOG(LogOSSO, Display, TEXT("Public Address information POSTed to randevu server."));

	return ERROR_SUCCESS;
}

void FOnlineSessionOtago::OnPostSessionResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (bWasSuccessful && Response.IsValid()) {
		if (Response.Get()->GetResponseCode() == EHttpResponseCodes::Ok)
		{
			lastHeartbeatDeltaSeconds = 0.0f;
			connectedToMaster = true; // TODO: Remove

			FString ResponseString = Response->GetContentAsString();
			TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ResponseString);
			TSharedPtr<FJsonObject> ResponseJson;
			if(FJsonSerializer::Deserialize(JsonReader, ResponseJson))
			{
				int32 SessionId = (int32)ResponseJson->GetNumberField(FString("id"));
				UE_LOG(LogOSSO, Display, TEXT("Successfully created Session ID %d"), SessionId);
			}
			else
			{
				UE_LOG(LogOSSO, Error, TEXT("Could not parse a Session Randevu server response: %s"), *ResponseString);
			}
		}
		else
		{
			UE_LOG(LogOSSO, Error, TEXT("Session Randevu server error: %s"), *Response.Get()->GetContentAsString());
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, Response.Get()->GetContentAsString());
		}
	}
	else
	{
		UE_LOG(LogOSSO, Error, TEXT("Session Randevu server unreachable!"));
	}
}

bool FOnlineSessionOtago::NeedsToAdvertise()
{
	FScopeLock ScopeLock(&SessionLock);

	bool bResult = false;
	for (int32 SessionIdx=0; SessionIdx < Sessions.Num(); SessionIdx++)
	{
		FNamedOnlineSession& Session = Sessions[SessionIdx];
		if (NeedsToAdvertise(Session))
		{
			bResult = true;
			break;
		}
	}

	return bResult;
}

bool FOnlineSessionOtago::NeedsToAdvertise( FNamedOnlineSession& Session )
{
	// In Otago, we have to imitate missing online service functionality, so we advertise:
	// a) LAN match with open public connections (same as usually)
	// b) Not started public LAN session (same as usually)
	// d) Joinable presence-enabled session that would be advertised with in an online service
	// (all of that only if we're server)
	return Session.SessionSettings.bShouldAdvertise && IsHost(Session) &&
		(
			(
			  Session.SessionSettings.bIsLANMatch &&
			  (Session.SessionState != EOnlineSessionState::InProgress || (Session.SessionSettings.bAllowJoinInProgress && Session.NumOpenPublicConnections > 0))
			)
			||
			(
				Session.SessionSettings.bAllowJoinViaPresence || Session.SessionSettings.bAllowJoinViaPresenceFriendsOnly
			)
		);
}

bool FOnlineSessionOtago::IsSessionJoinable(const FNamedOnlineSession& Session) const
{
	const FOnlineSessionSettings& Settings = Session.SessionSettings;

	// LAN beacons are implicitly advertised.
	const bool bIsAdvertised = Settings.bShouldAdvertise || Settings.bIsLANMatch;
	const bool bIsMatchInProgress = Session.SessionState == EOnlineSessionState::InProgress;
	const bool bJoinableFromProgress = (!bIsMatchInProgress || Settings.bAllowJoinInProgress);
	const bool bAreSpacesAvailable = Session.NumOpenPublicConnections > 0;

	// LAN matches don't care about private connections / invites.
	// LAN matches don't care about presence information.
	return bIsAdvertised && bJoinableFromProgress && bAreSpacesAvailable;
}

uint32 FOnlineSessionOtago::UpdateLANStatus()
{
	uint32 Result = ERROR_SUCCESS;

	if ( NeedsToAdvertise() )
	{
		// set up LAN session
		if (LANSessionManager.GetBeaconState() == ELanBeaconState::NotUsingLanBeacon)
		{
			FOnValidQueryPacketDelegate QueryPacketDelegate = FOnValidQueryPacketDelegate::CreateRaw(this, &FOnlineSessionOtago::OnValidQueryPacketReceived);
			if (!LANSessionManager.Host(QueryPacketDelegate))
			{
				Result = E_FAIL;

				LANSessionManager.StopLANSession();
			}
		}
	}
	else
	{
		if (LANSessionManager.GetBeaconState() != ELanBeaconState::Searching)
		{
			// Tear down the LAN beacon
			LANSessionManager.StopLANSession();
		}
	}

	return Result;
}

bool FOnlineSessionOtago::StartSession(FName SessionName)
{
	uint32 Result = E_FAIL;
	// Grab the session information by name
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		// Can't start a match multiple times
		if (Session->SessionState == EOnlineSessionState::Pending ||
			Session->SessionState == EOnlineSessionState::Ended)
		{
			// If this lan match has join in progress disabled, shut down the beacon
			Result = UpdateLANStatus();
			Session->SessionState = EOnlineSessionState::InProgress;
		}
		else
		{
			UE_LOG(LogOSSO, Error, TEXT("Can't start an online session (%s) in state %s"),
				*SessionName.ToString(),
				EOnlineSessionState::ToString(Session->SessionState));
		}
	}
	else
	{
		UE_LOG(LogOSSO, Error, TEXT("Can't start an online game for session (%s) that hasn't been created!"), *SessionName.ToString());
	}
	if (Result != ERROR_IO_PENDING)
	{
		// Just trigger the delegate
		TriggerOnStartSessionCompleteDelegates(SessionName, (Result == ERROR_SUCCESS) ? true : false);
	}

	return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}

bool FOnlineSessionOtago::UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData)
{
	bool bWasSuccessful = true;

	// Grab the session information by name
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		// @TODO ONLINE update LAN settings
		Session->SessionSettings = UpdatedSessionSettings;
		TriggerOnUpdateSessionCompleteDelegates(SessionName, bWasSuccessful);
	}

	return bWasSuccessful;
}

bool FOnlineSessionOtago::EndSession(FName SessionName)
{
	uint32 Result = E_FAIL;

	// Grab the session information by name
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		// Can't end a match that isn't in progress
		if (Session->SessionState == EOnlineSessionState::InProgress)
		{
			Session->SessionState = EOnlineSessionState::Ended;

			// If the session should be advertised and the lan beacon was destroyed, recreate
			Result = UpdateLANStatus();
		}
		else
		{
			UE_LOG(LogOSSO, Error, TEXT("Can't end session (%s) in state %s"),
				*SessionName.ToString(),
				EOnlineSessionState::ToString(Session->SessionState));
		}
	}
	else
	{
		UE_LOG(LogOSSO, Error, TEXT("Can't end an online game for session (%s) that hasn't been created"),
			*SessionName.ToString());
	}

	if (Result != ERROR_IO_PENDING)
	{
		if (Session)
		{
			Session->SessionState = EOnlineSessionState::Ended;
		}

		TriggerOnEndSessionCompleteDelegates(SessionName, (Result == ERROR_SUCCESS) ? true : false);
	}

	return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}

bool FOnlineSessionOtago::DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	uint32 Result = E_FAIL;
	// Find the session in question
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		// The session info is no longer needed
		RemoveNamedSession(Session->SessionName);

		Result = UpdateLANStatus();
	}
	else
	{
		UE_LOG(LogOSSO, Warning, TEXT("You can't destroy a null Online Session (%s)"), *SessionName.ToString());
	}

	if (Result != ERROR_IO_PENDING)
	{
		CompletionDelegate.ExecuteIfBound(SessionName, (Result == ERROR_SUCCESS) ? true : false);
		TriggerOnDestroySessionCompleteDelegates(SessionName, (Result == ERROR_SUCCESS) ? true : false);
	}

	return Result == ERROR_SUCCESS || Result == ERROR_IO_PENDING;
}

bool FOnlineSessionOtago::IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId)
{
	return IsPlayerInSessionImpl(this, SessionName, UniqueId);
}

bool FOnlineSessionOtago::StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	UE_LOG(LogOSSO, Warning, TEXT("StartMatchmaking is not supported on this platform. Use FindSessions or FindSessionById."));
	TriggerOnMatchmakingCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionOtago::CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName)
{
	UE_LOG(LogOSSO, Warning, TEXT("CancelMatchmaking is not supported on this platform. Use CancelFindSessions."));
	TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionOtago::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName)
{
	UE_LOG(LogOSSO, Warning, TEXT("CancelMatchmaking is not supported on this platform. Use CancelFindSessions."));
	TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionOtago::FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	uint32 Return = E_FAIL;

	// Don't start another search while one is in progress
	if (!CurrentSessionSearch.IsValid() && SearchSettings->SearchState != EOnlineAsyncTaskState::InProgress)
	{
		// Free up previous results
		SearchSettings->SearchResults.Empty();

		// Copy the search pointer so we can keep it around
		CurrentSessionSearch = SearchSettings;

		// remember the time at which we started search, as this will be used for a "good enough" ping estimation
		SessionSearchStartInSeconds = FPlatformTime::Seconds();

		if (SearchSettings->bIsLanQuery)
		{
			UE_LOG(LogOSSO, Display, TEXT("Finding LAN Sessions!"));
			Return = FindLANSession();
		}
		else
		{
			UE_LOG(LogOSSO, Display, TEXT("Finding WAN Session Created!"));
			Return = FindInternetSession();
		}

		if (Return == ERROR_IO_PENDING)
		{
			SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;
		}
	}
	else
	{
		UE_LOG(LogOSSO, Warning, TEXT("Ignoring game search request while one is pending"));
		Return = ERROR_IO_PENDING;
	}

	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

bool FOnlineSessionOtago::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	// This function doesn't use the SearchingPlayerNum parameter, so passing in anything is fine.
	return FindSessions(0, SearchSettings);
}

bool FOnlineSessionOtago::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegates)
{
	UE_LOG(LogOSSO, Warning, TEXT("Find Session by ID not supported!"));
	FOnlineSessionSearchResult EmptyResult;
	CompletionDelegates.ExecuteIfBound(0, false, EmptyResult);
	return true;
}

uint32 FOnlineSessionOtago::FindLANSession()
{
	uint32 Return = ERROR_IO_PENDING;

	// Recreate the unique identifier for this client
	GenerateNonce((uint8*)&LANSessionManager.LanNonce, 8);

	FOnValidResponsePacketDelegate ResponseDelegate = FOnValidResponsePacketDelegate::CreateRaw(this, &FOnlineSessionOtago::OnValidResponsePacketReceived);
	FOnSearchingTimeoutDelegate TimeoutDelegate = FOnSearchingTimeoutDelegate::CreateRaw(this, &FOnlineSessionOtago::OnLANSearchTimeout);

	FNboSerializeToBufferOtago Packet(LAN_BEACON_MAX_PACKET_SIZE);
	LANSessionManager.CreateClientQueryPacket(Packet, LANSessionManager.LanNonce);
	if (LANSessionManager.Search(Packet, ResponseDelegate, TimeoutDelegate) == false)
	{
		Return = E_FAIL;

		FinalizeLANSearch();

		CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;

		// Just trigger the delegate as having failed
		TriggerOnFindSessionsCompleteDelegates(false);
	}
	return Return;
}

uint32 FOnlineSessionOtago::FindInternetSession()
{
	TSharedRef<IHttpRequest> SessionDataRequest = FHttpModule::Get().CreateRequest();
	SessionDataRequest->SetURL(OtagoSubsystem->GetSessionServer() + "/sessions");
	SessionDataRequest->SetVerb("GET");
	SessionDataRequest->SetHeader(TEXT("User-Agent"), "X-UnrealEngine-Agent");
	SessionDataRequest->OnProcessRequestComplete().BindRaw(this, &FOnlineSessionOtago::OnSessionDataRequestComplete);
	SessionDataRequest->ProcessRequest();

	UE_LOG(LogOSSO, Display, TEXT("GET request sent to Session Randevu server"));

	return ERROR_IO_PENDING;
}

bool FOnlineSessionOtago::CancelFindSessions()
{
	uint32 Return = E_FAIL;
	if (CurrentSessionSearch.IsValid() && CurrentSessionSearch->SearchState == EOnlineAsyncTaskState::InProgress)
	{
		// Make sure it's the right type
		Return = ERROR_SUCCESS;

		FinalizeLANSearch();

		CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
		CurrentSessionSearch = NULL;
	}
	else
	{
		UE_LOG(LogOSSO, Warning, TEXT("Can't cancel a search that isn't in progress"));
	}

	if (Return != ERROR_IO_PENDING)
	{
		TriggerOnCancelFindSessionsCompleteDelegates(true);
	}

	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

bool FOnlineSessionOtago::JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	uint32 Return = E_FAIL;
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	// Don't join a session if already in one or hosting one
	if (Session == NULL)
	{
		// Create a named session from the search result data
		Session = AddNamedSession(SessionName, DesiredSession.Session);
		Session->HostingPlayerNum = PlayerNum;

		FOnlineSessionInfoOtago* NewSessionInfo = new FOnlineSessionInfoOtago();
		Session->SessionInfo = MakeShareable(NewSessionInfo);

		// Create Internet or LAN match
		if (Session->SessionSettings.bIsLANMatch)
		{

			Return = JoinLANSession(PlayerNum, Session, &DesiredSession.Session);
			UE_LOG(LogOSSO, Display, TEXT("You joined a LAN Session!"));
		}
		else
		{
			if (DesiredSession.Session.SessionInfo.IsValid())
			{
				const FOnlineSessionInfoOtago* SearchSessionInfo = (const FOnlineSessionInfoOtago*)DesiredSession.Session.SessionInfo.Get();
				NewSessionInfo->SessionId = SearchSessionInfo->SessionId;

				Return = JoinInternetSession(PlayerNum, Session, &DesiredSession.Session);
				UE_LOG(LogOSSO, Display, TEXT("You joined a WAN Session!"));
			}
			else
			{
				UE_LOG(LogOSSO, Error, TEXT("Invalid session info on search result: %s"), *SessionName.ToString());
			}
		}

		if (Return != ERROR_IO_PENDING)
		{
			if (Return != ERROR_SUCCESS)
			{
				// Clean up the session info so we don't get into a confused state
				RemoveNamedSession(SessionName);
			}
			else
			{
				RegisterLocalPlayers(Session);
			}
		}
	}
	else
	{
		UE_LOG(LogOSSO, Warning, TEXT("Session (%s) already exists, you can't join twice"), *SessionName.ToString());
	}

	if (Return != ERROR_IO_PENDING)
	{
		UE_LOG(LogOSSO, Display, TEXT("****************************TriggerOnJoinSessionCompleteDelegates**************************"));
		// Just trigger the delegate as having failed
		TriggerOnJoinSessionCompleteDelegates(SessionName, Return == ERROR_SUCCESS ? EOnJoinSessionCompleteResult::Success : EOnJoinSessionCompleteResult::UnknownError);
	}

	return Return == ERROR_SUCCESS || Return == ERROR_IO_PENDING;
}

bool FOnlineSessionOtago::JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	// Assuming player 0 should be OK here
	return JoinSession(0, SessionName, DesiredSession);
}

bool FOnlineSessionOtago::FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend)
{
	// this function has to exist due to interface definition, but it does not have a meaningful implementation in Otago subsystem
	TArray<FOnlineSessionSearchResult> EmptySearchResult;
	TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, EmptySearchResult);
	return false;
};

bool FOnlineSessionOtago::FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend)
{
	// this function has to exist due to interface definition, but it does not have a meaningful implementation in Otago subsystem
	TArray<FOnlineSessionSearchResult> EmptySearchResult;
	TriggerOnFindFriendSessionCompleteDelegates(0, false, EmptySearchResult);
	return false;
}

bool FOnlineSessionOtago::FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList)
{
	// this function has to exist due to interface definition, but it does not have a meaningful implementation in Otago subsystem
	TArray<FOnlineSessionSearchResult> EmptySearchResult;
	TriggerOnFindFriendSessionCompleteDelegates(0, false, EmptySearchResult);
	return false;
}

bool FOnlineSessionOtago::SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend)
{
	// this function has to exist due to interface definition, but it does not have a meaningful implementation in Otago subsystem
	return false;
};

bool FOnlineSessionOtago::SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend)
{
	// this function has to exist due to interface definition, but it does not have a meaningful implementation in Otago subsystem
	return false;
}

bool FOnlineSessionOtago::SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	// this function has to exist due to interface definition, but it does not have a meaningful implementation in Otago subsystem
	return false;
};

bool FOnlineSessionOtago::SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	// this function has to exist due to interface definition, but it does not have a meaningful implementation in Otago subsystem
	return false;
}

uint32 FOnlineSessionOtago::JoinLANSession(int32 PlayerNum, FNamedOnlineSession* Session, const FOnlineSession* SearchSession)
{
	uint32 Result = E_FAIL;
	Session->SessionState = EOnlineSessionState::Pending;

	if (Session->SessionInfo.IsValid() && SearchSession != nullptr && SearchSession->SessionInfo.IsValid())
	{
		// Copy the session info over
		const FOnlineSessionInfoOtago* SearchSessionInfo = (const FOnlineSessionInfoOtago*)SearchSession->SessionInfo.Get();
		FOnlineSessionInfoOtago* SessionInfo = (FOnlineSessionInfoOtago*)Session->SessionInfo.Get();
		SessionInfo->SessionId = SearchSessionInfo->SessionId;

		uint32 IpAddr;
		SearchSessionInfo->HostAddr->GetIp(IpAddr);
		SessionInfo->HostAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr(IpAddr, SearchSessionInfo->HostAddr->GetPort());
		UE_LOG(LogOSSO, Display, TEXT("Joining a LAN Session on Private Address: %s!"), *SessionInfo->HostAddr->ToString(true));
		Result = ERROR_SUCCESS;
	}

	return Result;
}

uint32 FOnlineSessionOtago::JoinInternetSession(int32 PlayerNum, FNamedOnlineSession* Session, const FOnlineSession* SearchSession)
{
	uint32 Result = E_FAIL;
	Session->SessionState = EOnlineSessionState::Pending;

	if (Session->SessionInfo.IsValid())
	{
		// Copy the session info over
		const FOnlineSessionInfoOtago* SearchSessionInfo = (const FOnlineSessionInfoOtago*)SearchSession->SessionInfo.Get();
		FOnlineSessionInfoOtago* SessionInfo = (FOnlineSessionInfoOtago*)Session->SessionInfo.Get();
		SessionInfo->SessionId = SearchSessionInfo->SessionId;

		uint32 IpAddr;
		SearchSessionInfo->HostAddr->GetIp(IpAddr);
		SessionInfo->HostAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr(IpAddr, SearchSessionInfo->HostAddr->GetPort());
		UE_LOG(LogOSSO, Display, TEXT("Joining a WAN Session on Public Address: %s!"), *SessionInfo->HostAddr->ToString(true));

		Result = ERROR_SUCCESS;
	}

	return Result;
}

bool FOnlineSessionOtago::PingSearchResults(const FOnlineSessionSearchResult& SearchResult)
{
	return false;
}

/** Get a resolved connection string from a session info */
static bool GetConnectStringFromSessionInfo(TSharedPtr<FOnlineSessionInfoOtago>& SessionInfo, FString& ConnectInfo, int32 PortOverride=0)
{
	bool bSuccess = false;
	if (SessionInfo.IsValid())
	{
		if (SessionInfo->HostAddr.IsValid() && SessionInfo->HostAddr->IsValid())
		{
			if (PortOverride != 0)
			{
				ConnectInfo = FString::Printf(TEXT("%s:%d"), *SessionInfo->HostAddr->ToString(false), PortOverride);
			}
			else
			{
				ConnectInfo = FString::Printf(TEXT("%s"), *SessionInfo->HostAddr->ToString(true));
			}

			bSuccess = true;
		}
	}

	return bSuccess;
}

bool FOnlineSessionOtago::GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType)
{
	bool bSuccess = false;
	// Find the session
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session != NULL)
	{
		TSharedPtr<FOnlineSessionInfoOtago> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoOtago>(Session->SessionInfo);
		if (PortType == BeaconPort)
		{
			int32 BeaconListenPort = GetBeaconPortFromSessionSettings(Session->SessionSettings);
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo, BeaconListenPort);
		}
		else if (PortType == GamePort)
		{
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo);
		}

		if (!bSuccess)
		{
			UE_LOG(LogOSSO, Warning, TEXT("Invalid session info for session %s in GetResolvedConnectString()"), *SessionName.ToString());
		}
	}
	else
	{
		UE_LOG(LogOSSO, Warning,
			TEXT("Unknown session name (%s) specified to GetResolvedConnectString()"),
			*SessionName.ToString());
	}

	return bSuccess;
}

bool FOnlineSessionOtago::GetResolvedConnectString(const FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)
{
	bool bSuccess = false;
	if (SearchResult.Session.SessionInfo.IsValid())
	{
		TSharedPtr<FOnlineSessionInfoOtago> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoOtago>(SearchResult.Session.SessionInfo);

		if (PortType == BeaconPort)
		{
			int32 BeaconListenPort = GetBeaconPortFromSessionSettings(SearchResult.Session.SessionSettings);
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo, BeaconListenPort);

		}
		else if (PortType == GamePort)
		{
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo);
		}
	}

	if (!bSuccess || ConnectInfo.IsEmpty())
	{
		UE_LOG(LogOSSO, Warning, TEXT("Invalid session info in search result to GetResolvedConnectString()"));
	}

	return bSuccess;
}

FOnlineSessionSettings* FOnlineSessionOtago::GetSessionSettings(FName SessionName)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		return &Session->SessionSettings;
	}
	return NULL;
}

void FOnlineSessionOtago::RegisterLocalPlayers(FNamedOnlineSession* Session)
{
	if (!OtagoSubsystem->IsDedicated())
	{
		IOnlineVoicePtr VoiceInt = OtagoSubsystem->GetVoiceInterface();
		if (VoiceInt.IsValid())
		{
			for (int32 Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
			{
				// Register the local player as a local talker
				VoiceInt->RegisterLocalTalker(Index);
			}
		}
	}
}

void FOnlineSessionOtago::RegisterVoice(const FUniqueNetId& PlayerId)
{
	IOnlineVoicePtr VoiceInt = OtagoSubsystem->GetVoiceInterface();
	if (VoiceInt.IsValid())
	{
		if (!OtagoSubsystem->IsLocalPlayer(PlayerId))
		{
			VoiceInt->RegisterRemoteTalker(PlayerId);
		}
		else
		{
			// This is a local player. In case their PlayerState came last during replication, reprocess muting
			VoiceInt->ProcessMuteChangeNotification();
		}
	}
}

void FOnlineSessionOtago::UnregisterVoice(const FUniqueNetId& PlayerId)
{
	IOnlineVoicePtr VoiceInt = OtagoSubsystem->GetVoiceInterface();
	if (VoiceInt.IsValid())
	{
		if (!OtagoSubsystem->IsLocalPlayer(PlayerId))
		{
			if (VoiceInt.IsValid())
			{
				VoiceInt->UnregisterRemoteTalker(PlayerId);
			}
		}
	}
}

bool FOnlineSessionOtago::RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)
{
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(MakeShareable(new FUniqueNetIdString(PlayerId)));
	return RegisterPlayers(SessionName, Players, bWasInvited);
}

bool FOnlineSessionOtago::RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited)
{
	bool bSuccess = false;
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		bSuccess = true;

		for (int32 PlayerIdx=0; PlayerIdx<Players.Num(); PlayerIdx++)
		{
			const TSharedRef<const FUniqueNetId>& PlayerId = Players[PlayerIdx];

			FUniqueNetIdMatcher PlayerMatch(*PlayerId);
			if (Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch) == INDEX_NONE)
			{
				Session->RegisteredPlayers.Add(PlayerId);
				RegisterVoice(*PlayerId);

				// update number of open connections
				if (Session->NumOpenPublicConnections > 0)
				{
					Session->NumOpenPublicConnections--;
				}
				else if (Session->NumOpenPrivateConnections > 0)
				{
					Session->NumOpenPrivateConnections--;
				}
			}
			else
			{
				RegisterVoice(*PlayerId);
				UE_LOG(LogOSSO, Warning, TEXT("Player %s already registered in session %s"), *PlayerId->ToDebugString(), *SessionName.ToString());
			}
		}
	}
	else
	{
		UE_LOG(LogOSSO, Warning, TEXT("No game present to join for session (%s)"), *SessionName.ToString());
	}

	TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
	return bSuccess;
}

bool FOnlineSessionOtago::UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId)
{
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(MakeShareable(new FUniqueNetIdString(PlayerId)));
	return UnregisterPlayers(SessionName, Players);
}

bool FOnlineSessionOtago::UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players)
{
	bool bSuccess = true;

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		for (int32 PlayerIdx=0; PlayerIdx < Players.Num(); PlayerIdx++)
		{
			const TSharedRef<const FUniqueNetId>& PlayerId = Players[PlayerIdx];

			FUniqueNetIdMatcher PlayerMatch(*PlayerId);
			int32 RegistrantIndex = Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch);
			if (RegistrantIndex != INDEX_NONE)
			{
				Session->RegisteredPlayers.RemoveAtSwap(RegistrantIndex);
				UnregisterVoice(*PlayerId);

				// update number of open connections
				if (Session->NumOpenPublicConnections < Session->SessionSettings.NumPublicConnections)
				{
					Session->NumOpenPublicConnections++;
				}
				else if (Session->NumOpenPrivateConnections < Session->SessionSettings.NumPrivateConnections)
				{
					Session->NumOpenPrivateConnections++;
				}
			}
			else
			{
				UE_LOG(LogOSSO, Warning, TEXT("Player %s is not part of session (%s)"), *PlayerId->ToDebugString(), *SessionName.ToString());
			}
		}
	}
	else
	{
		UE_LOG(LogOSSO, Warning, TEXT("No game present to leave for session (%s)"), *SessionName.ToString());
		bSuccess = false;
	}

	TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
	return bSuccess;
}

void FOnlineSessionOtago::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_Session_Interface);
	TickLanTasks(DeltaTime);
}

void FOnlineSessionOtago::TickLanTasks(float DeltaTime)
{
	LANSessionManager.Tick(DeltaTime);
}

void FOnlineSessionOtago::AppendSessionToPacket(FNboSerializeToBufferOtago& Packet, FOnlineSession* Session)
{
	/** Owner of the session */
	Packet << *StaticCastSharedPtr<const FUniqueNetIdString>(Session->OwningUserId)
		<< Session->OwningUserName
		<< Session->NumOpenPrivateConnections
		<< Session->NumOpenPublicConnections;

	// Try to get the actual port the netdriver is using
	SetPortFromNetDriver(*OtagoSubsystem, Session->SessionInfo);

	// Write host info (host addr, session id, and key)
	Packet << *StaticCastSharedPtr<FOnlineSessionInfoOtago>(Session->SessionInfo);

	// Now append per game settings
	AppendSessionSettingsToPacket(Packet, &Session->SessionSettings);
}

void FOnlineSessionOtago::AppendSessionSettingsToPacket(FNboSerializeToBufferOtago& Packet, FOnlineSessionSettings* SessionSettings)
{
#if DEBUG_LAN_BEACON
	UE_LOG_ONLINE(Verbose, TEXT("Sending session settings to client"));
#endif

	// Members of the session settings class
	Packet << SessionSettings->NumPublicConnections
		<< SessionSettings->NumPrivateConnections
		<< (uint8)SessionSettings->bShouldAdvertise
		<< (uint8)SessionSettings->bIsLANMatch
		<< (uint8)SessionSettings->bIsDedicated
		<< (uint8)SessionSettings->bUsesStats
		<< (uint8)SessionSettings->bAllowJoinInProgress
		<< (uint8)SessionSettings->bAllowInvites
		<< (uint8)SessionSettings->bUsesPresence
		<< (uint8)SessionSettings->bAllowJoinViaPresence
		<< (uint8)SessionSettings->bAllowJoinViaPresenceFriendsOnly
		<< (uint8)SessionSettings->bAntiCheatProtected
	    << SessionSettings->BuildUniqueId;

	// First count number of advertised keys
	int32 NumAdvertisedProperties = 0;
	for (FSessionSettings::TConstIterator It(SessionSettings->Settings); It; ++It)
	{
		const FOnlineSessionSetting& Setting = It.Value();
		if (Setting.AdvertisementType >= EOnlineDataAdvertisementType::ViaOnlineService)
		{
			NumAdvertisedProperties++;
		}
	}

	// Add count of advertised keys and the data
	Packet << (int32)NumAdvertisedProperties;
	for (FSessionSettings::TConstIterator It(SessionSettings->Settings); It; ++It)
	{
		const FOnlineSessionSetting& Setting = It.Value();
		if (Setting.AdvertisementType >= EOnlineDataAdvertisementType::ViaOnlineService)
		{
			Packet << It.Key();
			Packet << Setting;
#if DEBUG_LAN_BEACON
			UE_LOG_ONLINE(Verbose, TEXT("%s"), *Setting.ToString());
#endif
		}
	}
}

void FOnlineSessionOtago::OnValidQueryPacketReceived(uint8* PacketData, int32 PacketLength, uint64 ClientNonce)
{
	// Iterate through all registered sessions and respond for each one that can be joinable
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SessionIndex = 0; SessionIndex < Sessions.Num(); SessionIndex++)
	{
		FNamedOnlineSession* Session = &Sessions[SessionIndex];

		// Don't respond to query if the session is not a joinable LAN match.
		if (Session && IsSessionJoinable(*Session))
		{
			FNboSerializeToBufferOtago Packet(LAN_BEACON_MAX_PACKET_SIZE);
			// Create the basic header before appending additional information
			LANSessionManager.CreateHostResponsePacket(Packet, ClientNonce);

			// Add all the session details
			AppendSessionToPacket(Packet, Session);

			// Broadcast this response so the client can see us
			if (!Packet.HasOverflow())
			{
				LANSessionManager.BroadcastPacket(Packet, Packet.GetByteCount());
			}
			else
			{
				UE_LOG(LogOSSO, Error, TEXT("LAN broadcast packet overflow, cannot broadcast on LAN"));
			}
		}
	}
}

void FOnlineSessionOtago::ReadSessionFromPacket(FNboSerializeFromBufferOtago& Packet, FOnlineSession* Session)
{
#if DEBUG_LAN_BEACON
	UE_LOG_ONLINE(Verbose, TEXT("Reading session information from server"));
#endif

	/** Owner of the session */
	FUniqueNetIdString* UniqueId = new FUniqueNetIdString;
	Packet >> *UniqueId
		>> Session->OwningUserName
		>> Session->NumOpenPrivateConnections
		>> Session->NumOpenPublicConnections;

	Session->OwningUserId = MakeShareable(UniqueId);

	// Allocate and read the connection data
	FOnlineSessionInfoOtago* OtagoSessionInfo = new FOnlineSessionInfoOtago();
	OtagoSessionInfo->HostAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	Packet >> *OtagoSessionInfo;
	Session->SessionInfo = MakeShareable(OtagoSessionInfo);

	// Read any per object data using the server object
	ReadSettingsFromPacket(Packet, Session->SessionSettings);
}

void FOnlineSessionOtago::ReadSettingsFromPacket(FNboSerializeFromBufferOtago& Packet, FOnlineSessionSettings& SessionSettings)
{
#if DEBUG_LAN_BEACON
	UE_LOG_ONLINE(Verbose, TEXT("Reading game settings from server"));
#endif

	// Clear out any old settings
	SessionSettings.Settings.Empty();

	// Members of the session settings class
	Packet >> SessionSettings.NumPublicConnections
		>> SessionSettings.NumPrivateConnections;
	uint8 Read = 0;
	// Read all the bools as bytes
	Packet >> Read;
	SessionSettings.bShouldAdvertise = !!Read;
	Packet >> Read;
	SessionSettings.bIsLANMatch = !!Read;
	Packet >> Read;
	SessionSettings.bIsDedicated = !!Read;
	Packet >> Read;
	SessionSettings.bUsesStats = !!Read;
	Packet >> Read;
	SessionSettings.bAllowJoinInProgress = !!Read;
	Packet >> Read;
	SessionSettings.bAllowInvites = !!Read;
	Packet >> Read;
	SessionSettings.bUsesPresence = !!Read;
	Packet >> Read;
	SessionSettings.bAllowJoinViaPresence = !!Read;
	Packet >> Read;
	SessionSettings.bAllowJoinViaPresenceFriendsOnly = !!Read;
	Packet >> Read;
	SessionSettings.bAntiCheatProtected = !!Read;

	// BuildId
	Packet >> SessionSettings.BuildUniqueId;

	// Now read the contexts and properties from the settings class
	int32 NumAdvertisedProperties = 0;
	// First, read the number of advertised properties involved, so we can presize the array
	Packet >> NumAdvertisedProperties;
	if (Packet.HasOverflow() == false)
	{
		FName Key;
		// Now read each context individually
		for (int32 Index = 0;
			Index < NumAdvertisedProperties && Packet.HasOverflow() == false;
			Index++)
		{
			FOnlineSessionSetting Setting;
			Packet >> Key;
			Packet >> Setting;
			SessionSettings.Set(Key, Setting);

#if DEBUG_LAN_BEACON
			UE_LOG_ONLINE(Verbose, TEXT("%s"), *Setting->ToString());
#endif
		}
	}

	// If there was an overflow, treat the string settings/properties as broken
	if (Packet.HasOverflow())
	{
		SessionSettings.Settings.Empty();
		UE_LOG(LogOSSO, Error, TEXT("Packet overflow detected in ReadGameSettingsFromPacket()"));
	}
}

void FOnlineSessionOtago::OnValidResponsePacketReceived(uint8* PacketData, int32 PacketLength)
{
	if (CurrentSessionSearch.IsValid())
	{
		// Add space in the search results array
		FOnlineSessionSearchResult* NewResult = new (CurrentSessionSearch->SearchResults) FOnlineSessionSearchResult();
		// this is not a correct ping, but better than nothing
		NewResult->PingInMs = static_cast<int32>((FPlatformTime::Seconds() - SessionSearchStartInSeconds) * 1000);

		FOnlineSession* NewSession = &NewResult->Session;

		// Prepare to read data from the packet
		FNboSerializeFromBufferOtago Packet(PacketData, PacketLength);

		ReadSessionFromPacket(Packet, NewSession);

		// NOTE: we don't notify until the timeout happens
	}
	else
	{
		UE_LOG(LogOSSO, Error, TEXT("Failed to create new online game settings object"));
	}
}

uint32 FOnlineSessionOtago::FinalizeLANSearch()
{
	if (LANSessionManager.GetBeaconState() == ELanBeaconState::Searching)
	{
		LANSessionManager.StopLANSession();
	}

	return UpdateLANStatus();
}

void FOnlineSessionOtago::OnLANSearchTimeout()
{
	FinalizeLANSearch();

	if (CurrentSessionSearch.IsValid())
	{
		if (CurrentSessionSearch->SearchResults.Num() > 0)
		{
			// Allow game code to sort the servers
			CurrentSessionSearch->SortSearchResults();
		}
		CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Done;

		CurrentSessionSearch = NULL;
	}

	// Trigger the delegate as complete
	TriggerOnFindSessionsCompleteDelegates(true);
}

void FOnlineSessionOtago::OnSessionDataRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!CurrentSessionSearch.IsValid())
	{
		UE_LOG(LogOSSO, Error, TEXT("Failed to create new online game settings object during internet session search"));
		return;
	}
	if (bWasSuccessful && Response.IsValid()) {
		if (Response.Get()->GetResponseCode() == EHttpResponseCodes::Ok)
		{
			FString ResponseString = Response->GetContentAsString();
			TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ResponseString);
			TArray<TSharedPtr<FJsonValue>> SessionsArray;
			if(FJsonSerializer::Deserialize(JsonReader, SessionsArray))
			{
				// TODO: Just pack all found sessions into search results and let the client decide
				//       For now not doing this as the server doesn't expire old/inactive sessions
				int32 MaxSessionId = -1;
				TSharedPtr<FJsonObject> NewestSessionJson;
				for (auto& SessionValue : SessionsArray)
				{
					const TSharedPtr<FJsonObject> Session = SessionValue->AsObject();
					int32 Id = (int32)Session->GetNumberField("id");
					if (Id > MaxSessionId)
					{
						MaxSessionId = Id;
						NewestSessionJson = Session;
					}
				}

				// TODO: Use LogSession Method?
				UE_LOG(LogOSSO, Display, TEXT("SessionDetails"));
				if (NewestSessionJson.IsValid())
				{
					UE_LOG(LogOSSO, Display, TEXT("id: %d"), (int32)NewestSessionJson->GetNumberField("id"));
					UE_LOG(LogOSSO, Display, TEXT("name: %s"), *NewestSessionJson->GetStringField("name"));
					if (NewestSessionJson->HasTypedField<EJson::String>("local_ip"))
					{
						UE_LOG(LogOSSO, Display, TEXT("local_ip: %s"), *NewestSessionJson->GetStringField("local_ip"));
						UE_LOG(LogOSSO, Display, TEXT("local_port: %d"), (int32)NewestSessionJson->GetNumberField("local_port"));
					}
					else
					{
						UE_LOG(LogOSSO, Display, TEXT("no local address"));
					}
					if (NewestSessionJson->HasTypedField<EJson::String>("wan_ip"))
					{
						UE_LOG(LogOSSO, Display, TEXT("wan_ip: %s"), *NewestSessionJson->GetStringField("wan_ip"));
						UE_LOG(LogOSSO, Display, TEXT("wan_port: %d"), (int32)NewestSessionJson->GetNumberField("wan_port"));
					}
					else
					{
						UE_LOG(LogOSSO, Display, TEXT("no wan address"));
					}

					FOnlineSessionSearchResult* NewResult = new (CurrentSessionSearch->SearchResults) FOnlineSessionSearchResult();
					// To find the ping for real would require sessions acting as OnlineFramework::QosBeaconHost, which we'd ping
					// using QosBeaconClient. For now it's immaterial as we always have a specific session we want to join
					NewResult->PingInMs = 30;

					FOnlineSession* Session = &NewResult->Session;

					Session->OwningUserName = NewestSessionJson->GetStringField("owning_user");
					Session->OwningUserId = MakeShareable(new FUniqueNetIdString(Session->OwningUserName));
					// TODO: Seperate this out from NumPrivateConnections below based on number of current users (requires host to update server when people connect)
					Session->NumOpenPrivateConnections = NewestSessionJson->GetNumberField("num_private_connections");
					Session->NumOpenPublicConnections = NewestSessionJson->GetNumberField("num_public_connections");

					FOnlineSessionInfoOtago* SessionInfo = new FOnlineSessionInfoOtago();
					SessionInfo->HostAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
					FString test = SessionInfo->HostAddr->ToString(true);
					test = SessionInfo->HostAddr->ToString(true);
					// Use WAN if available, overwise fallback to local
					bool bIsValidIP = false;
					bool bIsLANMatch = !NewestSessionJson->HasTypedField<EJson::String>("wan_ip");
					if (bIsLANMatch)
					{
						SessionInfo->HostAddr->SetIp(*NewestSessionJson->GetStringField("local_ip"), bIsValidIP);
						SessionInfo->HostAddr->SetPort(NewestSessionJson->GetNumberField("local_port"));
					}
					else
					{
						// Still use local IP if on the same subnet
						bool bUsingLocal = false;
						/*if (NewestSessionJson->HasTypedField<EJson::String>("local_ip"))
						{
							bool bCanBind = false;
							TSharedRef<FInternetAddr> LocalAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBind);
							if (LocalAddr->IsValid())
							{
								uint32 LocalIpInt;
								LocalAddr->GetIp(LocalIpInt);
								// TODO: Get actual machine subnet, seems to be no Unreal API for that
								FIPv4Subnet Subnet(FIPv4Address(LocalIpInt), FIPv4SubnetMask(255, 255, 0, 0));
								in_addr RemoteIp;
								inet_pton(AF_INET, StringCast<ANSICHAR>(*NewestSessionJson->GetStringField("local_ip")).Get(), &RemoteIp);
								if (Subnet.ContainsAddress(FIPv4Address(ntohl(RemoteIp.s_addr))))
								{
									UE_LOG(LogOnline, Warning, TEXT("Subnet match, using local IP"));
									SessionInfo->HostAddr->SetIp(*NewestSessionJson->GetStringField("local_ip"), bIsValidIP);
									SessionInfo->HostAddr->SetPort(NewestSessionJson->GetNumberField("local_port"));
									bUsingLocal = true;
								}
							}
						}*/
						//if (!bUsingLocal)
						//{
							SessionInfo->HostAddr->SetIp(*NewestSessionJson->GetStringField("wan_ip"), bIsValidIP);
							SessionInfo->HostAddr->SetPort(NewestSessionJson->GetNumberField("wan_port"));
						//}
					}
					if (!bIsValidIP)
					{
						UE_LOG(LogOSSO, Error, TEXT("Invalid session IP Address: %s"), bIsLANMatch ? *NewestSessionJson->GetStringField("local_ip") : *NewestSessionJson->GetStringField("wan_ip"));
					}
					SessionInfo->SessionId = FUniqueNetIdString(NewestSessionJson->GetStringField("session_id"));
					Session->SessionInfo = MakeShareable(SessionInfo);

					Session->SessionSettings.Settings.Empty();
					Session->SessionSettings.bIsLANMatch = false;
					Session->SessionSettings.NumPrivateConnections = NewestSessionJson->GetNumberField("num_private_connections");
					Session->SessionSettings.NumPublicConnections = NewestSessionJson->GetNumberField("num_public_connections");
					Session->SessionSettings.bShouldAdvertise = true;
					Session->SessionSettings.bIsDedicated = false;
					Session->SessionSettings.bUsesStats = false;
					Session->SessionSettings.bAllowJoinInProgress = true;
					Session->SessionSettings.bAllowInvites = false;
					Session->SessionSettings.bUsesPresence = true;
					Session->SessionSettings.bAllowJoinViaPresence = true;
					Session->SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;
					Session->SessionSettings.bAntiCheatProtected = false;
					Session->SessionSettings.BuildUniqueId = NewestSessionJson->GetNumberField("build_unique_id");

					if (Session->SessionSettings.BuildUniqueId != GetBuildUniqueId())
					{
						UE_LOG(LogOSSO, Warning, TEXT("Session '%s' has differing build %d - ours is %d"), *SessionInfo->SessionId.ToString(), Session->SessionSettings.BuildUniqueId, GetBuildUniqueId());
					}
				}

				if (CurrentSessionSearch->SearchResults.Num() > 0)
				{
					CurrentSessionSearch->SortSearchResults();
				}
				CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Done;
				CurrentSessionSearch = NULL;

				TriggerOnFindSessionsCompleteDelegates(true);
			}
			else
			{
				UE_LOG(LogOSSO, Error, TEXT("Could not parse server response: %s"), *ResponseString);

				CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
				CurrentSessionSearch = NULL;

				TriggerOnFindSessionsCompleteDelegates(false);
			}
		}
		else
		{
			UE_LOG(LogOSSO, Error, TEXT("Session server error: %s"), *Response.Get()->GetContentAsString());
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, Response.Get()->GetContentAsString());

			CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
			CurrentSessionSearch = NULL;

			TriggerOnFindSessionsCompleteDelegates(false);
		}
	}
	else
	{
		UE_LOG(LogOSSO, Error, TEXT("Session server unreachable"));

		CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
		CurrentSessionSearch = NULL;

		TriggerOnFindSessionsCompleteDelegates(false);
	}
}

int32 FOnlineSessionOtago::GetNumSessions()
{
	FScopeLock ScopeLock(&SessionLock);
	return Sessions.Num();
}

void FOnlineSessionOtago::DumpSessionState()
{
	FScopeLock ScopeLock(&SessionLock);

	for (int32 SessionIdx=0; SessionIdx < Sessions.Num(); SessionIdx++)
	{
		DumpNamedSession(&Sessions[SessionIdx]);
	}
}

void FOnlineSessionOtago::RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::Success);
}

void FOnlineSessionOtago::UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(PlayerId, true);
}

void FOnlineSessionOtago::SetPortFromNetDriver(const FOnlineSubsystemOtago& Subsystem, const TSharedPtr<FOnlineSessionInfo>& SessionInfo)
{
	auto NetDriverPort = GetPortFromNetDriver(Subsystem.GetInstanceName());
	auto SessionInfoOtago = StaticCastSharedPtr<FOnlineSessionInfoOtago>(SessionInfo);
	if (SessionInfoOtago.IsValid() && SessionInfoOtago->HostAddr.IsValid())
	{
		SessionInfoOtago->HostAddr->SetPort(NetDriverPort);
	}
}

bool FOnlineSessionOtago::IsHost(const FNamedOnlineSession& Session) const
{
	if (OtagoSubsystem->IsDedicated())
	{
		return true;
	}

	IOnlineIdentityPtr IdentityInt = OtagoSubsystem->GetIdentityInterface();
	if (!IdentityInt.IsValid())
	{
		return false;
	}

	TSharedPtr<const FUniqueNetId> UserId = IdentityInt->GetUniquePlayerId(Session.HostingPlayerNum);
	return (UserId.IsValid() && (*UserId == *Session.OwningUserId));
}
FString FOnlineSessionOtago::getSessionIDFromRequest(FHttpRequestPtr Request)
{
	TArray<FString> Parsed;
	if (Request->GetURL().ParseIntoArray(Parsed, TEXT("/"), false) >= 6) {
		return Parsed[5];
	}
	else {
		return "";
	}
}

FString FOnlineSessionOtago::getDataIDFromRequest(FHttpRequestPtr Request)
{
	TArray<FString> Parsed;
	if (Request->GetURL().ParseIntoArray(Parsed, TEXT("/"), false) >= 8) {
		return Parsed[7];
	}
	else {
		return "";
	}
}