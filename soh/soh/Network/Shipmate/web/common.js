// Shared by every Shipmate page. Pages show only simple statuses;
// the details behind a failure go to the console.
const el = id => document.getElementById(id);

// URL options such as ?toolbar=false; a missing or unrecognized value keeps the default.
function urlFlag(name, fallback = true) {
    const value = new URLSearchParams(location.search).get(name);
    return value === 'true' ? true : value === 'false' ? false : fallback;
}

// The corner buttons and the sidebar of pages (styled by nav.css). ?toolbar=false hides the corner.
function buildMenu() {
    const icon = path => `<svg viewBox="0 0 24 24" aria-hidden="true"><path d="${path}" /></svg>`;
    const pages = [
        ['/', 'Pause Menu', 'M4 4h7v7H4zM13 4h7v7h-7zM4 13h7v7H4zM13 13h7v7h-7z'],
        ['/checks', 'Checks', 'M12 21s-6-5.5-6-11a6 6 0 1 1 12 0c0 5.5-6 11-6 11zM12 12.5a2.5 2.5 0 1 0 0-5 2.5 2.5 0 0 0 0 5'],
        ['/buddy', 'Buddy', 'M6 4l3.5 3h5L18 4v7a6 6 0 0 1-12 0zM10 11h.01M14 11h.01M11 14.5h2'],
        ['/checklist', 'Checklist', 'M4 6l1.5 1.5L8 5M4 12l1.5 1.5L8 11M4 18l1.5 1.5L8 17M11 6h9M11 12h9M11 18h9']
    ];
    document.body.insertAdjacentHTML('afterbegin', `
    <nav class="toolbar" aria-label="Shipmate">
        <button id="menu" class="tool" type="button" aria-haspopup="dialog" aria-label="Menu" title="Menu">${icon('M4 6h16M4 12h16M4 18h16')}</button>
        <button id="fullscreen" class="tool" type="button" aria-pressed="false" aria-label="Fullscreen" title="Fullscreen" hidden>
            <svg class="enter" viewBox="0 0 24 24" aria-hidden="true"><path d="M4 9V4h5M15 4h5v5M20 15v5h-5M9 20H4v-5" /></svg>
            <svg class="exit" viewBox="0 0 24 24" aria-hidden="true"><path d="M9 4v5H4M15 4v5h5M15 20v-5h5M9 20v-5H4" /></svg>
        </button>
    </nav>
    <dialog id="sidebar" class="sidebar" aria-label="Shipmate menu">
        <header><h2>Shipmate</h2><button class="close" type="button" aria-label="Close menu" title="Close">${icon('M6 6l12 12M18 6L6 18')}</button></header>
        <h3>Pages</h3>
        <ul>${pages.map(([href, label, path]) => `<li><a class="item" href="${href}"${href === location.pathname ? ' aria-current="page"' : ''}>${icon(path)}${label}</a></li>`).join('')}</ul>
    </dialog>`);
    document.body.classList.toggle('no-toolbar', !urlFlag('toolbar'));
    const sidebar = el('sidebar');
    el('menu').onclick = () => sidebar.showModal();
    sidebar.querySelector('.close').onclick = () => sidebar.close();
    // Clicks on the backdrop land on the dialog itself, outside its box.
    sidebar.addEventListener('click', ({ target, clientX: x, clientY: y }) => {
        const box = sidebar.getBoundingClientRect();
        if (target === sidebar && (x < box.left || x > box.right || y < box.top || y > box.bottom)) sidebar.close();
    });
}
buildMenu();

// Browsers without the Fullscreen API (such as iPhone Safari) keep the button hidden.
if (document.fullscreenEnabled) {
    const fullscreen = el('fullscreen');
    fullscreen.hidden = false;
    fullscreen.onclick = () => {
        const request = document.fullscreenElement ? document.exitFullscreen() : document.documentElement.requestFullscreen();
        request.catch(error => console.warn('Shipmate fullscreen:', error.message));
    };
    document.addEventListener('fullscreenchange', () => {
        fullscreen.setAttribute('aria-pressed', String(!!document.fullscreenElement));
    });
}

// Adds a titled group of buttons to the sidebar, below the pages, returning its list.
function menuSection(title) {
    const list = document.createElement('ul');
    const heading = document.createElement('h3');
    heading.textContent = title;
    el('sidebar').append(heading, list);
    return list;
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
