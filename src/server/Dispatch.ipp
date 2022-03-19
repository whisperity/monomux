#ifndef DISPATCH
/// Set for the given \p MessageKind the callback \p FUNCTION_NAME.
#define DISPATCH(KIND, FUNCTION_NAME)
#endif

DISPATCH(ClientIDRequest, requestClientID)
DISPATCH(DataSocketRequest, requestDataSocket)

DISPATCH(SessionListRequest, requestSessionList)
DISPATCH(MakeSessionRequest, requestMakeSession)
DISPATCH(AttachRequest, requestAttach)

#undef DISPATCH
