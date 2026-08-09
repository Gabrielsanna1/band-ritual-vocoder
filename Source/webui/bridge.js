// Band Ritual - C++ Bridge
// Handles communication between WebView and JUCE plugin

class BandRitualBridge {
    constructor() {
        this.callbacks = {};
        this.isConnected = false;
        this.parameterQueue = [];

        // Check if running inside JUCE WebView
        this.checkConnection();
    }

    checkConnection() {
        // JUCE injects a __JUCE__ object when running in WebBrowserComponent
        if (typeof __JUCE__ !== 'undefined') {
            this.isConnected = true;
            console.log('Band Ritual: Connected to JUCE');

            // Process any queued parameter changes
            this.parameterQueue.forEach(p => this.setParameter(p.param, p.value));
            this.parameterQueue = [];

            // Request initial parameter values
            this.requestAllParameters();
        } else {
            // Running standalone (for development)
            console.log('Band Ritual: Running in standalone mode');
            this.isConnected = false;
        }
    }

    // Send parameter change to C++
    setParameter(param, value) {
        if (!this.isConnected) {
            // Queue for when connection established
            this.parameterQueue.push({ param, value });
            return;
        }

        // JUCE WebBrowserComponent native function call
        try {
            if (typeof __JUCE__.backend !== 'undefined') {
                __JUCE__.backend.emitByBackend('parameterChanged', JSON.stringify({
                    param: param,
                    value: value
                }));
            }
        } catch (e) {
            console.error('Failed to send parameter to C++:', e);
        }
    }

    // Send EQ node change to C++
    setEQNode(nodeId, freq, gain, q) {
        if (!this.isConnected) return;

        try {
            if (typeof __JUCE__.backend !== 'undefined') {
                __JUCE__.backend.emitByBackend('eqNodeChanged', JSON.stringify({
                    nodeId: nodeId,
                    freq: freq,
                    gain: gain,
                    q: q
                }));
            }
        } catch (e) {
            console.error('Failed to send EQ node to C++:', e);
        }
    }

    // Request all parameter values from C++
    requestAllParameters() {
        if (!this.isConnected) return;

        try {
            if (typeof __JUCE__.backend !== 'undefined') {
                __JUCE__.backend.emitByBackend('requestParameters', '{}');
            }
        } catch (e) {
            console.error('Failed to request parameters:', e);
        }
    }

    // Called from C++ to update a parameter value in the UI
    updateParameter(param, value) {
        if (window.knobController) {
            window.knobController.setValue(param, value);
        }

        // Dispatch event for other listeners
        document.dispatchEvent(new CustomEvent('parameterUpdate', {
            detail: { param, value }
        }));
    }

    // Called from C++ to update band levels visualization
    updateBandLevels(levelsJson) {
        try {
            const levels = JSON.parse(levelsJson);
            if (window.bandVisualizer) {
                window.bandVisualizer.updateBandLevels(levels);
            }
        } catch (e) {
            console.error('Failed to parse band levels:', e);
        }
    }

    // Called from C++ to update spectrum data
    updateSpectrum(spectrumJson) {
        try {
            const spectrum = JSON.parse(spectrumJson);
            if (window.eqVisualizer) {
                window.eqVisualizer.updateSpectrum(spectrum);
            }
        } catch (e) {
            console.error('Failed to parse spectrum:', e);
        }
    }

    // Called from C++ to set all parameters at once (initial load)
    setAllParameters(paramsJson) {
        try {
            const params = JSON.parse(paramsJson);
            Object.entries(params).forEach(([param, value]) => {
                this.updateParameter(param, value);
            });
        } catch (e) {
            console.error('Failed to set all parameters:', e);
        }
    }

    // Register callback for specific events
    on(event, callback) {
        if (!this.callbacks[event]) {
            this.callbacks[event] = [];
        }
        this.callbacks[event].push(callback);
    }

    // Trigger registered callbacks
    trigger(event, data) {
        if (this.callbacks[event]) {
            this.callbacks[event].forEach(cb => cb(data));
        }
    }
}

// Global instance
window.BandRitualBridge = new BandRitualBridge();

// Expose functions for C++ to call
window.updateParameter = (param, value) => window.BandRitualBridge.updateParameter(param, value);
window.updateBandLevels = (levels) => window.BandRitualBridge.updateBandLevels(levels);
window.updateSpectrum = (spectrum) => window.BandRitualBridge.updateSpectrum(spectrum);
window.setAllParameters = (params) => window.BandRitualBridge.setAllParameters(params);

// Debug helper for standalone testing
if (typeof __JUCE__ === 'undefined') {
    console.log('Band Ritual WebUI loaded in standalone mode');
    console.log('Use window.BandRitualBridge to interact with the bridge');

    // Simulate connection after a delay (for testing)
    setTimeout(() => {
        document.body.classList.add('connected');
    }, 100);
}
