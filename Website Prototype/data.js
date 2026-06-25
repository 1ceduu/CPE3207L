export const penData = {
    1: {
        name: "Pen 01",
        feedRemaining: 24, waterLevel: 92, nh3: 12,
        status: "Normal", statusClass: "normal", cardClass: "",
        refillThreshold: 40,
        history: [31, 33, 35, 32, 36, 34, 38],
        dangerZone: [22, 22, 22, 22, 22, 22, 22],
        alerts: [
            { time: "10:45 AM", type: "status",  text: "System normal — all parameters in range" },
            { time: "09:30 AM", type: "status",  text: "Morning feed dispensed successfully" },
            { time: "08:15 AM", type: "status",  text: "Water reservoir refilled automatically" }
        ],
        schedule: [
            { label: "Morning Cycle", time: "06:00 AM", amount: "2.0 kg" },
            { label: "Midday Cycle",  time: "12:00 PM", amount: "1.5 kg" }
        ],
        settings: { mHour: 6, mMin: 0, mPer: "AM", mAmount: 2.0, aHour: 12, aMin: 0, aPer: "PM", aAmount: 1.5 }
    },
    2: {
        name: "Pen 02",
        feedRemaining: 18, waterLevel: 85, nh3: 22,
        status: "Warning", statusClass: "warning", cardClass: "warning",
        refillThreshold: 40,
        history: [32, 31, 28, 25, 23, 20, 18],
        dangerZone: [22, 22, 22, 22, 22, 22, 22],
        alerts: [
            { time: "10:45 AM", type: "warning", text: "NH3 approaching 25 ppm threshold" },
            { time: "09:30 AM", type: "warning", text: "Feed consumption dropping below baseline" },
            { time: "08:15 AM", type: "status",  text: "Morning feed dispensed successfully" }
        ],
        schedule: [
            { label: "Morning Cycle", time: "06:00 AM", amount: "2.0 kg" },
            { label: "Midday Cycle",  time: "12:00 PM", amount: "1.5 kg" }
        ],
        settings: { mHour: 6, mMin: 0, mPer: "AM", mAmount: 2.0, aHour: 12, aMin: 0, aPer: "PM", aAmount: 1.5 }
    },
    3: {
        name: "Pen 03",
        feedRemaining: 27, waterLevel: 12, nh3: 18,
        status: "Critical", statusClass: "critical", cardClass: "critical",
        refillThreshold: 40,
        history: [30, 28, 24, 18, 14, 10, 9],
        dangerZone: [22, 22, 22, 22, 22, 22, 22],
        alerts: [
            { time: "10:45 AM", type: "critical", text: "Low water level detected — action required" },
            { time: "09:30 AM", type: "critical", text: "Severe feed deviation flagged by sensor" },
            { time: "08:15 AM", type: "status",   text: "Morning feed dispensed successfully" }
        ],
        schedule: [
            { label: "Morning Cycle", time: "06:00 AM", amount: "2.0 kg" },
            { label: "Midday Cycle",  time: "12:00 PM", amount: "1.5 kg" }
        ],
        settings: { mHour: 6, mMin: 0, mPer: "AM", mAmount: 2.0, aHour: 12, aMin: 0, aPer: "PM", aAmount: 1.5 }
    }
};

export const state = {
    currentPen: 1,
    feedDispatch: 1.0,
    waterDispatch: 2.0
};
