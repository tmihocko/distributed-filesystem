#ifndef PEERSTATUS_HPP
#define PEERSTATUS_HPP
#include "Node.hpp"
#include <optional>

// Request sent to ask a peer for its current Raft state.
struct PeerStatusRequest {
	NodeId requester_id;
	Term requester_term;
};

// Information returned by a peer-discovery/status request.
// This is not a standard Raft RPC; it is an application-specific RPC.
struct PeerStatus {
	NodeId node_id;
	Term term;
	NodeRole role;

	// The leader this peer currently believes is active.
	std::optional<NodeId> leader_id;
};

#endif // PEERSTATUS_HPP