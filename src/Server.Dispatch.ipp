#ifndef DISPATCH
#define DISPATCH(KIND, FUNCTION_NAME)
#endif

DISPATCH(REQ_ClientID, clientID)

DISPATCH(REQ_SpawnProcess, spawnProcess)

#undef DISPATCH
