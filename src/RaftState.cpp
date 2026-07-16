#include "RaftState.hpp"

Term RaftState::current_term() {
	return current_term_;
}

void RaftState::observe_higher_term(Term term) {
	if (term <= current_term_) return;

	current_term_ = term;
	role_ = NodeRole::FOLLOWER;
	voted_for_.reset();

	// Persist current_term_ and voted_for_ before responding
	// to further Raft RPCs.
	persist();
}