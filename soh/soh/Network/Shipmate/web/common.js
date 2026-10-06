// Shared by the pause menu and checklist pages. Pages show only simple statuses;
// the details behind a failure go to the console.
const el = id => document.getElementById(id);

// URL options such as ?toolbar=false; a missing or unrecognized value keeps the default.
function urlFlag(name, fallback = true) {
    const value = new URLSearchParams(location.search).get(name);
    return value === 'true' ? true : value === 'false' ? false : fallback;
}

// Carry URL options across Shipmate's own pages.
function syncLinks() {
    for (const link of document.querySelectorAll('a[href^="/"]')) link.search = location.search;
}
syncLinks();

// Drop a URL option in place, such as one the page's own controls have replaced.
function dropUrlOption(name) {
    const url = new URL(location.href);
    if (!url.searchParams.has(name)) return;
    url.searchParams.delete(name);
    history.replaceState(null, '', url);
    syncLinks();
}

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

// Polls a state URL, passing null while Ship is unreachable. onPoll sees every result; onChange
// sees only results whose select()ed data changed. Returns a function to refresh immediately.
function watchState({ url = '/state', interval = 250, select = state => state, onPoll = () => {}, onChange,
                      paused = () => false }) {
    let previous, failing = false;
    async function refresh() {
        let state = null;
        try {
            ({ state } = await requestJson(url));
            failing = false;
        } catch (error) {
            // Log once per outage rather than on every poll.
            if (!failing) console.warn(`Shipmate ${url}:`, error.message);
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
        setTimeout(poll, interval);
    }
    poll();
    return refresh;
}
