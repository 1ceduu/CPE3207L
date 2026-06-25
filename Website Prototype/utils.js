export function currentTimeStr() {
    const now  = new Date();
    let h      = now.getHours();
    const ampm = h >= 12 ? "PM" : "AM";
    h          = h % 12 || 12;
    return `${h}:${String(now.getMinutes()).padStart(2, "0")} ${ampm}`;
}

export function showToast(message) {
    const toast = document.getElementById("toast");
    if (toast) {
        toast.textContent = message;
        toast.classList.add("show");
        setTimeout(() => toast.classList.remove("show"), 2200);
    }
}

export function updateClock() {
    const now  = new Date();
    let h      = now.getHours();
    const ampm = h >= 12 ? "PM" : "AM";
    h          = h % 12 || 12;
    const clockEl = document.getElementById("clock");
    if (clockEl) {
        clockEl.textContent =
            `${h}:${String(now.getMinutes()).padStart(2, "0")}:${String(now.getSeconds()).padStart(2, "0")} ${ampm}`;
    }
}

export function updateRangeTrack(v) {
    const pct = ((v - 10) / (80 - 10)) * 100;
    const inp = document.getElementById("water-threshold-range");
    if (inp) {
        inp.style.background = `linear-gradient(90deg, var(--cyan) ${pct}%, rgba(255,255,255,0.08) ${pct}%)`;
    }
}
