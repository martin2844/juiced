#pragma once
// GameSpy's NAT negotiation of the SDK linked in the stock game (0x0056C290 .. 0x0056CC90; docs/spec/28-network.md): two
// machines that share a cookie each tell the natneg servers (natneg1/natneg2.gamespy.com:27901) of themselves (INIT,
// from the game's socket and from a socket of its own), the servers answer each with the other's address (CONNECT),
// and the two ping each other (CONNECT_PING) until both have heard the other: the way through their NATs is open.
// Every packet: FD FC 1E 66 6A B2, the version 2, its type, the cookie (u32 BE).

#include "Core/Network/GameSpy/GameSpy.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace oj::net {

// NegotiateState.
enum NatNegState : int {
    kNnInitSent = 0,
    kNnInitAcked = 1,
    kNnConnectPing = 2,
    kNnFinished = 3,
    kNnCanceled = 4,
};

// NegotiateResult.
enum NatNegResult : int {
    kNnSuccess = 0,
    kNnDeadBeatPartner = 1,
    kNnInitTimeout = 2,
    kNnUnknownError = 3,
};

// NegotiateError.
enum NatNegError : int {
    kNnNoError = 0,
    kNnAllocationError = 1,
    kNnSocketError = 2,
    kNnDnsError = 3,
};

// The progress (the state reached) and the end: the result, the negotiation's own socket when it is handed over (a
// success without the game's socket; null otherwise: the game's socket is the one to use) and the other's address
// (null on a failure).
using NatNegProgressCallback = std::function<void(int state)>;
using NatNegCompletedCallback =
    std::function<void(int result, std::unique_ptr<UdpSocket> socket, const Address* remote)>;

// NNBeginNegotiationWithSocket (0x0056C7A0): kNnSocketError when the available check's result is not 1; a negotiator
// added to the list; kNnDnsError when the natneg servers do not resolve (Stock problem: the negotiator added stays,
// zeroed); its own UDP socket (kNnSocketError without), then the INITs. `gameSocket` may be none (the stock's -1).
int nnBeginNegotiationWithSocket(const GameSocket& gameSocket, int32_t cookie, int clientIndex,
                                 NatNegProgressCallback progress, NatNegCompletedCallback complete);
// NNThink (0x0056CA20): each negotiator from the last (0x0056C890): a cancelled one goes; its own socket read; the
// INITs again each 500 ms (30 times) or the PINGs each 700 ms (20 times), then kNnInitTimeout; finished, 5 s on (the
// result given then without the game's socket) it is cancelled; acknowledged without a CONNECT for 60 s,
// kNnDeadBeatPartner.
void nnThink();
// NNProcessData (0x0056CC90): a packet of NAT negotiation (from the transport's filter or the own socket).
void nnProcessData(const uint8_t* data, size_t size, const Address& from);
// NNCancel (0x0056C840): the negotiator of the cookie: its socket closed, cancelled (it goes at the next think).
void nnCancel(int32_t cookie);
// The list [0x0075F4C8] freed as the matchmaking does (0x0056D860; the negotiators' sockets with it here, the stock
// leaves them open) and none again.
void nnFreeNegotiateList();
bool nnNegotiateListExists();
size_t nnNegotiatorCount();

}  // namespace oj::net
