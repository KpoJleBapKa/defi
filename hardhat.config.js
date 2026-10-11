import { defineConfig } from "hardhat/config";
import hardhatEthers from "@nomicfoundation/hardhat-ethers";

export default defineConfig({
    plugins: [hardhatEthers],
    solidity: {
        profiles: {
            default: {
                version: "0.8.24"
            },
            production: {
                version: "0.8.24",
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
