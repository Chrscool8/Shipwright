// Shared by the pause menu and checklist pages. Pages show only simple statuses;
// the details behind a failure go to the console.
const el = id => document.getElementById(id);

async function requestJson(url, options = {}) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(new Error(`${url} timed out`)), 5000);
    try {
        const response = await fetch(url, { cache: 'no-store', ...options, signal: controller.signal });
        const data = await response.json().catch(() => ({}));
        if (!response.ok || data.status !== 'success') {
            throw new Error(data.message ?? `${url} returned HTTP ${response.status}`);
        }
        return data;
    } finally {
        clearTimeout(timeout);
    }
}

// Common status text; null when a save is loaded, so each page can word that state itself.
function stateStatus(state) {
    if (!state) return 'Disconnected';
    return state.loaded ? null : 'Load a save in Ship.';
}

// Polls /state, passing null while Ship is unreachable. onPoll sees every result; onChange
// sees only results whose select()ed data changed. Returns a function to refresh immediately.
function watchState({ select = state => state, onPoll = () => {}, onChange, paused = () => false }) {
    let previous, failing = false;
    async function refresh() {
        let state = null;
        try {
            ({ state } = await requestJson('/state'));
            failing = false;
        } catch (error) {
            // Log once per outage rather than on every poll.
            if (!failing) console.warn('Shipmate state:', error.message);
            failing = true;
        }
        onPoll(state);
        const signature = JSON.stringify(state && select(state));
        if (signature !== previous) {
            previous = signature;
            onChange(state);
        }
    }
    async function poll() {
        // Hidden tabs skip refreshes to spare the game thread.
        if (!document.hidden && !paused()) await refresh();
        setTimeout(poll, 250);
    }
    poll();
    return refresh;
}
