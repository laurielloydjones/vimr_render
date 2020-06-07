#include "OnlineAsyncTaskManagerOtago.h"

void FOnlineAsyncTaskManagerOtago::OnlineTick()
{
	check(OtagoSubsystem);
	check(FPlatformTLS::GetCurrentThreadId() == OnlineThreadId || !FPlatformProcess::SupportsMultithreading());
}
