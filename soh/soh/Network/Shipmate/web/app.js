const names = ['Deku stick', 'Deku nut', 'Bomb', 'Bow', 'Fire arrows', "Din’s Fire", 'Slingshot', 'Fairy Ocarina', 'Ocarina of Time', 'Bombchu', 'Hookshot', 'Longshot', 'Ice arrows', "Farore’s Wind", 'Boomerang', 'Lens of Truth', 'Magic bean', 'Hammer', 'Light arrows', "Nayru’s Love", 'Empty bottle', 'Red potion', 'Green potion', 'Blue potion', 'Fairy', 'Fish', 'Milk', "Ruto’s letter", 'Blue fire', 'Bugs', 'Big Poe', 'Half milk', 'Poe', 'Weird egg', 'Cucco', "Zelda’s letter", 'Keaton Mask', 'Skull Mask', 'Spooky Mask', 'Bunny Hood', 'Goron Mask', 'Zora Mask', 'Gerudo Mask', 'Mask of Truth', 'Sold out', 'Pocket egg', 'Pocket Cucco', 'Cojiro', 'Odd mushroom', 'Odd potion', "Poacher’s saw", 'Broken Goron sword', 'Prescription', 'Eyeball frog', 'Eye drops', 'Claim check', 'Bow + fire arrows', 'Bow + ice arrows', 'Bow + light arrows', 'Kokiri Sword', 'Master Sword', 'Biggoron Sword', 'Deku Shield', 'Hylian Shield', 'Mirror Shield', 'Kokiri Tunic', 'Goron Tunic', 'Zora Tunic', 'Kokiri Boots', 'Iron Boots', 'Hover Boots'];
const labels = ['C-left', 'C-down', 'C-right', 'D-pad up', 'D-pad down', 'D-pad left', 'D-pad right'];
const bootsNames = ['Kokiri Boots', 'Iron Boots', 'Hover Boots'];
const gearNames = [
    ['None', 'Kokiri Sword', 'Master Sword', 'Biggoron Sword'],
    ['None', 'Deku Shield', 'Hylian Shield', 'Mirror Shield'],
    ['None', 'Kokiri Tunic', 'Goron Tunic', 'Zora Tunic'],
    ['None', ...bootsNames]
];
const name = item => item === 255 ? 'Empty' : item === 85 ? 'Broken Giant’s Knife' :
    item === 61 ? (state?.biggoronSword ? 'Biggoron Sword' : 'Giant’s Knife') : (names[item] ?? `Item ${item}`);
const el = id => document.getElementById(id);
let state = null,
    selected = null,
    selectedButton = null,
    busy = false,
    previous = '';

function button(text, disabled, action) {
    const b = document.createElement('button');
    b.textContent = text;
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
            item: state.buttonItems[index + 1],
            slot: state.buttonSlots[index],
            fileNum: state.fileNum,
            age: state.age
        };
        render();
    }
}

function icon(item, asset) {
    const img = document.createElement('img');
    img.src = `/assets/${asset ?? `item-${item}`}.png?v=${state?.assetRevision ?? 0}`;
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
    const img = icon(null, asset);
    img.style.filter = `url(#tint-${channel})`;
    return img;
}

function updateColors(colors) {
    if (!colors) return;
    for (const [channel, [r, g, b]] of Object.entries(colors)) {
        el(`color-${channel}`).setAttribute('values', `${r/255} 0 0 0 0 0 ${g/255} 0 0 0 0 0 ${b/255} 0 0 0 0 0 1 0`);
    }
}

function decorateSlot(b, item, x, y, caption, equipped = false, asset) {
    b.className = 'slot';
    b.title = caption;
    b.setAttribute('aria-label', caption);
    b.style.left = `${x / 240 * 100}%`;
    b.style.top = `${y / 160 * 100}%`;
    if (equipped) {
        const outline = icon(null, 'gEquippedItemOutlineTex');
        outline.className = 'equipped';
        b.append(outline);
    }
    b.append(icon(item, asset));
    for (const corner of ['TopLeft', 'TopRight', 'BottomLeft', 'BottomRight']) {
        const cursor = icon(null, `gPauseMenuCursor${corner}Tex`);
        cursor.className = `cursor ${corner}`;
        b.append(cursor);
    }
}

function showCaption(b, id, text) {
    b.onmouseenter = b.onfocus = () => {
        el(id).textContent = text;
    };
}

function render() {
    for (const id of ['buttons', 'inventory', 'gear', 'worn']) el(id).replaceChildren();
    el('unassign').hidden = true;
    el('item-name').textContent = 'Select Item';
    el('gear-name').textContent = 'Equipment';
    if (!state?.loaded) {
        selected = selectedButton = null;
        el('selection').textContent = 'Load a save in Ship.';
        return;
    }
    if (selected && (selected.fileNum !== state.fileNum || selected.age !== state.age || state.items[selected.slot] !== selected.item)) selected = null;
    if (selectedButton && (selectedButton.fileNum !== state.fileNum || selectedButton.age !== state.age ||
            state.buttonItems[selectedButton.button + 1] !== selectedButton.item ||
            state.buttonSlots[selectedButton.button] !== selectedButton.slot ||
            (selectedButton.button >= 3 && !state.dpadEnabled))) selectedButton = null;
    const enabled = state.canChange && !busy;
    el('selection').textContent = selected ? `Selected: ${name(selected.item)}. Choose a button above.` : selectedButton ? `Selected: ${labels[selectedButton.button]}. Choose an item or Unassign. Click again to cancel.` : 'Select an item and a button, in either order.';
    const cButtons = document.createElement('div');
    cButtons.className = 'hud-c-buttons';
    const dpad = document.createElement('div');
    dpad.className = 'hud-dpad';
    const cross = tint('hud-dpad', 'dpad');
    cross.className = 'dpad-background';
    dpad.append(cross);
    el('buttons').append(cButtons, dpad);
    const positions = ['left', 'down', 'right', 'up', 'down', 'left', 'right'];
    for (let i = -1; i < labels.length; ++i) {
        const item = state.buttonItems[i + 1],
            label = i < 0 ? 'B' : labels[i];
        const b = i < 0 ? document.createElement('div') : button('', !enabled || (selected && !state.assignable[selected.slot]) || (i >= 3 && !state.dpadEnabled), () => chooseButton(i));
        b.className = `assignment ${i < 0 ? 'hud-b readonly' : i < 3 ? `hud-c ${positions[i]}` : `hud-direction ${positions[i]}`}`;
        b.title = `${label}: ${name(item)}`;
        b.setAttribute('aria-label', b.title);
        if (i >= 0) b.setAttribute('aria-pressed', String(selectedButton?.button === i));
        else b.setAttribute('role', 'img');
        if (i < 3) {
            const background = tint('hud-button', i < 0 ? 'b' : ['left', 'down', 'right'][i]);
            background.className = 'button-background';
            b.append(background);
        }
        if (item !== 255) {
            const image = icon(item);
            image.className = 'assigned-icon';
            b.append(image);
            const ammoSlot = ({
                0: 0,
                1: 1,
                2: 2,
                3: 3,
                6: 6,
                9: 8,
                16: 14,
                56: 3,
                57: 3,
                58: 3
            })[item];
            if (ammoSlot !== undefined) {
                const count = Math.max(0, state.ammo[ammoSlot]);
                const ammo = document.createElement('span');
                ammo.className = 'hud-ammo';
                for (const digit of String(count)) ammo.append(icon(null, `gAmmoDigit${digit}Tex`));
                b.append(ammo);
                b.setAttribute('aria-label', `${b.title}, ${count}`);
            }
        } else if (i >= 0 && i < 3) {
            const arrow = tint(`hud-c-${positions[i]}`, positions[i]);
            arrow.className = 'empty-c-arrow';
            b.append(arrow);
        }
        (i < 0 ? el('buttons') : i < 3 ? cButtons : dpad).append(b);
    }
    if (selectedButton) {
        el('unassign').hidden = false;
        el('unassign').textContent = `Unassign ${labels[selectedButton.button]}`;
        el('unassign').disabled = !enabled || selectedButton.item === 255;
        el('unassign').onclick = () => change({
            action: 'unassign',
            ...selectedButton
        });
    }
    // Original pause geometry: 28px icons, 32px spacing, six columns by four rows.
    state.items.forEach((item, slot) => {
        if (item === 255) return; // Position derives from slot, so empty slots never collapse the grid.
        const bottle = slot >= 18 && slot <= 21 ? ` (bottle ${slot - 17})` : '';
        const title = name(item) + bottle;
        const b = button('', !state.assignable[slot] || !enabled, () => chooseItem(slot, item));
        decorateSlot(b, item, 26 + (slot % 6) * 32, 24 + Math.floor(slot / 6) * 32, title, state.buttonSlots.includes(slot));
        b.classList.toggle('restricted', !state.assignable[slot]);
        b.setAttribute('aria-pressed', String(selected?.slot === slot));
        showCaption(b, 'item-name', title);
        if ([0, 1, 2, 3, 6, 8, 14].includes(slot)) {
            const ammo = document.createElement('span');
            ammo.className = 'ammo';
            const count = Math.max(0, state.ammo[slot]);
            b.setAttribute('aria-label', `${title}, ${count}`);
            for (const digit of String(count)) ammo.append(icon(null, `gAmmoDigit${digit}Tex`));
            b.append(ammo);
        }
        el('inventory').append(b);
    });
    if (selected) el('item-name').textContent = name(selected.item);
    const gearIcon = (category, value) => 59 + category * 3 + value - 1;
    const gearAsset = (category, value) => category === 0 && value === 3 ?
        (state.biggoronSword ? 'gItemIconSwordBiggoronTex' : (state.ownedEquipment & 8) ? 'gItemIconBrokenGiantsKnifeTex' : undefined) : undefined;
    const gearName = (category, value) => category === 0 && value === 3 ?
        (state.biggoronSword ? 'Biggoron Sword' : (state.ownedEquipment & 8) ? 'Broken Giant’s Knife' : 'Giant’s Knife') : gearNames[category][value];
    for (let row = 0; row < 4; ++row) {
        for (let value = 1; value <= 3; ++value) {
            const owned = !!(state.ownedEquipment & (1 << (row * 4 + value - 1))) || (row === 0 && value === 3 && !!(state.ownedEquipment & 8));
            if (!owned) continue;
            const current = ((state.equipment >> (row * 4)) & 15) === value;
            const title = gearName(row, value);
            const allowed = state.equipmentAllowed?.[row * 3 + value - 1] === true;
            const b = button('', !enabled || !allowed, () => change({
                action: 'equip',
                category: row,
                value,
                fileNum: state.fileNum,
                age: state.age
            }));
            decorateSlot(b, gearIcon(row, value), 134 + (value - 1) * 32, 24 + row * 32, title, current, gearAsset(row, value));
            b.classList.toggle('restricted', state.equipmentAllowed?.[row * 3 + value - 1] === false);
            b.setAttribute('aria-pressed', String(current));
            showCaption(b, 'gear-name', title);
            el('gear').append(b);
        }
    }
    // Upgrade column from the game's equipment page; older companion builds omit this field.
    if (state.upgrades !== undefined) {
        const upgrades = state.upgrades;
        const quiver = upgrades & 7,
            bulletBag = (upgrades >> 14) & 7;
        const levels = [state.age === 1 || !quiver ? bulletBag : quiver, (upgrades >> 3) & 7, (upgrades >> 6) & 7, (upgrades >> 9) & 7];
        const bases = [state.age === 1 || !quiver ? 71 : 74, 77, 80, 83];
        const descriptions = [
            `${state.age === 1 || !quiver ? 'Deku Seed Bullet Bag' : 'Quiver'} (Holds ${[0,30,40,50][levels[0]]})`,
            `Bomb Bag (Holds ${[0,20,30,40][levels[1]]})`,
            ['None', 'Goron Bracelet', 'Silver Gauntlets', 'Golden Gauntlets'][levels[2]],
            ['None', 'Silver Scale', 'Golden Scale'][levels[3]],
        ];
        levels.forEach((level, row) => {
            if (!level) return;
            const b = document.createElement('div');
            b.tabIndex = 0;
            b.setAttribute('role', 'img');
            decorateSlot(b, bases[row] + level - 1, 8, 24 + row * 32, descriptions[row]);
            showCaption(b, 'gear-name', descriptions[row]);
            el('gear').append(b);
        });
    }
    const heading = document.createElement('div');
    heading.className = 'worn-title';
    heading.textContent = 'EQUIPPED';
    el('worn').append(heading);
    for (let category = 0; category < 4; ++category) {
        const value = (state.equipment >> (category * 4)) & 15;
        const row = document.createElement('div');
        row.className = 'worn-row';
        if (value >= 1 && value <= 3) row.append(icon(gearIcon(category, value), gearAsset(category, value)));
        const text = document.createElement('span');
        text.textContent = value >= 1 && value <= 3 ? gearName(category, value) : 'None';
        row.append(text);
        el('worn').append(row);
    }
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
async function refresh() {
    try {
        const response = await fetch('/state');
        const data = await response.json();
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
        const response = await fetch('/action', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(payload)
        });
        const data = await response.json();
        el('message').textContent = data.status === 'success' ? '' : data.message ?? data.status;
        if (data.status === 'success') selected = selectedButton = null;
    } catch {
        el('message').textContent = 'Connection lost. Check Ship before retrying.';
    }
    await refresh();
    busy = false;
    render();
}
async function poll() {
    if (!busy) await refresh();
    setTimeout(poll, 250);
}
poll();