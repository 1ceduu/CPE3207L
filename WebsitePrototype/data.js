export const penData = {};

export const state = {
    currentPen: 1,
    feedDispatch: 1.0,
    waterDispatch: 2.0,
    connectedCount: 0
};

export function createDefaultPen(penId) {
    const id = parseInt(penId, 10);
    return {
        pen_id: id,
        name: `Pen ${String(id).padStart(2, '0')}`,
        feedRemaining: 0.0,
        waterLevel: 0,
        nh3: 0.0,
        status: "Connecting...",
        statusClass: "normal",
        cardClass: "",
        refillThreshold: 45,
        history: [28, 30, 32, 29, 31, 33, 30],
        dangerZone: [20, 20, 20, 20, 20, 20, 20],
        alerts: [
            { time: "--:-- --", type: "status", text: "Feeder Node initial connection..." }
        ],
        schedule: [
            { label: "Morning Cycle", time: "06:00 AM", amount: "2.0 kg" },
            { label: "Midday Cycle",  time: "12:00 PM", amount: "1.5 kg" }
        ],
        settings: { mHour: 6, mMin: 0, mPer: "AM", mAmount: 2.0, aHour: 12, aMin: 0, aPer: "PM", aAmount: 1.5 }
    };
}
