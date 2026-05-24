var isOnline = false;
var netplayInstance = null;

(function() {
'use strict';

let p2p = null;

class P2PGameWrapper extends netplayjs.GameWrapper {
	static canvasSize = { width: 720, height: 720 };

	constructor() {
		super(P2PGameWrapper);
		this.game = new Game();
		this.game._status_ = new GameStatus();
		this.conn = null;
		this.localPlayerId = -1;
	}

	startHost(players, conn) {
		this.conn = conn;
		this.localPlayerId = 0;
		netplayInstance = this;
		document.getElementById('onlineBtn').innerText = '游戏中';
		this.conn.on('data', (data) => this._onData(data));
		renderBoard(this.game);
	}

	startClient(players, conn) {
		this.conn = conn;
		this.localPlayerId = 1;
		netplayInstance = this;
		document.getElementById('onlineBtn').innerText = '游戏中';
		this.conn.on('data', (data) => this._onData(data));
		renderBoard(this.game);
	}

	_onData(data) {
		if (data.type === 'move') {
			let inst = new Instruction(OperType.move, data.dir, data.count);
			this.game.OnCmd(inst);
			renderBoard(this.game);
		} else if (data.type === 'undo_request') {
			if (!this._canUndo()) {
				this.conn.send({ type: 'undo_reject' });
				return;
			}
			if (confirm('对方请求悔棋，是否同意？')) {
				this._executeUndo();
				renderBoard(this.game);
				this.conn.send({ type: 'undo_accept' });
			} else {
				this.conn.send({ type: 'undo_reject' });
			}
		} else if (data.type === 'undo_accept') {
			this._executeUndo();
			renderBoard(this.game);
		} else if (data.type === 'undo_reject') {
			alert('对方拒绝了悔棋请求');
		} else if (data.type === 'rematch_request') {
			if (confirm('对方请求再来一局，是否同意？')) {
				this.game.Reset();
				renderBoard(this.game);
				this.conn.send({ type: 'rematch_accept' });
			} else {
				this.conn.send({ type: 'rematch_reject' });
			}
		} else if (data.type === 'rematch_accept') {
			this.game.Reset();
			renderBoard(this.game);
		} else if (data.type === 'rematch_reject') {
			alert('对方拒绝了再来一局的请求');
		} else if (data.type === 'leave') {
			this._exitRoom();
			alert('对方已退出房间');
		}
	}

	sendMove(direction, count) {
		let inst = new Instruction(OperType.move, direction, count);
		this.game.OnCmd(inst);
		renderBoard(this.game);
		if (this.conn) {
			this.conn.send({ type: 'move', dir: direction, count: count });
		}
	}

	_canUndo() {
		let h = this.game._status_.history_;
		let moves = 0;
		for (let i = h.length - 1; i >= 0; i--) {
			if (h[i].oper_type_ === OperType.move) moves++;
			if (moves >= 2) return true;
			if (moves === 1 && h[i].oper_type_ === OperType.pass) return true;
		}
		return false;
	}

	_executeUndo() {
		let hist = this.game._status_.history_;
		let popped = [];
		while (hist.length > 0 && popped.length < 2) {
			let inst = hist.pop();
			if (inst.oper_type_ === OperType.move) {
				popped.push(inst);
			} else if (inst.oper_type_ === OperType.pass && popped.length === 1) {
				popped.push(inst);
			}
		}
		if (popped.length < 2) {
			popped.reverse().forEach(i => hist.push(i));
			return false;
		}
		if (popped[1].oper_type_ === OperType.pass) {
			this.game._status_.redo_.push(popped[0]);
		} else {
			this.game._status_.redo_.push(popped[1], popped[0]);
		}
		let remaining = hist.slice();
		this.game.Reset();
		remaining.forEach(inst => this.game.OnCmd(inst));
		return true;
	}

	requestUndo() {
		if (this.game.CurrPlayer() !== this.localPlayerId) {
			alert('现在不是你的回合，不能悔棋');
			return;
		}
		if (!this._canUndo()) {
			alert('没有可以悔棋的步骤');
			return;
		}
		if (this.conn) {
			this.conn.send({ type: 'undo_request' });
		}
	}

	requestRematch() {
		if (this.conn) {
			this.conn.send({ type: 'rematch_request' });
		}
	}

	_exitRoom() {
		isOnline = false;
		netplayInstance = null;
		if (this.conn) this.conn.close();
		document.getElementById('onlineBtn').style.display = '';
		document.getElementById('onlineBtn').disabled = false;
		document.getElementById('onlineBtn').innerText = '联机对战';
		document.getElementById('exitBtn').style.display = 'none';
		document.getElementById('rematchBtn').style.display = 'none';
		document.getElementById('undoBtn').style.display = '';
		document.getElementById('undoBtn').innerText = '回退';
		document.getElementById('redoBtn').style.display = '';
		document.getElementById('opponent').style.display = 'none';
		this.game.Reset();
		window.game = this.game;
		renderBoard(this.game);
	}

	exitRoom() {
		if (this.game.GetWinner() === -1) {
			if (!confirm('游戏未结束，确定要退出房间吗？')) return;
		}
		if (this.conn) {
			this.conn.send({ type: 'leave' });
		}
		this._exitRoom();
	}
}

window.startOnline = function startOnline() {
	isOnline = true;
	let btn = document.getElementById('onlineBtn');
	btn.disabled = true;
	btn.innerText = '连接中...';
	document.getElementById('redoBtn').style.display = 'none';
	document.getElementById('undoBtn').innerText = '悔棋';
	document.getElementById('exitBtn').style.display = '';
	p2p = new P2PGameWrapper();
	p2p.canvas.style.display = 'none';
	p2p.start();
};

window.exitOnline = function exitOnline() {
	if (netplayInstance) netplayInstance.exitRoom();
};

window.requestRematch = function requestRematch() {
	if (netplayInstance) netplayInstance.requestRematch();
};

})();
