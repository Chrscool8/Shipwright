const labels = ['C-left', 'C-down', 'C-right', 'D-pad up', 'D-pad down', 'D-pad left', 'D-pad right'];
// Quest Status [x, y, width, height] by cursor point, from the pause menu's quest vertices.
const questRects = [
    [194, 42, 24, 24], [194, 74, 24, 24], [166, 92, 24, 24], [138, 74, 24, 24], [138, 42, 24, 24], [166, 24, 24, 24],
    [14, 102, 12, 20], [32, 102, 12, 20], [50, 102, 12, 20], [68, 102, 12, 20], [86, 102, 12, 20], [104, 102, 12, 20],
    [14, 80, 12, 20], [32, 80, 12, 20], [50, 80, 12, 20], [68, 80, 12, 20], [86, 80, 12, 20], [104, 80, 12, 20],
    [142, 128, 20, 20], [168, 128, 20, 20], [194, 128, 20, 20],
    [12, 24, 20, 20], [36, 24, 20, 20], [12, 48, 20, 20], [68, 24, 44, 44]
];
// Song notes by ocarina button index (A, C-down, C-right, C-left, C-up): texture, tint and staff y.
const noteButtons = [
    ['gOcarinaBtnIconATex', 'note-a', 142], ['gOcarinaBtnIconCDownTex', 'note-down', 136],
    ['gOcarinaBtnIconCRightTex', 'note-right', 129], ['gOcarinaBtnIconCLeftTex', 'note-left', 126],
    ['gOcarinaBtnIconCUpTex', 'note-up', 121]
];
const questTints ={ 6: 'minuet', 7: 'bolero', 8: 'serenade', 9: 'requiem', 10: 'nocturne', 11: 'prelude', 24: 'heart' };
let state = null,
    selected = null,
    selectedButton = null,
    busy = false;

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

function icon(asset, revision = state.assetRevision) {
    const img = document.createElement('img');
    img.src = `/assets/${asset}.png?v=${revision}`;
    img.alt = '';
    img.className = 'icon';
    img.draggable = false;
    img.onerror = () => {
        img.hidden = true;
        el('asset-warning').hidden = false;
    };
    return img;
}

function background(element, url) {
    element.style.backgroundImage = `url('${url}')`;
    // CSS backgrounds have no error event, so load the same URL as an image to catch failures.
    const probe = new Image();
    probe.onerror = () => el('asset-warning').hidden = false;
    probe.src = url;
}

function tint(asset, channel) {
    const img = icon(asset);
    img.style.filter = `url(#tint-${channel})`;
    return img;
}

// Color matrix mapping texel intensity 0 to `from` and 1 to `to`; from black, it is a plain tint.
function lerpMatrix(from, to) {
    const row = c => [0, 1, 2, 3].map(i => i === c ? (to[c] - from[c]) / 255 : 0).join(' ') + ` ${from[c] / 255}`;
    return `${row(0)} ${row(1)} ${row(2)} 0 0 0 1 0`;
}

function updateColors(colors) {
    if (!colors) return;
    for (const [channel, color] of Object.entries(colors)) {
        el(`color-${channel}`)?.setAttribute('values', lerpMatrix([0, 0, 0], color));
    }
    el('color-life').setAttribute('values', lerpMatrix(colors['heart-border'], colors['heart-fill']));
    el('color-life-dd').setAttribute('values', lerpMatrix(colors['dd-border'], colors['dd-fill']));
    el('meters').style.setProperty('--magic', `rgb(${colors.magic})`);
    el('meters').style.setProperty('--magic-infinite', `rgb(${colors['magic-infinite']})`);
}

// Life meter and magic bar in the HUD's pixels (--m): hearts 10px apart in rows of ten at the HUD's
// 0.7 scale, and the magic bar's 8px ends around a 24px tiled middle, filled 3px down for 7px.
function heartTexture(fraction) {
    return fraction === 0 ? 'Full' : fraction < 6 ? 'Quarter' : fraction < 11 ? 'Half' : 'ThreeQuarter';
}

function place(element, x, y, width, height) {
    Object.assign(element.style, {
        left: `calc(${x} * var(--m))`,
        top: `calc(${y} * var(--m))`,
        width: `calc(${width} * var(--m))`,
        height: `calc(${height} * var(--m))`
    });
    return element;
}

let metersSignature;
function renderMeters(next) {
    const meters = next?.loaded ? next.meters : null;
    const signature = JSON.stringify([meters, next?.assetRevision]);
    if (signature === metersSignature) return;
    metersSignature = signature;
    el('meters').replaceChildren();
    el('meters').hidden = !meters;
    if (!meters) return;
    const revision = next.assetRevision;
    const { health, healthCapacity, doubleDefense, magic, magicCapacity, infiniteMagic } = meters;
    // HealthMeter_Draw: the heart after the last full one shows the remaining sixteenths.
    const total = Math.floor(healthCapacity / 16);
    const fraction = health % 16;
    let full = Math.floor(health / 16);
    if (!fraction) full--;
    const hearts = document.createElement('div');
    hearts.className = 'hearts';
    // Sized to the hearts themselves so the rows center over the magic bar.
    hearts.style.width = `calc(${(Math.min(total, 10) - 1) * 10 + 11.2} * var(--m))`;
    hearts.style.height = `calc(${(Math.ceil(total / 10) - 1) * 10 + 11.2} * var(--m))`;
    for (let i = 0; i < total; i++) {
        const kind = i < full ? 'Full' : i === full ? heartTexture(fraction) : 'Empty';
        const heart = icon(`g${doubleDefense ? 'Defense' : ''}Heart${kind}Tex`, revision);
        heart.style.filter = `url(#tint-${doubleDefense ? 'life-dd' : 'life'})`;
        hearts.append(place(heart, (i % 10) * 10, Math.floor(i / 10) * 10, 11.2, 11.2));
    }
    el('meters').append(hearts);
    let label = `Hearts: ${health / 16} of ${total}`;
    if (magicCapacity > 0) {
        const bar = document.createElement('div');
        bar.className = 'magic';
        bar.style.width = `calc(${magicCapacity + 16} * var(--m))`;
        const left = icon('gMagicMeterEndTex', revision);
        const right = icon('gMagicMeterEndTex', revision);
        right.className = 'icon mirrored';
        const middle = document.createElement('div');
        background(middle, `/assets/gMagicMeterMidTex.png?v=${revision}`);
        const fill = document.createElement('div');
        fill.className = `fill${infiniteMagic ? ' infinite' : ''}`;
        bar.append(place(left, 0, 0, 8, 16), place(middle, 8, 0, magicCapacity, 16),
            place(right, magicCapacity + 8, 0, 8, 16), place(fill, 8, 3, magic, 7));
        for (const part of [left, middle, right]) part.style.filter = 'url(#tint-magic-border)';
        el('meters').append(bar);
        label += `. Magic: ${magic} of ${magicCapacity}`;
    }
    el('meters').setAttribute('aria-label', label);
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
            b.className = 'assignment hud-b';
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

function renderQuest() {
    for (const entry of state.quest) {
        const b = document.createElement('div');
        b.tabIndex = 0;
        b.setAttribute('role', 'img');
        const [x, y, width, height] = questRects[entry.point];
        decorateSlot(b, entry, x, y);
        b.style.width = `${width / 240 * 100}%`;
        b.style.height = `${height / 160 * 100}%`;
        const tint = questTints[entry.point];
        if (tint && entry.asset) b.querySelector('.icon').style.filter = `url(#tint-${tint})`;
        // Like the pause menu, only a song under the cursor shows its notes.
        b.onmouseenter = b.onfocus = () => {
            el('quest-name').textContent = entry.name;
            showNotes(entry.notes ?? []);
        };
        el('quest').append(b);
        if (entry.tokens !== undefined) {
            b.setAttribute('aria-label', `${entry.name}, ${entry.tokens}`);
            appendTokens(entry.tokens);
        }
    }
}

function showNotes(notes) {
    el('notes').replaceChildren();
    notes.forEach((button, i) => {
        const [texture, channel, y] = noteButtons[button];
        const note = tint(texture, channel);
        note.className = 'note';
        note.style.left = `${(24 + i * 12) / 240 * 100}%`;
        note.style.top = `${y / 160 * 100}%`;
        el('notes').append(note);
    });
}

function appendTokens(count) {
    // The pause menu's token counter: digit columns at x 30, 37 and 46, leading zeros hidden, red at 100.
    const digits = String(count).padStart(3, ' ');
    [30, 37, 46].forEach((x, i) => {
        if (digits[i] === ' ') return;
        const digit = icon(`gCounterDigit${digits[i]}Tex`);
        digit.className = 'counter-digit';
        digit.classList.toggle('full', count === 100);
        digit.style.left = `${x / 240 * 100}%`;
        el('quest').append(digit);
    });
}

function render() {
    for (const id of ['buttons', 'inventory', 'gear', 'worn', 'quest', 'notes']) el(id).replaceChildren();
    el('unassign').hidden = true;
    el('item-name').textContent = 'Select Item';
    el('gear-name').textContent = 'Equipment';
    el('quest-name').textContent = 'Quest Status';
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
    renderQuest();
}

function setView(view) {
    if (!['items', 'equipment', 'quest'].includes(view)) view = 'items';
    el('panels').dataset.view = view;
    try {
        localStorage.setItem('shipmate-view', view);
    } catch {}
}
for (const b of document.querySelectorAll('.page-switch')) {
    b.onclick = () => {
        const from = el('panels').dataset.view;
        setView(b.dataset.view);
        // Focus the tab that leads back to the previous page.
        document.querySelector(`#${b.dataset.view}-panel .page-switch[data-view="${from}"]`)?.focus();
    };
}
try {
    setView(localStorage.getItem('shipmate-view'));
} catch {
    setView('items');
}
// The toolbar is pinned outside #screen, so mirror the screen's scale onto it.
new ResizeObserver(() => {
    document.querySelector('.toolbar').style.setProperty('--u', getComputedStyle(el('screen')).getPropertyValue('--u'));
}).observe(el('screen'));
// Browsers without the Fullscreen API (such as iPhone Safari) keep the button hidden.
if (document.fullscreenEnabled) {
    const fullscreen = el('fullscreen');
    fullscreen.hidden = false;
    fullscreen.onclick = () => {
        const request = document.fullscreenElement ? document.exitFullscreen() : document.documentElement.requestFullscreen();
        request.catch(error => console.warn('Shipmate fullscreen:', error.message));
    };
    document.onfullscreenchange = () => {
        fullscreen.setAttribute('aria-pressed', String(!!document.fullscreenElement));
    };
}
function setState(next) {
    el('status').textContent = stateStatus(next) ?? (next.canChange ? '' : 'Link is busy');
    if (next && (state?.assetRevision !== next.assetRevision || state?.language !== next.language)) {
        const { assetRevision: revision, language } = next;
        el('asset-warning').hidden = true;
        background(document.querySelector('.pause-panel.items'), `/assets/items-${language}.png?v=${revision}`);
        background(document.querySelector('.pause-panel.equipment'), `/assets/equipment-${language}.png?v=${revision}`);
        background(document.querySelector('.pause-panel.quest'), `/assets/quest-${language}.png?v=${revision}`);
        for (const b of document.querySelectorAll('.page-switch.left')) background(b, `/assets/gLButtonTex.png?v=${revision}`);
        for (const b of document.querySelectorAll('.page-switch.right')) background(b, `/assets/gRButtonTex.png?v=${revision}`);
    }
    state = next;
    render();
}
async function change(payload) {
    busy = true;
    render();
    el('message').textContent = '';
    try {
        await requestJson('/action', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(payload)
        });
        selected = selectedButton = null;
    } catch (error) {
        console.warn('Shipmate action:', error.message);
        el('message').textContent = 'Could not make that change.';
    }
    try {
        await refresh();
    } finally {
        busy = false;
        render();
    }
}
const refresh = watchState({
    // Colors and meters update independently; rainbow colors and health changes should not rebuild the controls.
    select: next => ({ ...next, colors: undefined, meters: undefined }),
    onPoll: next => {
        updateColors(next?.colors);
        renderMeters(next);
    },
    onChange: setState,
    paused: () => busy
});
