const labels = ['C-left', 'C-down', 'C-right', 'D-pad up', 'D-pad down', 'D-pad left', 'D-pad right'];
const el = id => document.getElementById(id);
let state = null,
    selected = null,
    selectedButton = null,
    busy = false,
    previous = '';

function button(disabled, action) {
    const b = document.createElement('button');
    b.disabled = disabled;
    b.onclick = action;
    return b;
}

function chooseItem(slot, item) {
    const selection = {
        slot,
        item,
        fileNum: state.fileNum,
        age: state.age
    };
    if (selectedButton) {
        change({
            action: 'assign',
            ...selection,
            button: selectedButton.button
        });
    } else {
        selected = selected?.slot === slot ? null : selection;
        render();
    }
}

function chooseButton(index) {
    if (selected) {
        change({
            action: 'assign',
            ...selected,
            button: index
        });
    } else {
        selectedButton = selectedButton?.button === index ? null : {
            button: index,
            item: state.buttons[index + 1].item,
            slot: state.buttons[index + 1].slot,
            fileNum: state.fileNum,
            age: state.age
        };
        render();
    }
}

function icon(asset) {
    const img = document.createElement('img');
    img.src = `/assets/${asset}.png?v=${state.assetRevision}`;
    img.alt = '';
    img.className = 'icon';
    img.draggable = false;
    img.onerror = () => {
        img.hidden = true;
        el('asset-warning').hidden = false;
    };
    return img;
}

function tint(asset, channel) {
    const img = icon(asset);
    img.style.filter = `url(#tint-${channel})`;
    return img;
}

function updateColors(colors) {
    if (!colors) return;
    for (const [channel, [r, g, b]] of Object.entries(colors)) {
        el(`color-${channel}`).setAttribute('values', `${r/255} 0 0 0 0 0 ${g/255} 0 0 0 0 0 ${b/255} 0 0 0 0 0 1 0`);
    }
}

function decorateSlot(b, entry, x, y) {
    b.className = 'slot';
    const caption = entry.name;
    b.title = caption;
    b.setAttribute('aria-label', caption);
    b.style.left = `${x / 240 * 100}%`;
    b.style.top = `${y / 160 * 100}%`;
    if (entry.equipped) {
        const outline = icon('gEquippedItemOutlineTex');
        outline.className = 'equipped';
        b.append(outline);
    }
    if (entry.asset) b.append(icon(entry.asset));
    for (const corner of ['TopLeft', 'TopRight', 'BottomLeft', 'BottomRight']) {
        const cursor = icon(`gPauseMenuCursor${corner}Tex`);
        cursor.className = `cursor ${corner}`;
        b.append(cursor);
    }
}

function showCaption(b, id, text) {
    b.onmouseenter = b.onfocus = () => {
        el(id).textContent = text;
    };
}

function appendAmmo(element, entry, className) {
    if (entry.ammo === null) return;
    const ammo = document.createElement('span');
    ammo.className = className;
    for (const digit of String(entry.ammo)) ammo.append(icon(`gAmmoDigit${digit}Tex`));
    element.append(ammo);
    element.setAttribute('aria-label', `${element.title}, ${entry.ammo}`);
}

function updateSelection() {
    if (selected && (selected.fileNum !== state.fileNum || selected.age !== state.age ||
            state.items[selected.slot]?.item !== selected.item)) selected = null;
    const selectedEntry = selectedButton ? state.buttons[selectedButton.button + 1] : null;
    if (selectedButton && (selectedButton.fileNum !== state.fileNum || selectedButton.age !== state.age ||
            !selectedEntry?.allowed || selectedEntry.item !== selectedButton.item ||
            selectedEntry.slot !== selectedButton.slot)) selectedButton = null;
}

function renderButtons(enabled) {
    const cButtons = document.createElement('div');
    cButtons.className = 'hud-c-buttons';
    const dpad = document.createElement('div');
    dpad.className = 'hud-dpad';
    const cross = tint('hud-dpad', 'dpad');
    cross.className = 'dpad-background';
    dpad.append(cross);
    el('buttons').append(cButtons, dpad);
    const positions = ['left', 'down', 'right', 'up', 'down', 'left', 'right'];
    for (const entry of state.buttons) {
        const i = entry.button;
        const label = i < 0 ? 'B' : labels[i];
        const b = i < 0 ? document.createElement('div') : button(!enabled || !entry.allowed ||
            (selected && !state.items[selected.slot].allowed), () => chooseButton(i));
        let parent;
        if (i < 0) {
            b.className = 'assignment hud-b readonly';
            parent = el('buttons');
        } else if (i < 3) {
            b.className = `assignment hud-c ${positions[i]}`;
            parent = cButtons;
        } else {
            b.className = `assignment hud-direction ${positions[i]}`;
            parent = dpad;
        }
        b.title = `${label}: ${entry.name}`;
        b.setAttribute('aria-label', b.title);
        if (i >= 0) b.setAttribute('aria-pressed', String(selectedButton?.button === i));
        else b.setAttribute('role', 'img');
        if (i < 3) {
            const background = tint('hud-button', i < 0 ? 'b' : positions[i]);
            background.className = 'button-background';
            b.append(background);
        }
        if (!entry.empty) {
            if (entry.asset) {
                const image = icon(entry.asset);
                image.className = 'assigned-icon';
                b.append(image);
            }
            appendAmmo(b, entry, 'hud-ammo');
        } else if (i >= 0 && i < 3) {
            const arrow = tint(`hud-c-${positions[i]}`, positions[i]);
            arrow.className = 'empty-c-arrow';
            b.append(arrow);
        }
        parent.append(b);
    }
    if (selectedButton) {
        const selectedEntry = state.buttons[selectedButton.button + 1];
        el('unassign').hidden = false;
        el('unassign').textContent = `Unassign ${labels[selectedButton.button]}`;
        el('unassign').disabled = !enabled || selectedEntry.empty;
        el('unassign').onclick = () => change({ action: 'unassign', ...selectedButton });
    }
}

function renderInventory(enabled) {
    // Original pause geometry: 28px icons, 32px spacing, six columns by four rows.
    for (const entry of state.items) {
        if (entry.empty) continue;
        const slot = entry.slot;
        const b = button(!entry.allowed || !enabled, () => chooseItem(slot, entry.item));
        decorateSlot(b, entry, 26 + (slot % 6) * 32, 24 + Math.floor(slot / 6) * 32);
        b.classList.toggle('restricted', !entry.allowed);
        b.setAttribute('aria-pressed', String(selected?.slot === slot));
        showCaption(b, 'item-name', entry.name);
        appendAmmo(b, entry, 'ammo');
        el('inventory').append(b);
    }
    if (selected) el('item-name').textContent = state.items[selected.slot].name;
}

function renderEquipment(enabled) {
    for (const entry of state.equipment) {
        const b = button(!enabled || !entry.allowed, () => change({
            action: 'equip',
            category: entry.category,
            value: entry.value,
            fileNum: state.fileNum,
            age: state.age
        }));
        decorateSlot(b, entry, 134 + (entry.value - 1) * 32, 24 + entry.category * 32);
        b.classList.toggle('restricted', !entry.allowed);
        b.setAttribute('aria-pressed', String(entry.equipped));
        showCaption(b, 'gear-name', entry.name);
        el('gear').append(b);
    }
    for (const entry of state.upgrades) {
        const b = document.createElement('div');
        b.tabIndex = 0;
        b.setAttribute('role', 'img');
        decorateSlot(b, entry, 8, 24 + entry.row * 32);
        showCaption(b, 'gear-name', entry.name);
        el('gear').append(b);
    }
}

function renderWorn() {
    const heading = document.createElement('div');
    heading.className = 'worn-title';
    heading.textContent = 'EQUIPPED';
    el('worn').append(heading);
    for (const entry of state.worn) {
        const row = document.createElement('div');
        row.className = 'worn-row';
        if (entry.asset) row.append(icon(entry.asset));
        const text = document.createElement('span');
        text.textContent = entry.name;
        row.append(text);
        el('worn').append(row);
    }
}

function render() {
    for (const id of ['buttons', 'inventory', 'gear', 'worn']) el(id).replaceChildren();
    el('unassign').hidden = true;
    el('item-name').textContent = 'Select Item';
    el('gear-name').textContent = 'Equipment';
    if (!state?.loaded) {
        selected = selectedButton = null;
        return;
    }
    updateSelection();
    const enabled = state.canChange && !busy;
    renderButtons(enabled);
    renderInventory(enabled);
    renderEquipment(enabled);
    renderWorn();
}

function setView(view) {
    if (!['items', 'equipment'].includes(view)) view = 'items';
    el('panels').dataset.view = view;
    try {
        localStorage.setItem('shipmate-view', view);
    } catch {}
}
el('to-equipment').onclick = () => {
    setView('equipment');
    el('to-items').focus();
};
el('to-items').onclick = () => {
    setView('items');
    el('to-equipment').focus();
};
try {
    setView(localStorage.getItem('shipmate-view'));
} catch {
    setView('items');
}
async function requestJson(url, options = {}) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 5000);
    try {
        const response = await fetch(url, { ...options, signal: controller.signal });
        return await response.json();
    } finally {
        clearTimeout(timeout);
    }
}

async function refresh() {
    try {
        const data = await requestJson('/state');
        const next = data.state ?? null;
        el('status').textContent = next?.loaded ? (next.canChange ? '' : 'Link is busy') : data.message ?? 'Load a save in Ship.';
        if (next && state?.assetRevision !== next.assetRevision) {
            const revision = next.assetRevision;
            document.querySelector('.pause-panel.items').style.backgroundImage = `url('/assets/items.png?v=${revision}')`;
            document.querySelector('.pause-panel.equipment').style.backgroundImage = `url('/assets/equipment.png?v=${revision}')`;
            el('to-equipment').style.backgroundImage = `url('/assets/gLButtonTex.png?v=${revision}')`;
            el('to-items').style.backgroundImage = `url('/assets/gRButtonTex.png?v=${revision}')`;
            el('asset-warning').hidden = true;
        }
        updateColors(next?.colors);
        // Color filters update independently; rainbow changes should not rebuild the controls.
        const signature = JSON.stringify(next ? { ...next, colors: undefined } : null);
        state = next;
        if (signature !== previous) {
            previous = signature;
            render();
        }
    } catch {
        state = null;
        previous = '';
        el('status').textContent = 'Shipmate disconnected';
        render();
    }
}
async function change(payload) {
    busy = true;
    render();
    el('message').textContent = '';
    try {
        const data = await requestJson('/action', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(payload)
        });
        el('message').textContent = data.status === 'success' ? '' : data.message ?? data.status;
        if (data.status === 'success') selected = selectedButton = null;
    } catch {
        el('message').textContent = 'Connection lost. Check Ship before retrying.';
    }
    try {
        await refresh();
    } finally {
        busy = false;
        render();
    }
}
async function poll() {
    if (!busy) await refresh();
    setTimeout(poll, 250);
}
poll();