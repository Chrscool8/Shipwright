// The check tracker's areas and rows, as /checks/state reports them.
// Areas the player has opened or closed keep that choice; others start open while incomplete.
const openAreas = new Map();

// Collected and saved outrank skipped, as in the tracker window.
function rowState(check) {
    if (check.status === 'collected' || check.status === 'saved') return check.status;
    return check.skipped ? 'skipped' : check.status;
}

// The mark column's symbol per row state; unchecked rows leave it blank.
const marks = { collected: '✓', saved: '✓', seen: '•', identified: '•', scummed: '•', skipped: '–' };

function area(entry) {
    const details = document.createElement('details');
    const summary = document.createElement('summary');
    const name = document.createElement('span');
    name.textContent = entry.name;
    const count = document.createElement('span');
    count.className = 'count';
    summary.append(name, count);
    details.append(summary);
    if (!entry.revealed) {
        count.textContent = '???';
        return details;
    }
    const complete = entry.gotten === entry.total;
    details.classList.toggle('complete', complete);
    details.open = openAreas.get(entry.name) ?? !complete;
    // Record only clicks; a toggle event would also fire for the default set above.
    summary.addEventListener('click', () => openAreas.set(entry.name, !details.open));
    count.textContent = `${entry.gotten} / ${entry.total}`;
    const list = document.createElement('ul');
    for (const check of entry.checks) {
        const row = document.createElement('li');
        const state = rowState(check);
        row.className = state;
        const mark = document.createElement('span');
        mark.className = 'mark';
        mark.textContent = marks[state] ?? '';
        if (mark.textContent) {
            mark.title = state[0].toUpperCase() + state.slice(1);
            mark.setAttribute('role', 'img');
            mark.setAttribute('aria-label', mark.title);
        }
        const name = document.createElement('span');
        name.className = 'name';
        name.textContent = check.name;
        if (check.item) {
            const item = document.createElement('span');
            item.className = 'item';
            item.textContent = `(${check.item})`;
            name.append(item);
        }
        row.append(mark, name);
        list.append(row);
    }
    details.append(list);
    return details;
}

watchState({
    url: '/checks/state',
    // The full check list is larger than /state, and checks change rarely.
    interval: 1000,
    onChange(state) {
        el('status').textContent = stateStatus(state) ?? `${state.gotten} Checked / ${state.total} Total`;
        // Keep the last list visible while disconnected.
        if (!state) return;
        el('checks').hidden = el('check-tools').hidden = !state.loaded;
        el('checks').replaceChildren(...(state.loaded ? state.areas.map(area) : []));
    }
});

// "Hide done" hides finished rows and complete areas, remembered per browser.
function setHideDone(hide) {
    el('hide-done').checked = hide;
    el('checks').classList.toggle('hide-done', hide);
}
try {
    setHideDone(localStorage.getItem('shipmate-hide-done') === 'true');
} catch {}
el('hide-done').onchange = () => {
    setHideDone(el('hide-done').checked);
    try {
        localStorage.setItem('shipmate-hide-done', String(el('hide-done').checked));
    } catch {}
};
