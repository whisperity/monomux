/* SPDX-License-Identifier: LGPL-3.0-only */

#ifndef DISPATCH
/// Set for the given \p MessageKind the callback \p FUNCTION_NAME.
#define DISPATCH(KIND, FUNCTION_NAME)
#endif

DISPATCH(ClientIDRequest, requestClientID)
DISPATCH(DataSocketRequest, requestDataSocket)

DISPATCH(SessionListRequest, requestSessionList)
DISPATCH(MakeSessionRequest, requestMakeSession)
DISPATCH(AttachRequest, requestAttach)
DISPATCH(DetachRequest, requestDetach)

DISPATCH(SignalRequest, signalSession)

DISPATCH(RedrawNotification, redrawNotified)

DISPATCH(StatisticsRequest, statisticsRequest)

#undef DISPATCH
