// Picks the following dog's model, speed and party, as /buddy/state reports them.
let saving = false, dragging = false;
let party = [], partyMax = 0, names = new Map();

// Actions report why they failed (such as no save loaded); settings just say they couldn't save.
async function save(url, body, savedText, { showError = false } = {}) {
    saving = true;
    el('status').textContent = 'Saving…';
    try {
        await requestJson(url, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
        el('status').textContent = savedText;
    } catch (error) {
        console.warn(`Shipmate ${url}:`, error.message);
        el('status').textContent = showError ? error.message : 'Could not save';
    } finally {
        saving = false;
        refresh();
    }
}

const select = id => save('/buddy/select', { model: id }, 'Saved — shows up next spawn');

const act = (action, doneText) => save('/buddy/action', { action }, doneText, { showError: true });
el('summon').addEventListener('click', () => act('summon', 'Here comes your buddy'));
el('dismiss').addEventListener('click', () => act('dismiss', 'Sent your buddy home'));

// Summoning over a current buddy swaps it for the selected one.
function showFollowing(following) {
    el('summon').textContent = following ? 'Swap now' : 'Summon';
    el('dismiss').disabled = !following;
}

const speedText = value => `${Number(value).toFixed(2)}x`;
el('speed').addEventListener('input', () => {
    dragging = true;
    el('speed-value').textContent = speedText(el('speed').value);
});
// Save once the slider is let go rather than on every step.
el('speed').addEventListener('change', () => {
    dragging = false;
    save('/buddy/speed', { speed: Number(el('speed').value) }, 'Saved speed');
});

// The conga gap works like the speed slider: saved once let go, left alone while dragging.
let draggingGap = false;
el('gap').addEventListener('input', () => {
    draggingGap = true;
    el('gap-value').textContent = el('gap').value;
});
el('gap').addEventListener('change', () => {
    draggingGap = false;
    save('/buddy/party', { gap: Number(el('gap').value) }, 'Saved conga gap');
});

function showGap(gap) {
    if (draggingGap) return;
    el('gap').min = gap.min;
    el('gap').max = gap.max;
    el('gap').value = gap.value;
    el('gap-value').textContent = Math.round(gap.value);
}

function showSpeed(speed) {
    if (dragging) return;
    el('speed').min = speed.min;
    el('speed').max = speed.max;
    el('speed').value = speed.value;
    el('speed-value').textContent = speedText(speed.value);
}

const PARTY_SAVED = 'Saved party — Swap now to bring them';
const saveParty = newParty => save('/buddy/party', { party: newParty }, PARTY_SAVED);

function addToParty(id) {
    if (party.length >= partyMax) {
        el('status').textContent = 'The party is full';
        return;
    }
    saveParty([...party, id]);
}

el('party-clear').addEventListener('click', () => saveParty([]));

// A full party of random buddies, brought in right away
el('party-chaos').addEventListener('click', async () => {
    const ids = [...names.keys()];
    const chaos = Array.from({ length: partyMax }, () => ids[Math.floor(Math.random() * ids.length)]);
    await saveParty(chaos);
    act('summon', 'Chaos incoming!');
});
for (const input of document.querySelectorAll('input[name="formation"]')) {
    input.addEventListener('change', () => save('/buddy/party', { formation: Number(input.value) }, 'Changing formation…'));
}

function showParty(state) {
    party = state.party;
    partyMax = state.partyMax;
    el('party-count').textContent = `(${party.length}/${partyMax})`;
    el('party-empty').hidden = party.length > 0;
    el('party-clear').disabled = party.length === 0;
    for (const input of document.querySelectorAll('input[name="formation"]')) {
        input.checked = Number(input.value) === state.formation;
    }
    el('party').replaceChildren(...party.map((id, index) => {
        const item = document.createElement('li');
        const remove = document.createElement('button');
        remove.type = 'button';
        remove.textContent = '×';
        remove.ariaLabel = `Remove ${names.get(id)} from the party`;
        remove.addEventListener('click', () => saveParty(party.filter((_, i) => i !== index)));
        item.append(names.get(id) ?? `#${id}`, remove);
        return item;
    }));
}

function option(model, selected) {
    const label = document.createElement('label');
    const input = document.createElement('input');
    input.type = 'radio';
    input.name = 'buddy';
    input.value = model.id;
    input.checked = model.id === selected;
    input.addEventListener('change', () => select(model.id));
    const name = document.createElement('span');
    name.textContent = model.name;
    const add = document.createElement('button');
    add.type = 'button';
    add.className = 'add';
    add.textContent = '+';
    add.title = 'Add to the party';
    add.ariaLabel = `Add ${model.name} to the party`;
    add.disabled = party.length >= partyMax;
    add.addEventListener('click', event => {
        // Adding shouldn't also pick this buddy as the main one
        event.preventDefault();
        event.stopPropagation();
        addToParty(model.id);
    });
    label.append(input, name, add);
    return label;
}

// One section per group, in the order the groups first appear.
function groups(models, selected) {
    const sections = new Map();
    for (const model of models) {
        if (!sections.has(model.group)) {
            const section = document.createElement('section');
            const heading = document.createElement('h2');
            heading.textContent = model.group;
            const grid = document.createElement('div');
            grid.className = 'grid';
            section.append(heading, grid);
            sections.set(model.group, section);
        }
        sections.get(model.group).lastChild.append(option(model, selected));
    }
    return [...sections.values()];
}

const refresh = watchState({
    url: '/buddy/state',
    interval: 1000,
    // Don't redraw under a pick that is still saving.
    paused: () => saving,
    onChange(state) {
        if (!state) {
            el('status').textContent = 'Disconnected';
            return;
        }
        if (el('status').textContent === 'Connecting…' || el('status').textContent === 'Disconnected') {
            el('status').textContent = 'Pick a buddy';
        }
        el('buddy').hidden = false;
        showSpeed(state.speed);
        showFollowing(state.following);
        names = new Map(state.models.map(model => [model.id, model.name]));
        showParty(state);
        showGap(state.gap);
        el('models').replaceChildren(...groups(state.models, state.selected));
    }
});
