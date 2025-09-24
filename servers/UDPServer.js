
import dgram from "dgram";
import crypto from "crypto";

const HOST = process.env.UDP_HOST || "0.0.0.0";
const PORT = parseInt(process.env.UDP_PORT || "25565", 10);
const HEARTBEAT_TIMEOUT_MS = 15000;

const server = dgram.createSocket("udp4");

// State
// clientKey: `${address}:${port}` -> { clientId, lobbyId, isHost, lastSeen }
const clients = new Map();
// lobbyId -> { hostKey, members: Set<clientKey>, token }
const lobbies = new Map();

function now() { return Date.now(); }
function clientKey(rinfo) { return `${rinfo.address}:${rinfo.port}`; }
function send(buf, port, address) {
	server.send(buf, port, address, (err) => { if (err) console.error(err); });
}
function sendJSON(obj, port, address) {
	try { send(Buffer.from(JSON.stringify(obj)), port, address); } catch (e) { console.error(e); }
}

function cleanupStaleClients() {
	const cutoff = now() - HEARTBEAT_TIMEOUT_MS;
	for (const [key, info] of clients.entries()) {
		if (info.lastSeen < cutoff) {
			// remove from lobby
			if (info.lobbyId && lobbies.has(info.lobbyId)) {
				const lobby = lobbies.get(info.lobbyId);
				lobby.members.delete(key);
				if (lobby.hostKey === key) {
					lobby.hostKey = lobby.members.values().next().value || null;
				}
				if (lobby.members.size === 0) lobbies.delete(info.lobbyId);
			}
			clients.delete(key);
		}
	}
}

setInterval(cleanupStaleClients, 5000);

server.on("message", (msg, rinfo) => {
	const key = clientKey(rinfo);
	let data = null;
	try {
		data = JSON.parse(msg.toString());
	} catch (e) {
		// Treat as opaque game payload: relay based on role
		const c = clients.get(key);
		if (!c) return; // unknown client
		c.lastSeen = now();
		const lobby = lobbies.get(c.lobbyId);
		if (!lobby) return;
		if (c.isHost) {
			// relay to all members except host
			for (const memberKey of lobby.members) {
				if (memberKey === key) continue;
				const [addr, prt] = memberKey.split(":");
				send(msg, Number(prt), addr);
			}
		} else if (lobby.hostKey) {
			const [addr, prt] = lobby.hostKey.split(":");
			send(msg, Number(prt), addr);
		}
		return;
	}

	const { type } = data || {};
	if (!type) { return; }

	switch (type) {
		case 'register': {
			// { type: 'register', lobbyId, token, clientId, isHost }
			const { lobbyId, token, clientId, isHost } = data;
			if (!lobbyId || !token || typeof clientId !== 'number') {
				return sendJSON({ type: 'error', error: 'invalid_register' }, rinfo.port, rinfo.address);
			}
			let lobby = lobbies.get(lobbyId);
			if (!lobby) {
				// First registrant for lobby must be host and sets token
				if (!isHost) return sendJSON({ type: 'error', error: 'lobby_missing_host' }, rinfo.port, rinfo.address);
				lobby = { hostKey: null, members: new Set(), token };
				lobbies.set(lobbyId, lobby);
			} else {
				if (lobby.token !== token) return sendJSON({ type: 'error', error: 'bad_token' }, rinfo.port, rinfo.address);
			}

			clients.set(key, { clientId, lobbyId, isHost: !!isHost, lastSeen: now() });
			lobby.members.add(key);
			if (isHost) lobby.hostKey = key;
			return sendJSON({ type: 'registered', lobbyId, isHost: !!isHost }, rinfo.port, rinfo.address);
		}
		case 'heartbeat': {
			const c = clients.get(key);
			if (c) c.lastSeen = now();
			return sendJSON({ type: 'heartbeat_ack', t: now() }, rinfo.port, rinfo.address);
		}
		case 'set_host': {
			const c = clients.get(key);
			if (!c) return sendJSON({ type: 'error', error: 'not_registered' }, rinfo.port, rinfo.address);
			const lobby = lobbies.get(c.lobbyId);
			if (!lobby) return sendJSON({ type: 'error', error: 'lobby_missing' }, rinfo.port, rinfo.address);
			lobby.hostKey = key;
			c.isHost = true;
			return sendJSON({ type: 'ok' }, rinfo.port, rinfo.address);
		}
		default:
			return sendJSON({ type: 'error', error: 'unknown_type' }, rinfo.port, rinfo.address);
	}
});

server.on("listening", () => {
	const addr = server.address();
	console.log(`UDP server listening on ${addr.address}:${addr.port}`);
});

server.on("close", () => {
	console.log("UDP server closed");
});

server.bind(PORT, HOST);