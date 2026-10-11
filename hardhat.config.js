import { defineConfig } from "hardhat/config";
import hardhatEthers from "@nomicfoundation/hardhat-ethers";

export default defineConfig({
    plugins: [hardhatEthers],
    solidity: {
        profiles: {
            default: {
                version: "0.8.34"
            },
            production: {
                version: "0.8.34",
                settings: {
                    optimizer: {
                        enabled: true,
                        runs: 200
                    }
                }
            }
        }
    }
});
