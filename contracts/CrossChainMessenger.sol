pragma solidity ^0.8.24;

import {Client} from "@chainlink/contracts-ccip/contracts/libraries/Client.sol";
import {IRouterClient} from "@chainlink/contracts-ccip/contracts/interfaces/IRouterClient.sol";

contract CrossChainMessenger {
    IRouterClient public immutable router;

    event MessageSent(bytes32 indexed messageId, uint64 indexed destinationChainSelector, address indexed receiver, string text, uint256 fee);

    constructor(address routerAddress) {
        router = IRouterClient(routerAddress);
    }

    receive() external payable {
    }

    function getFee(uint64 destinationChainSelector, address receiver, string calldata text) external view returns (uint256) {
        return router.getFee(destinationChainSelector, buildMessage(receiver, text));
    }

    function sendMessage(uint64 destinationChainSelector, address receiver, string calldata text) external returns (bytes32 messageId) {
        Client.EVM2AnyMessage memory message = buildMessage(receiver, text);
        uint256 fee = router.getFee(destinationChainSelector, message);
        require(address(this).balance >= fee, "Insufficient CCIP fee");
        messageId = router.ccipSend{value: fee}(destinationChainSelector, message);
        emit MessageSent(messageId, destinationChainSelector, receiver, text, fee);
    }

    function buildMessage(address receiver, string calldata text) private pure returns (Client.EVM2AnyMessage memory) {
        Client.EVMTokenAmount[] memory tokenAmounts = new Client.EVMTokenAmount[](0);
        return Client.EVM2AnyMessage({
            receiver: abi.encode(receiver),
            data: abi.encode(text),
            tokenAmounts: tokenAmounts,
            feeToken: address(0),
            extraArgs: Client._argsToBytes(Client.GenericExtraArgsV2({gasLimit: 200000, allowOutOfOrderExecution: true}))
        });
    }
}
