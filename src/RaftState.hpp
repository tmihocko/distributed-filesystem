#ifndef RAFTSTATE_HPP
#define RAFTSTATE_HPP
#include "NodeConfig.hpp"
#include "util/Singleton.hpp"
#include <optional>

class RaftState : public Singleton<RaftState> {
  public:
	[[nodiscard]] Term current_term();

	void observe_higher_term(Term term);

  private:
	void persist();

	Term current_term_ = 0;
	NodeRole role_ = NodeRole::FOLLOWER;
	std::optional<NodeId> voted_for_;
};

#endif // RAFTSTATE_HPP