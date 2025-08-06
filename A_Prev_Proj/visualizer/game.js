// Fetches and renders the game state periodically
let intervalId = null;
let previousBoard = [];

async function fetchGameState() {
    try {
        const res = await fetch('/game_state.json?_=' + new Date().getTime());
        if (!res.ok) throw new Error("Failed to fetch game state");
        const state = await res.json();
        renderGameBoard(state);
    } catch (err) {
        console.error("Error fetching game state:", err);
    }
}

function renderGameBoard(state) {
    const board = document.getElementById('gameBoard');
    board.innerHTML = '';

    // HUD panel
    let hud = document.getElementById('hudPanel');
    if (!hud) {
        hud = document.createElement('div');
        hud.id = 'hudPanel';
        hud.style.position = 'absolute';
        hud.style.right = '30px';
        hud.style.top = '100px';
        hud.style.width = '200px';
        hud.style.background = '#2c2c2c';
        hud.style.padding = '1em';
        hud.style.color = 'white';
        hud.style.borderRadius = '12px';
        hud.style.boxShadow = '0 0 10px rgba(0,0,0,0.5)';
        document.body.appendChild(hud);
    }
    hud.innerHTML = `
        <h3>Game Stats</h3>
        <p>🟦 Player 1 Tanks: ${state.player1Tanks ?? '?'}</p>
        <p>🟩 Player 2 Tanks: ${state.player2Tanks ?? '?'}</p>
    `;


    // Update turn counter
    let counter = document.getElementById('turnCounter');
    if (!counter) {
        counter = document.createElement('div');
        counter.id = 'turnCounter';
        counter.style.fontSize = '20px';
        counter.style.fontWeight = 'bold';
        counter.style.marginBottom = '10px';
        board.parentElement.insertBefore(counter, board);
    }

    // Render progress bar
    let turnBar = document.getElementById('turnBar');
    if (!turnBar) {
        turnBar = document.createElement('progress');
        turnBar.id = 'turnBar';
        turnBar.style.width = '60%';
        turnBar.style.height = '1.5em';
        turnBar.style.marginBottom = '10px';
        board.parentElement.insertBefore(turnBar, counter);
    }

    let turnLabel = document.getElementById('turnLabel');
    if (!turnLabel) {
        turnLabel = document.createElement('div');
        turnLabel.id = 'turnLabel';
        turnLabel.style.marginBottom = '10px';
        board.parentElement.insertBefore(turnLabel, counter.nextSibling);
    }
    turnLabel.textContent = `Turn ${state.turn} / ${state.maxSteps - 1}`;

    // Set max dynamically based on game state
    turnBar.max = state.maxSteps-1 || 100; // fallback for safety
    turnBar.value = state.turn ?? 0;

    // Render board
    state.board.forEach((row, y) => {
        const rowDiv = document.createElement('div');
        rowDiv.className = 'row';
        row.forEach((cell, x) => {
            const cellDiv = document.createElement('div');
            cellDiv.className = 'cell';

            if (cell === '1') cellDiv.classList.add('tank1');
            else if (cell === '2') cellDiv.classList.add('tank2');
            else if (cell === '@') cellDiv.classList.add('mine');
            else if (cell === '#') cellDiv.classList.add('wall');
            else if (cell === '$') cellDiv.classList.add('weak');
            else if (cell === '*') cellDiv.classList.add('shell');
            else cellDiv.classList.add('empty');

            cellDiv.title = {
                '1': 'Player 1 Tank',
                '2': 'Player 2 Tank',
                '@': 'Mine',
                '#': 'Wall',
                '$': 'Damaged Wall',
                '*': 'Shell'
            }[cell] || 'Empty';

            // Optional emoji representation
            cellDiv.textContent = {
                '1': '🟦',
                '2': '🟩',
                '#': '⬛',
                '$': '🧱',
                '@': '⛳️',
                '*': '💥',
            }[cell] || '';

            // Highlight if changed
            if (previousBoard[y]?.[x] !== cell) {
                cellDiv.classList.add('changed');
                setTimeout(() => cellDiv.classList.remove('changed'), 300);
            }

            rowDiv.appendChild(cellDiv);
        });
        board.appendChild(rowDiv);
    });
    previousBoard = state.board;

    // Winner display
    let winnerDisplay = document.getElementById('winnerDisplay');
    if (!winnerDisplay) {
        winnerDisplay = document.createElement('div');
        winnerDisplay.id = 'winnerDisplay';
        winnerDisplay.style.fontSize = '24px';
        winnerDisplay.style.fontWeight = 'bold';
        winnerDisplay.style.marginTop = '1em';
        winnerDisplay.style.color = 'gold';
        winnerDisplay.style.textShadow = '0 0 10px gold';
        board.parentElement.appendChild(winnerDisplay);
    }

    if (state.gameOver && state.winner) {
        winnerDisplay.textContent = `🏆 ${state.winner}`;

        if (autoPlaying) {
            clearInterval(intervalId);
            setTimeout(() => {
                fetch('/reset', { method: 'POST' }).then(() => {
                    fetchGameState();
                    intervalId = setInterval(() => {
                        fetch('/step', { method: 'POST' }).then(() => fetchGameState());
                    }, 800);
                });
            }, 2000); // 2s pause before restarting
        }

    } else {
        winnerDisplay.textContent = '';
    }
}

function stepForward() {
    fetch('/step', { method: 'POST' })
        .then(() => fetchGameState());
}

window.onload = () => {
    const controls = document.createElement('div');
    controls.style.marginTop = '1em';
    controls.style.display = 'flex';
    controls.style.gap = '1em';
    controls.style.justifyContent = 'center';
    controls.style.alignItems = 'center';
    controls.style.flexWrap = 'wrap';

    const stepBtn = document.createElement('button');
    stepBtn.textContent = '⏭ Step';
    stepBtn.onclick = stepForward;
    stepBtn.style.padding = '10px 20px';
    stepBtn.style.fontSize = '18px';
    stepBtn.style.fontWeight = 'bold';
    stepBtn.style.border = 'none';
    stepBtn.style.borderRadius = '8px';
    stepBtn.style.backgroundColor = '#007bff';
    stepBtn.style.color = 'white';
    stepBtn.style.cursor = 'pointer';
    stepBtn.style.boxShadow = '0 4px 8px rgba(0, 0, 0, 0.2)';
    stepBtn.onmouseover = () => stepBtn.style.backgroundColor = '#0056b3';
    stepBtn.onmouseout = () => stepBtn.style.backgroundColor = '#007bff';

    const autoBtn = document.createElement('button');
    autoBtn.textContent = '▶ Auto';
    autoBtn.style.padding = stepBtn.style.padding;
    autoBtn.style.fontSize = stepBtn.style.fontSize;
    autoBtn.style.fontWeight = stepBtn.style.fontWeight;
    autoBtn.style.border = stepBtn.style.border;
    autoBtn.style.borderRadius = stepBtn.style.borderRadius;
    autoBtn.style.backgroundColor = '#28a745';
    autoBtn.style.color = 'white';
    autoBtn.style.cursor = 'pointer';
    autoBtn.style.boxShadow = stepBtn.style.boxShadow;
    autoBtn.onmouseover = () => autoBtn.style.backgroundColor = '#1e7e34';
    autoBtn.onmouseout = () => autoBtn.style.backgroundColor = '#28a745';

    let autoPlaying = false;
    autoBtn.onclick = () => {
        autoPlaying = !autoPlaying;
        autoBtn.textContent = autoPlaying ? '⏹ Stop' : '▶ Auto';
        if (autoPlaying) {
            intervalId = setInterval(() => {
                fetch('/step', { method: 'POST' })
                    .then(() => fetchGameState());
            }, 800); // Adjust speed as desired
        } else {
            clearInterval(intervalId);
        }
    };

    controls.appendChild(stepBtn);
    controls.appendChild(autoBtn);
    document.body.appendChild(controls);

    fetchGameState();
};
