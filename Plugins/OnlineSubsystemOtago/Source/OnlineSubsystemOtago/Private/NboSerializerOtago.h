#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemOtagoTypes.h"
#include "NboSerializer.h"

/**
 * Serializes data in network byte order form into a buffer
 */
class FNboSerializeToBufferOtago : public FNboSerializeToBuffer
{
public:
	/** Default constructor zeros num bytes*/
	FNboSerializeToBufferOtago() :
		FNboSerializeToBuffer(512)
	{
	}

	/** Constructor specifying the size to use */
	FNboSerializeToBufferOtago(uint32 Size) :
		FNboSerializeToBuffer(Size)
	{
	}

	/**
	 * Adds Otago session info to the buffer
	 */
 	friend inline FNboSerializeToBufferOtago& operator<<(FNboSerializeToBufferOtago& Ar, const FOnlineSessionInfoOtago& SessionInfo)
 	{
		check(SessionInfo.HostAddr.IsValid());
		// Skip SessionType (assigned at creation)
		Ar << SessionInfo.SessionId;
		Ar << *SessionInfo.HostAddr;
		return Ar;
 	}

	/**
	 * Adds Otago Unique Id to the buffer
	 */
	friend inline FNboSerializeToBufferOtago& operator<<(FNboSerializeToBufferOtago& Ar, const FUniqueNetIdString& UniqueId)
	{
		Ar << UniqueId.UniqueNetIdStr;
		return Ar;
	}
};

/**
 * Class used to write data into packets for sending via system link
 */
class FNboSerializeFromBufferOtago : public FNboSerializeFromBuffer
{
public:
	/**
	 * Initializes the buffer, size, and zeros the read offset
	 */
	FNboSerializeFromBufferOtago(uint8* Packet,int32 Length) :
		FNboSerializeFromBuffer(Packet,Length)
	{
	}

	/**
	 * Reads Otago session info from the buffer
	 */
 	friend inline FNboSerializeFromBufferOtago& operator>>(FNboSerializeFromBufferOtago& Ar, FOnlineSessionInfoOtago& SessionInfo)
 	{
		check(SessionInfo.HostAddr.IsValid());
		// Skip SessionType (assigned at creation)
		Ar >> SessionInfo.SessionId;
		Ar >> *SessionInfo.HostAddr;
		return Ar;
 	}

	/**
	 * Reads Otago Unique Id from the buffer
	 */
	friend inline FNboSerializeFromBufferOtago& operator>>(FNboSerializeFromBufferOtago& Ar, FUniqueNetIdString& UniqueId)
	{
		Ar >> UniqueId.UniqueNetIdStr;
		return Ar;
	}
};
