// Band Ritual - Visualizers
// Band display and EQ curve visualization

class BandVisualizer {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.bandLevels = new Array(32).fill(0);
        this.bandCount = 32;
        this.lowFreq = 180;
        this.highFreq = 9000;
        this.animationId = null;

        this.colors = {
            band: '#ffd700',
            bandGlow: 'rgba(255, 215, 0, 0.3)',
            grid: '#333',
            text: '#666'
        };

        this.init();
    }

    init() {
        this.resize();
        window.addEventListener('resize', () => this.resize());

        // Listen for parameter changes
        document.addEventListener('parameterChanged', (e) => {
            if (e.detail.param === 'bandCount') {
                this.bandCount = Math.round(e.detail.value);
                this.bandLevels = new Array(this.bandCount).fill(0);
            }
        });

        this.animate();
    }

    resize() {
        const rect = this.canvas.parentElement.getBoundingClientRect();
        const dpr = window.devicePixelRatio || 1;
        this.canvas.width = rect.width * dpr;
        this.canvas.height = (rect.height - 40) * dpr; // Account for axis
        this.ctx.scale(dpr, dpr);
        this.width = rect.width;
        this.height = rect.height - 40;
    }

    // Called from C++ to update band levels
    updateBandLevels(levels) {
        this.bandLevels = levels;
    }

    // Simulate band levels for demo
    simulateLevels() {
        for (let i = 0; i < this.bandCount; i++) {
            // Random decay simulation
            const target = Math.random() * 0.8;
            this.bandLevels[i] = this.bandLevels[i] * 0.9 + target * 0.1;
        }
    }

    draw() {
        const ctx = this.ctx;
        const w = this.width;
        const h = this.height;

        // Clear
        ctx.clearRect(0, 0, w, h);

        // Draw grid lines
        ctx.strokeStyle = this.colors.grid;
        ctx.lineWidth = 0.5;
        for (let i = 1; i < 4; i++) {
            const y = h * (i / 4);
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(w, y);
            ctx.stroke();
        }

        // Draw bands
        const bandWidth = (w - 20) / this.bandCount;
        const gap = Math.max(1, bandWidth * 0.1);
        const barWidth = bandWidth - gap;

        ctx.fillStyle = this.colors.band;
        ctx.shadowColor = this.colors.bandGlow;
        ctx.shadowBlur = 10;

        for (let i = 0; i < this.bandCount; i++) {
            const x = 10 + i * bandWidth;
            const level = this.bandLevels[i] || 0;
            const barHeight = level * h * 0.9;

            // Gradient for each bar
            const gradient = ctx.createLinearGradient(x, h, x, h - barHeight);
            gradient.addColorStop(0, '#ffd700');
            gradient.addColorStop(0.5, '#ffaa00');
            gradient.addColorStop(1, '#ff6600');
            ctx.fillStyle = gradient;

            ctx.fillRect(x, h - barHeight, barWidth, barHeight);
        }

        ctx.shadowBlur = 0;
    }

    animate() {
        this.simulateLevels(); // Remove this when connected to C++
        this.draw();
        this.animationId = requestAnimationFrame(() => this.animate());
    }

    stop() {
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
        }
    }
}

class EQVisualizer {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.spectrumData = new Array(256).fill(0);
        this.eqNodes = [];
        this.selectedNode = null;
        this.animationId = null;
        this.complementMode = false;

        this.colors = {
            spectrum: '#333',
            spectrumFill: 'rgba(50, 50, 50, 0.5)',
            eqCurve: '#00d4ff',
            eqCurveFill: 'rgba(0, 212, 255, 0.1)',
            nodeBell: '#fff',
            nodeComplement: '#ffd700',
            nodeHP: '#4ade80',
            nodeLP: '#f472b6',
            grid: '#2a2a2a',
            text: '#666'
        };

        // Default EQ nodes
        this.eqNodes = [
            { id: 'hp', type: 'hp', freq: 80, enabled: true },
            { id: 'bell1', type: 'bell', freq: 500, gain: 0, q: 1, enabled: true },
            { id: 'bell2', type: 'bell', freq: 2000, gain: 0, q: 1, enabled: true },
            { id: 'lp', type: 'lp', freq: 18000, enabled: true },
            { id: 'w1', type: 'complement', freq: 800, gain: 0, q: 1, enabled: false },
            { id: 'w2', type: 'complement', freq: 3000, gain: 0, q: 1, enabled: false }
        ];

        this.init();
    }

    init() {
        this.resize();
        window.addEventListener('resize', () => this.resize());

        // Mouse interaction for nodes
        this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseup', () => this.onMouseUp());

        // Complement toggle
        document.getElementById('complementToggle')?.addEventListener('click', (e) => {
            this.complementMode = !this.complementMode;
            e.target.classList.toggle('active', this.complementMode);
            this.eqNodes.find(n => n.id === 'w1').enabled = this.complementMode;
            this.eqNodes.find(n => n.id === 'w2').enabled = this.complementMode;
        });

        this.animate();
    }

    resize() {
        const rect = this.canvas.parentElement.getBoundingClientRect();
        const dpr = window.devicePixelRatio || 1;
        this.canvas.width = rect.width * dpr;
        this.canvas.height = rect.height * dpr;
        this.ctx.scale(dpr, dpr);
        this.width = rect.width;
        this.height = rect.height;
    }

    freqToX(freq) {
        // Logarithmic scale: 20Hz to 20kHz
        const minLog = Math.log10(20);
        const maxLog = Math.log10(18000);
        const freqLog = Math.log10(Math.max(20, freq));
        return ((freqLog - minLog) / (maxLog - minLog)) * this.width;
    }

    xToFreq(x) {
        const minLog = Math.log10(20);
        const maxLog = Math.log10(18000);
        const ratio = x / this.width;
        return Math.pow(10, minLog + ratio * (maxLog - minLog));
    }

    gainToY(gain) {
        // -24dB to +24dB mapped to height
        return this.height / 2 - (gain / 24) * (this.height / 2 - 20);
    }

    yToGain(y) {
        return -((y - this.height / 2) / (this.height / 2 - 20)) * 24;
    }

    // Called from C++ to update spectrum
    updateSpectrum(data) {
        this.spectrumData = data;
    }

    simulateSpectrum() {
        for (let i = 0; i < this.spectrumData.length; i++) {
            const target = Math.random() * 0.3 + 0.1;
            this.spectrumData[i] = this.spectrumData[i] * 0.95 + target * 0.05;
        }
    }

    draw() {
        const ctx = this.ctx;
        const w = this.width;
        const h = this.height;

        // Clear
        ctx.clearRect(0, 0, w, h);

        // Draw frequency grid
        ctx.strokeStyle = this.colors.grid;
        ctx.lineWidth = 0.5;
        const freqs = [50, 100, 200, 500, 1000, 2000, 5000, 10000];
        freqs.forEach(freq => {
            const x = this.freqToX(freq);
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, h);
            ctx.stroke();
        });

        // Draw dB grid
        [-18, -12, -6, 0, 6, 12, 18].forEach(db => {
            const y = this.gainToY(db);
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(w, y);
            ctx.stroke();
        });

        // Draw spectrum
        ctx.beginPath();
        ctx.moveTo(0, h);
        for (let i = 0; i < this.spectrumData.length; i++) {
            const x = (i / this.spectrumData.length) * w;
            const y = h - this.spectrumData[i] * h * 0.8;
            ctx.lineTo(x, y);
        }
        ctx.lineTo(w, h);
        ctx.closePath();
        ctx.fillStyle = this.colors.spectrumFill;
        ctx.fill();

        // Draw EQ curve
        this.drawEQCurve();

        // Draw nodes
        this.drawNodes();

        // Draw frequency labels
        ctx.fillStyle = this.colors.text;
        ctx.font = '9px sans-serif';
        ctx.textAlign = 'center';
        [100, 1000, 10000].forEach(freq => {
            const x = this.freqToX(freq);
            const label = freq >= 1000 ? `${freq/1000}k` : freq;
            ctx.fillText(label, x, h - 5);
        });
    }

    drawEQCurve() {
        const ctx = this.ctx;
        const w = this.width;
        const h = this.height;

        ctx.beginPath();
        ctx.strokeStyle = this.colors.eqCurve;
        ctx.lineWidth = 2;

        for (let x = 0; x < w; x++) {
            const freq = this.xToFreq(x);
            let totalGain = 0;

            // Calculate combined response from all enabled nodes
            this.eqNodes.filter(n => n.enabled).forEach(node => {
                if (node.type === 'bell' || node.type === 'complement') {
                    // Bell filter response approximation
                    const q = node.q || 1;
                    const centerFreq = node.freq;
                    const ratio = freq / centerFreq;
                    const response = 1 / (1 + q * q * Math.pow(ratio - 1/ratio, 2));
                    totalGain += (node.gain || 0) * response;
                } else if (node.type === 'hp') {
                    // High-pass response
                    if (freq < node.freq) {
                        totalGain -= 24 * (1 - freq / node.freq);
                    }
                } else if (node.type === 'lp') {
                    // Low-pass response
                    if (freq > node.freq) {
                        totalGain -= 24 * ((freq - node.freq) / node.freq);
                    }
                }
            });

            const y = this.gainToY(totalGain);
            if (x === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        }

        ctx.stroke();
    }

    drawNodes() {
        const ctx = this.ctx;

        this.eqNodes.filter(n => n.enabled).forEach(node => {
            const x = this.freqToX(node.freq);
            const y = node.type === 'hp' || node.type === 'lp' ?
                      this.height / 2 :
                      this.gainToY(node.gain || 0);

            // Node color based on type
            let color;
            switch (node.type) {
                case 'hp': color = this.colors.nodeHP; break;
                case 'lp': color = this.colors.nodeLP; break;
                case 'complement': color = this.colors.nodeComplement; break;
                default: color = this.colors.nodeBell;
            }

            // Draw node
            ctx.beginPath();
            ctx.arc(x, y, 8, 0, Math.PI * 2);
            ctx.fillStyle = this.selectedNode === node ? color : 'transparent';
            ctx.fill();
            ctx.strokeStyle = color;
            ctx.lineWidth = 2;
            ctx.stroke();

            // Label
            ctx.fillStyle = color;
            ctx.font = '9px sans-serif';
            ctx.textAlign = 'center';
            const label = node.type === 'complement' ? node.id.toUpperCase() : node.id.toUpperCase();
            ctx.fillText(label, x, y - 12);
        });
    }

    onMouseDown(e) {
        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;

        // Check if clicking on a node
        this.selectedNode = null;
        this.eqNodes.filter(n => n.enabled).forEach(node => {
            const nodeX = this.freqToX(node.freq);
            const nodeY = node.type === 'hp' || node.type === 'lp' ?
                          this.height / 2 :
                          this.gainToY(node.gain || 0);

            const dist = Math.sqrt(Math.pow(x - nodeX, 2) + Math.pow(y - nodeY, 2));
            if (dist < 12) {
                this.selectedNode = node;
                this.isDragging = true;
            }
        });

        // Update control knobs if node selected
        if (this.selectedNode) {
            this.updateNodeControls();
        }
    }

    onMouseMove(e) {
        if (!this.isDragging || !this.selectedNode) return;

        const rect = this.canvas.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;

        this.selectedNode.freq = Math.max(20, Math.min(18000, this.xToFreq(x)));

        if (this.selectedNode.type !== 'hp' && this.selectedNode.type !== 'lp') {
            this.selectedNode.gain = Math.max(-24, Math.min(24, this.yToGain(y)));
        }

        this.updateNodeControls();
        this.notifyNodeChange();
    }

    onMouseUp() {
        this.isDragging = false;
    }

    updateNodeControls() {
        if (!this.selectedNode) return;

        if (window.knobController) {
            window.knobController.setValue('nodeFreq', this.selectedNode.freq);
            if (this.selectedNode.gain !== undefined) {
                window.knobController.setValue('nodeGain', this.selectedNode.gain);
            }
            if (this.selectedNode.q !== undefined) {
                window.knobController.setValue('nodeQ', this.selectedNode.q);
            }
        }
    }

    notifyNodeChange() {
        if (window.BandRitualBridge && this.selectedNode) {
            window.BandRitualBridge.setEQNode(
                this.selectedNode.id,
                this.selectedNode.freq,
                this.selectedNode.gain || 0,
                this.selectedNode.q || 1
            );
        }
    }

    animate() {
        this.simulateSpectrum(); // Remove when connected to C++
        this.draw();
        this.animationId = requestAnimationFrame(() => this.animate());
    }

    stop() {
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
        }
    }
}

// Initialize on load
document.addEventListener('DOMContentLoaded', () => {
    window.bandVisualizer = new BandVisualizer('bandCanvas');
    window.eqVisualizer = new EQVisualizer('eqCanvas');
});
