// The check tracker's areas and rows, as /checks/state reports them.
// Areas the player has opened or closed keep that choice; others start open while incomplete.
const openAreas = new Map();

// Collected and saved outrank skipped, as in the tracker window.
function rowState(check) {
    if (check.status === 'collected' || check.status === 'saved') return check.status;
    return check.skipped ? 'skipped' : check.status;
}

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
        row.className = rowState(check);
        row.textContent = check.name;
        if (check.item) {
            const item = document.createElement('span');
            item.className = 'item';
            item.textContent = `(${check.item})`;
            row.append(item);
        }
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
        el('checks').hidden = !state.loaded;
        el('checks').replaceChildren(...(state.loaded ? state.areas.map(area) : []));
    }
});
