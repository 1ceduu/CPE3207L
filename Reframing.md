## Refactored File Structure Plan

### Target Structure
```
Website Prototype/
├── index.html          # Semantic HTML structure only
├── styles.css          # All CSS (670+ lines)
├── config.js           # Constants, magic numbers, defaults
├── data.js             # State store, pen data, mutations
├── charts.js           # Canvas chart rendering (drawConsumptionChart)
├── ui.js               # Screen/modal management, DOM rendering
├── utils.js            # Helpers (time, toast, clamping, formatting)
└── app.js              # Entry point, event wiring, initialization
```

---

### Module Responsibilities

| File | Exports / Responsibilities |
|------|---------------------------|
| **config.js** | `CONFIG` object: `MAX_WATER_L`, `NH3_LIMIT_PPM`, `THRESHOLD_RANGE`, `FEED_DISPATCH_STEP`, colors, breakpoints |
| **data.js** | `penData` (immutable source), `state` (currentPen, dispatch values), `mutators` (`setWaterLevel`, `addAlert`, `updateSchedule`), `subscribe()` for reactivity |
| **charts.js** | `drawConsumptionChart(canvas, history, dangerZone, options)` — pure function, no side effects |
| **ui.js** | `renderDashboard()`, `loadPenControlData()`, `loadFeedSettings()`, `loadWaterSettings()`, `loadOverrideScreen()`, `showScreen()`, `openModal()`, `closeModal()`, `renderSchedule()`, `renderAlerts()` |
| **utils.js** | `formatTime()`, `currentTimeStr()`, `showToast()`, `clamp()`, `debounce()`, `padStart()`, `updateRangeTrack()` |
| **app.js** | `init()` — wires DOM events, starts clock, fullscreen handler, resize debounce, calls `renderDashboard()` |

---

### HTML Changes (index.html)

- Remove `<style>` and `<script>` blocks
- Add `<link rel="stylesheet" href="styles.css">`
- Add `<script type="module" src="app.js"></script>` (enables ES modules)
- Keep all existing DOM structure, IDs, classes **identical**
- Add `defer` on module script (auto-deferred)

---

### Migration Strategy (Zero Behavioral Change)

1. **Copy-paste CSS verbatim** → `styles.css`
2. **Extract config constants** first (search for magic numbers: `5`, `25`, `40`, `80`, `10`, `0.5`, `2.0`, etc.)
3. **Move `penData` + state vars** → `data.js` with getter/setter wrappers
4. **Extract pure functions** → `utils.js` (no DOM deps)
5. **Extract chart function** → `charts.js` (only canvas ctx deps)
6. **Extract UI renderers** → `ui.js` (imports data, utils, charts)
7. **Wire in `app.js`** — import all modules, attach event listeners, call `init()`

---

### Key Decisions Needed

| Decision | Options | Recommendation |
|----------|---------|----------------|
| **Module format** | ES modules (`type="module"`) vs IIFE | ES modules — cleaner, modern, works on `file://` with local server |
| **State reactivity** | Manual DOM updates (current) vs tiny pub/sub | Keep manual for now — zero behavior change |
| **Chart library** | Keep custom Canvas vs Chart.js | Keep custom — lightweight, no deps, matches prototype |
| **Build step** | None (dev) vs Vite/esbuild (prod) | **None for prototype** — keep it runnable via double-click |

---

### File Size Estimates

| File | Est. Lines |
|------|-----------|
| `index.html` | ~180 |
| `styles.css` | ~670 |
| `config.js` | ~40 |
| `data.js` | ~120 |
| `charts.js` | ~90 |
| `ui.js` | ~280 |
| `utils.js` | ~60 |
| `app.js` | ~50 |
| **Total** | **~1,490** (same logic, organized) |

---

### Execution Order (when you approve)

1. Create `styles.css` — copy entire `<style>` block
2. Create `config.js` — extract all magic numbers
3. Create `data.js` — move `penData`, `currentPen`, `feedDispatch`, `waterDispatch` + mutators
4. Create `utils.js` — pure helpers
5. Create `charts.js` — `drawConsumptionChart` only
6. Create `ui.js` — all render/load/showScreen/openModal functions
7. Create `app.js` — imports + `init()` + event listeners
8. Create `index.html` — strip styles/scripts, add links
9. **Smoke test** — open in browser, verify identical behavior

---

### Risks / Gotchas

- **`onclick` attributes in HTML** → must keep global functions or rebind in `app.js`
  - Fix: Keep `onclick="showScreen(...)"` but ensure `showScreen` is on `window` (export from `ui.js`, assign in `app.js`)
- **`oninput`/`onchange` inline handlers** → same treatment
- **CSS custom properties** — must load before any inline style uses them (OK with `<link>` in `<head>`)
- **Font preconnect** — keep in HTML `<head>`

---
