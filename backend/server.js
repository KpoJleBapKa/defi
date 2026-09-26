import { readFileSync } from "node:fs";
import { createServer } from "node:http";
import { DatabaseSync } from "node:sqlite";
import { Contract, JsonRpcProvider, formatEther, isAddress } from "ethers";

const configuration = JSON.parse(readFileSync(".lab3/deployment.json", "utf8"));
const artifact = JSON.parse(readFileSync("artifacts/contracts/DexPool.sol/DexPool.json", "utf8"));
const provider = new JsonRpcProvider(configuration.rpcUrl);
const pool = new Contract(configuration.poolAddress, artifact.abi, provider);
const database = new DatabaseSync(".lab3/swaps.sqlite");

database.exec(`
    PRAGMA journal_mode = WAL;
    CREATE TABLE IF NOT EXISTS swaps (
        transaction_hash TEXT PRIMARY KEY,
        block_number INTEGER NOT NULL,
        log_index INTEGER NOT NULL,
        trader TEXT NOT NULL,
        amount_in TEXT NOT NULL,
        amount_out TEXT NOT NULL,
        fee_bps INTEGER NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_swaps_trader ON swaps(trader);
    CREATE TABLE IF NOT EXISTS indexer_state (
        key TEXT PRIMARY KEY,
        value TEXT NOT NULL
    );
`);

const insertSwap = database.prepare(`
    INSERT OR IGNORE INTO swaps (
        transaction_hash, block_number, log_index, trader, amount_in, amount_out, fee_bps
    ) VALUES (?, ?, ?, ?, ?, ?, ?)
`);
const readSwaps = database.prepare(`
    SELECT transaction_hash, block_number, trader, amount_in, amount_out, fee_bps
    FROM swaps
    WHERE trader = ?
    ORDER BY block_number DESC, log_index DESC
`);
const readState = database.prepare("SELECT value FROM indexer_state WHERE key = ?");
const writeState = database.prepare(`
    INSERT INTO indexer_state (key, value) VALUES (?, ?)
    ON CONFLICT(key) DO UPDATE SET value = excluded.value
`);

let polling = false;
let nextBlock = Number(readState.get("next_block")?.value ?? configuration.deploymentBlock);

async function pollEvents() {
    if (polling) {
        return;
    }

    polling = true;
    try {
        const latestBlock = await provider.getBlockNumber();
        if (nextBlock > latestBlock) {
            return;
        }

        const events = await pool.queryFilter(pool.filters.Swap(), nextBlock, latestBlock);
        for (const event of events) {
            insertSwap.run(
                event.transactionHash,
                event.blockNumber,
                event.index,
                event.args.trader.toLowerCase(),
                event.args.amountIn.toString(),
                event.args.amountOut.toString(),
                Number(event.args.feeBps)
            );
            console.log(`Indexed Swap ${event.transactionHash}`);
        }

        nextBlock = latestBlock + 1;
        writeState.run("next_block", String(nextBlock));
    } catch (error) {
        console.error("Indexer error:", error.message);
    } finally {
        polling = false;
    }
}

function sendJson(response, statusCode, value) {
    response.writeHead(statusCode, {
        "Access-Control-Allow-Headers": "Content-Type",
        "Access-Control-Allow-Methods": "GET, OPTIONS",
        "Access-Control-Allow-Origin": "*",
        "Content-Type": "application/json; charset=utf-8"
    });
    response.end(JSON.stringify(value));
}

const server = createServer((request, response) => {
    if (request.method === "OPTIONS") {
        sendJson(response, 204, null);
        return;
    }

    const url = new URL(request.url, "http://127.0.0.1:3000");
    if (request.method === "GET" && url.pathname === "/api/health") {
        sendJson(response, 200, {
            status: "ok",
            poolAddress: configuration.poolAddress,
            indexedTrader: configuration.traderAddress
        });
        return;
    }

    if (request.method === "GET" && url.pathname === "/api/swaps") {
        const trader = url.searchParams.get("trader") ?? "";
        if (!isAddress(trader)) {
            sendJson(response, 400, { error: "Valid trader address is required" });
            return;
        }

        const records = readSwaps.all(trader.toLowerCase()).map((record) => ({
            transactionHash: record.transaction_hash,
            blockNumber: record.block_number,
            trader: record.trader,
            amountIn: formatEther(record.amount_in),
            amountOut: formatEther(record.amount_out),
            feePercent: Number(record.fee_bps) / 100
        }));
        sendJson(response, 200, records);
        return;
    }

    sendJson(response, 404, { error: "Not found" });
});

await pollEvents();
setInterval(pollEvents, 2000);
server.listen(3000, "127.0.0.1", () => {
    console.log("Indexer and REST API: http://127.0.0.1:3000");
    console.log("Indexed trader:", configuration.traderAddress);
});

async function shutdown() {
    server.close();
    database.close();
    await provider.destroy();
    process.exit(0);
}

process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
