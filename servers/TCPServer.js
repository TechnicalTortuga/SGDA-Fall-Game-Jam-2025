// server.js
//
// Very small signaling server for GameNetworkingSockets P2P connections.
// Purpose: exchange identity blobs between two peers in the same lobby.
//
// Usage:
//   node server.js
// Then connect peers to ws://<server_ip>:8080 and send:
//   { "lobbyId": "my-lobby", "identity": "<identity-string>" }

const WebSocket = require('ws');
const crypto = require('crypto');

const PORT = process.env.PORT || 25575;
const UDP_HOST = process.env.UDP_HOST || '0.0.0.0';
const UDP_PORT = parseInt(process.env.UDP_PORT || '25565', 10);

const wss = new WebSocket.Server({ port: PORT });
console.log(`Signaling server listening on ws://0.0.0.0:${PORT}`);

// State
let nextClientId = 1;
const clients = new Map(); // ws -> { clientId, lobbyId }
const lobbies = new Map(); // lobbyId -> { lobbyId, lobbyName, hostClientId, clients: Map<clientId, ws> }

function send(ws, obj) {
	try {
		ws.send(JSON.stringify(obj));
	} catch (err) {
		console.error('Failed to send message:', err);
	}
}

function broadcastToLobby(lobby, obj) {
	for (const [, memberWs] of lobby.clients.entries()) {
		send(memberWs, obj);
	}
}

function lobbySnapshot(lobby) {
	return {
		lobbyId: lobby.lobbyId,
		lobbyName: lobby.lobbyName,
		hostClientId: lobby.hostClientId,
		players: Array.from(lobby.clients.keys()),
	};
}

function ensureInLobby(ws, lobbyId) {
	const client = clients.get(ws);
	if (!client || client.lobbyId !== lobbyId) {
		send(ws, { type: 'error', error: 'Not in specified lobby' });
		return false;
	}
	return true;
}

wss.on('connection', ws => {
	const clientId = nextClientId++;
	clients.set(ws, { clientId, lobbyId: null });
	send(ws, { type: 'hello', clientId });

	ws.on('message', raw => {
		let data;
		try {
			data = JSON.parse(raw);
		} catch (err) {
			console.error('Invalid JSON from client:', err);
			send(ws, { type: 'error', error: 'invalid_json' });
			return;
		}

		const { type } = data || {};
		if (!type) {
			send(ws, { type: 'error', error: 'missing_type' });
			return;
		}

		switch (type) {
			case 'create_lobby': {
				const lobbyName = (data && data.lobbyName) || `Lobby-${crypto.randomBytes(3).toString('hex')}`;
				const lobbyId = (data && data.lobbyId) || crypto.randomBytes(4).toString('hex');
				if (lobbies.has(lobbyId)) {
					send(ws, { type: 'error', error: 'lobby_exists' });
					return;
				}
				const lobby = {
					lobbyId,
					lobbyName,
					hostClientId: clients.get(ws).clientId,
					clients: new Map(),
				};
				lobby.clients.set(clients.get(ws).clientId, ws);
				lobbies.set(lobbyId, lobby);
				clients.get(ws).lobbyId = lobbyId;
				send(ws, { type: 'lobby_created', ...lobbySnapshot(lobby) });
				break;
			}
			case 'join_lobby': {
				const lobbyId = data && data.lobbyId;
				if (!lobbyId || !lobbies.has(lobbyId)) {
					send(ws, { type: 'error', error: 'lobby_not_found' });
					return;
				}
				const lobby = lobbies.get(lobbyId);
				lobby.clients.set(clients.get(ws).clientId, ws);
				clients.get(ws).lobbyId = lobbyId;
				send(ws, { type: 'lobby_joined', ...lobbySnapshot(lobby) });
				broadcastToLobby(lobby, { type: 'lobby_updated', ...lobbySnapshot(lobby) });
				break;
			}
			case 'leave_lobby': {
				const lobbyId = data && data.lobbyId;
				if (!lobbyId || !ensureInLobby(ws, lobbyId)) return;
				const lobby = lobbies.get(lobbyId);
				lobby.clients.delete(clients.get(ws).clientId);
				clients.get(ws).lobbyId = null;
				send(ws, { type: 'lobby_left', lobbyId });
				if (lobby.clients.size === 0) {
					lobbies.delete(lobbyId);
				} else {
					if (!Array.from(lobby.clients.keys()).includes(lobby.hostClientId)) {
						// Reassign host to smallest clientId
						lobby.hostClientId = Math.min(...Array.from(lobby.clients.keys()));
					}
					broadcastToLobby(lobby, { type: 'lobby_updated', ...lobbySnapshot(lobby) });
				}
				break;
			}
			case 'request_udp_handoff': {
				const lobbyId = data && data.lobbyId;
				if (!lobbyId || !ensureInLobby(ws, lobbyId)) return;
				const lobby = lobbies.get(lobbyId);
				if (!lobby) { send(ws, { type: 'error', error: 'lobby_not_found' }); return; }
				const token = crypto.randomBytes(8).toString('hex');
				broadcastToLobby(lobby, {
					type: 'udp_handoff',
					lobbyId,
					udpAddress: UDP_HOST,
					udpPort: UDP_PORT,
					token
				});
				break;
			}
			default:
				send(ws, { type: 'error', error: 'unknown_type' });
		}
	});

	ws.on('close', () => {
		const client = clients.get(ws);
		if (!client) return;
		const { clientId, lobbyId } = client;
		clients.delete(ws);
		if (lobbyId && lobbies.has(lobbyId)) {
			const lobby = lobbies.get(lobbyId);
			lobby.clients.delete(clientId);
			if (lobby.clients.size === 0) {
				lobbies.delete(lobbyId);
			} else {
				if (!Array.from(lobby.clients.keys()).includes(lobby.hostClientId)) {
					lobby.hostClientId = Math.min(...Array.from(lobby.clients.keys()));
				}
				broadcastToLobby(lobby, { type: 'lobby_updated', ...lobbySnapshot(lobby) });
			}
		}
	});
});