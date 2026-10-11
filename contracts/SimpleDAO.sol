pragma solidity ^0.8.34;

interface IGovernanceToken {
    function balanceOf(address account) external view returns (uint256);
}

contract SimpleDAO {
    IGovernanceToken public immutable governanceToken;
    uint256 public immutable votingPeriod;
    uint256 public proposalCount;

    struct Proposal {
        address targetContract;
        bytes payload;
        uint256 yesVotes;
        uint256 noVotes;
        uint256 deadline;
        bool executed;
    }

    mapping(uint256 => Proposal) public proposals;
    mapping(uint256 => mapping(address => bool)) public hasVoted;

    event ProposalCreated(uint256 indexed id, address indexed target, string description);
    event Voted(uint256 indexed id, address indexed voter, bool support, uint256 weight);
    event ProposalExecuted(uint256 indexed id);

    constructor(address tokenAddress, uint256 period) {
        governanceToken = IGovernanceToken(tokenAddress);
        votingPeriod = period;
    }

    function createProposal(address targetContract, bytes calldata payload, string calldata description) external returns (uint256 id) {
        require(governanceToken.balanceOf(msg.sender) > 0, "No governance tokens");
        id = proposalCount++;
        Proposal storage proposal = proposals[id];
        proposal.targetContract = targetContract;
        proposal.payload = payload;
        proposal.deadline = block.timestamp + votingPeriod;
        emit ProposalCreated(id, targetContract, description);
    }

    function vote(uint256 proposalId, bool support) external {
        Proposal storage proposal = proposals[proposalId];
        require(block.timestamp < proposal.deadline, "Voting ended");
        require(!hasVoted[proposalId][msg.sender], "Already voted");
        uint256 weight = governanceToken.balanceOf(msg.sender);
        require(weight > 0, "No voting power");
        support ? proposal.yesVotes += weight : proposal.noVotes += weight;
        hasVoted[proposalId][msg.sender] = true;
        emit Voted(proposalId, msg.sender, support, weight);
    }

    function executeProposal(uint256 proposalId) external {
        Proposal storage proposal = proposals[proposalId];
        require(block.timestamp >= proposal.deadline, "Voting continues");
        require(!proposal.executed, "Already executed");
        require(proposal.yesVotes > proposal.noVotes, "Proposal rejected");
        proposal.executed = true;
        (bool success, ) = proposal.targetContract.call(proposal.payload);
        require(success, "Execution failed");
        emit ProposalExecuted(proposalId);
    }
}
