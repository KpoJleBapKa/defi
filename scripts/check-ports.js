import { connect } from "node:net";

const ports = [8545, 3000, 5173];

function isPortUsed(port) {
    return new Promise((resolve) => {
        const socket = connect({ host: "127.0.0.1", port });
        socket.setTimeout(500);
        socket.once("connect", () => {
            socket.destroy();
            resolve(true);
        });
        socket.once("error", () => resolve(false));
        socket.once("timeout", () => {
            socket.destroy();
            resolve(false);
        });
    });
}

const usedPorts = [];
for (const port of ports) {
    if (await isPortUsed(port)) {
        usedPorts.push(port);
    }
}

if (usedPorts.length > 0) {
    console.error(`Ports already in use: ${usedPorts.join(", ")}`);
    console.error("Run stop-lab3.cmd and start the laboratory again.");
    process.exit(1);
}
