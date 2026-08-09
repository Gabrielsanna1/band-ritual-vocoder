// Band Ritual - Knob Controller
// Handles all rotary knob interactions

class KnobController {
    constructor() {
        this.knobs = new Map();
        this.activeKnob = null;
        this.startY = 0;
        this.startValue = 0;
        this.sensitivity = 0.5; // Degrees per pixel

        this.init();
    }

    init() {
        // Find all knobs
        document.querySelectorAll('.knob').forEach(knobEl => {
            const id = knobEl.id;
            const param = knobEl.dataset.param || id;
            const min = parseFloat(knobEl.dataset.min) || 0;
            const max = parseFloat(knobEl.dataset.max) || 100;
            const value = parseFloat(knobEl.dataset.value) || 0;

            const knob = {
                element: knobEl,
                param: param,
                min: min,
                max: max,
                value: value,
                normalized: (value - min) / (max - min)
            };

            this.knobs.set(id, knob);
            this.updateKnobVisual(knob);

            // Event listeners
            knobEl.addEventListener('mousedown', (e) => this.onMouseDown(e, id));
            knobEl.addEventListener('dblclick', () => this.resetKnob(id));
        });

        // Global mouse events
        document.addEventListener('mousemove', (e) => this.onMouseMove(e));
        document.addEventListener('mouseup', () => this.onMouseUp());

        // Prevent context menu on knobs
        document.addEventListener('contextmenu', (e) => {
            if (e.target.classList.contains('knob')) {
                e.preventDefault();
            }
        });
    }

    onMouseDown(e, knobId) {
        e.preventDefault();
        const knob = this.knobs.get(knobId);
        if (!knob) return;

        this.activeKnob = knob;
        this.startY = e.clientY;
        this.startValue = knob.normalized;

        knob.element.classList.add('dragging');
        document.body.style.cursor = 'ns-resize';
    }

    onMouseMove(e) {
        if (!this.activeKnob) return;

        const deltaY = this.startY - e.clientY;
        const deltaNorm = deltaY * this.sensitivity / 100;

        let newNorm = this.startValue + deltaNorm;
        newNorm = Math.max(0, Math.min(1, newNorm));

        this.activeKnob.normalized = newNorm;
        this.activeKnob.value = this.activeKnob.min + newNorm * (this.activeKnob.max - this.activeKnob.min);

        this.updateKnobVisual(this.activeKnob);
        this.notifyParameterChange(this.activeKnob);
    }

    onMouseUp() {
        if (this.activeKnob) {
            this.activeKnob.element.classList.remove('dragging');
            this.activeKnob = null;
        }
        document.body.style.cursor = '';
    }

    updateKnobVisual(knob) {
        // Rotate indicator from -135 to +135 degrees
        const angle = -135 + (knob.normalized * 270);
        knob.element.style.setProperty('--rotation', `${angle}deg`);
        knob.element.querySelector('::after')?.style;

        // Update the ::after pseudo-element via CSS variable
        knob.element.style.transform = ''; // Reset any existing transform
        const indicator = knob.element.querySelector('.indicator') || this.createIndicator(knob.element);
        indicator.style.transform = `translate(-50%, -100%) rotate(${angle}deg)`;

        // Update value display
        const valueEl = document.getElementById(`${knob.element.id}-value`);
        if (valueEl) {
            valueEl.textContent = this.formatValue(knob);
        }
    }

    createIndicator(knobEl) {
        const indicator = document.createElement('div');
        indicator.className = 'indicator';
        indicator.style.cssText = `
            position: absolute;
            top: 50%;
            left: 50%;
            width: 3px;
            height: 40%;
            background: var(--accent-cyan, #00d4ff);
            transform-origin: bottom center;
            border-radius: 2px;
            pointer-events: none;
        `;
        knobEl.appendChild(indicator);
        return indicator;
    }

    formatValue(knob) {
        const val = knob.value;
        const param = knob.param;

        // Custom formatting based on parameter
        if (param === 'mix' || param === 'depth' || param === 'gate' ||
            param === 'scaleAmount' || param === 'chordAmount' ||
            param === 'bandWidth' || param === 'unvoiced' || param === 'hpBlend') {
            return `${Math.round(val)}%`;
        }
        if (param === 'bandCount') {
            return Math.round(val).toString();
        }
        if (param === 'attack' || param === 'release') {
            return `${Math.round(val)}ms`;
        }
        if (param === 'formantShift' || param === 'harmShift') {
            const sign = val >= 0 ? '+' : '';
            return `${sign}${Math.round(val)}st`;
        }
        if (param === 'nodeFreq') {
            if (val >= 1000) {
                return `${(val / 1000).toFixed(1)}kHz`;
            }
            return `${Math.round(val)}Hz`;
        }
        if (param === 'nodeGain') {
            const sign = val >= 0 ? '+' : '';
            return `${sign}${val.toFixed(1)}dB`;
        }
        if (param === 'nodeQ') {
            return val.toFixed(1);
        }

        return Math.round(val).toString();
    }

    notifyParameterChange(knob) {
        // Send to C++ via bridge
        if (window.BandRitualBridge) {
            window.BandRitualBridge.setParameter(knob.param, knob.value);
        }

        // Also dispatch a custom event for visualizers
        document.dispatchEvent(new CustomEvent('parameterChanged', {
            detail: { param: knob.param, value: knob.value, normalized: knob.normalized }
        }));
    }

    resetKnob(knobId) {
        const knob = this.knobs.get(knobId);
        if (!knob) return;

        // Reset to default (middle or dataset default)
        const defaultVal = parseFloat(knob.element.dataset.default) ||
                          (knob.min + knob.max) / 2;
        knob.value = defaultVal;
        knob.normalized = (defaultVal - knob.min) / (knob.max - knob.min);

        this.updateKnobVisual(knob);
        this.notifyParameterChange(knob);
    }

    // Called from C++ to update knob value
    setValue(paramId, value) {
        const knob = this.knobs.get(paramId);
        if (!knob) return;

        knob.value = value;
        knob.normalized = (value - knob.min) / (knob.max - knob.min);
        this.updateKnobVisual(knob);
    }

    getValue(paramId) {
        const knob = this.knobs.get(paramId);
        return knob ? knob.value : 0;
    }
}

// Initialize on load
document.addEventListener('DOMContentLoaded', () => {
    window.knobController = new KnobController();
});
