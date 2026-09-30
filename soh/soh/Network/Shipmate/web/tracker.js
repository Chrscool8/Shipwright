// These are the stable pause-menu slots and identifiers already returned by /state.
const inventory = [
    [0, 0x00, 'Deku Stick'], [1, 0x01, 'Deku Nut'], [2, 0x02, 'Bombs'],
    [3, 0x03, 'Bow'], [4, 0x04, 'Fire Arrow'], [5, 0x05, "Din's Fire"],
    [6, 0x06, 'Slingshot'], [7, 0x08, 'Ocarina'], [8, 0x09, 'Bombchu'],
    [9, 0x0a, 'Hookshot'], [10, 0x0c, 'Ice Arrow'], [11, 0x0d, "Farore's Wind"],
    [12, 0x0e, 'Boomerang'], [13, 0x0f, 'Lens of Truth'], [14, 0x10, 'Magic Bean'],
    [15, 0x11, 'Megaton Hammer'], [16, 0x12, 'Light Arrow'], [17, 0x13, "Nayru's Love"],
    [18, 0x14, 'Bottle 1'], [19, 0x14, 'Bottle 2'], [20, 0x14, 'Bottle 3'], [21, 0x14, 'Bottle 4']
];

const equipment = [
    ['equipment', 0, 1, 0x3b, 'Kokiri Sword'], ['equipment', 0, 2, 0x3c, 'Master Sword'],
    ['equipment', 0, 3, 0x3d, "Biggoron's Sword"], ['equipment', 1, 1, 0x3e, 'Deku Shield'],
    ['equipment', 1, 2, 0x3f, 'Hylian Shield'], ['equipment', 1, 3, 0x40, 'Mirror Shield'],
    ['equipment', 2, 2, 0x42, 'Goron Tunic'], ['equipment', 2, 3, 0x43, 'Zora Tunic'],
    ['equipment', 3, 2, 0x45, 'Iron Boots'], ['equipment', 3, 3, 0x46, 'Hover Boots'],
    ['upgrade', 0, 0, 0x47, 'Ammo Upgrade'], ['upgrade', 1, 0, 0x4d, 'Bomb Bag'],
    ['upgrade', 2, 0, 0x50, 'Strength'], ['upgrade', 3, 0, 0x53, 'Scale']
];

const quest = [
    [0, 0x66, 'Forest Medallion'], [1, 0x67, 'Fire Medallion'], [2, 0x68, 'Water Medallion'],
    [3, 0x69, 'Spirit Medallion'], [4, 0x6a, 'Shadow Medallion'], [5, 0x6b, 'Light Medallion'],
    [18, 0x6c, 'Kokiri Emerald'], [19, 0x6d, 'Goron Ruby'], [20, 0x6e, 'Zora Sapphire'],
    [21, 0x6f, 'Stone of Agony'], [22, 0x70, "Gerudo's Card"], [23, 0x71, 'Gold Skulltula Tokens']
];

const songs = [
    [6, 0x5a, 'Minuet of Forest'], [7, 0x5b, 'Bolero of Fire'], [8, 0x5c, 'Serenade of Water'],
    [9, 0x5d, 'Requiem of Spirit'], [10, 0x5e, 'Nocturne of Shadow'], [11, 0x5f, 'Prelude of Light'],
    [12, 0x60, "Zelda's Lullaby"], [13, 0x61, "Epona's Song"], [14, 0x62, "Saria's Song"],
    [15, 0x63, "Sun's Song"], [16, 0x64, 'Song of Time'], [17, 0x65, 'Song of Storms']
];

function tile(parent, placeholder, name, entry, count, revision) {
    const node = document.createElement('div');
    node.className = `tile${entry ? ' owned' : ''}`;
    node.title = entry?.name ?? name;
    node.setAttribute('role', 'img');
    node.setAttribute('aria-label', `${node.title}: ${entry ? 'owned' : 'not owned'}`);
    const image = document.createElement('img');
    image.alt = '';
    image.src = `/assets/${entry?.asset ?? `item-${placeholder}`}.png?v=${revision}`;
    node.append(image);
    if (count !== undefined) {
        const badge = document.createElement('span');
        badge.className = 'count';
        badge.textContent = count;
        node.append(badge);
    }
    parent.append(node);
}

function render(state) {
    for (const id of ['inventory', 'equipment', 'songs', 'quest']) el(id).replaceChildren();
    for (const [slot, icon, name] of inventory) {
        const entry = state.items[slot];
        tile(el('inventory'), icon, name, entry.empty ? null : entry, entry.ammo ?? undefined, state.assetRevision);
    }
    for (const [kind, first, second, icon, name] of equipment) {
        const entry = kind === 'equipment'
            ? state.equipment.find(item => item.category === first && item.value === second)
            : state.upgrades.find(item => item.row === first);
        tile(el('equipment'), icon, name, entry, undefined, state.assetRevision);
    }
    for (const [point, icon, name] of songs) {
        tile(el('songs'), icon, name, state.quest.find(item => item.point === point), undefined, state.assetRevision);
    }
    for (const [point, icon, name] of quest) {
        const entry = state.quest.find(item => item.point === point);
        tile(el('quest'), icon, name, entry, entry?.tokens, state.assetRevision);
    }
}

function emptyState(revision) {
    return {
        assetRevision: revision,
        items: Array.from({ length: 24 }, () => ({ empty: true, ammo: null })),
        equipment: [],
        upgrades: [],
        quest: []
    };
}

watchState({
    // Compare only displayed data; HUD colors and assignments do not affect this page.
    select: ({ loaded, assetRevision, items, equipment, upgrades, quest }) =>
        loaded ? { assetRevision, items, equipment, upgrades, quest } : { assetRevision },
    onChange(state) {
        el('status').textContent = stateStatus(state) ?? 'Live tracker';
        // Keep the last tracker visible while disconnected.
        if (!state) return;
        el('tracker').hidden = false;
        render(state.loaded ? state : emptyState(state.assetRevision));
    }
});
