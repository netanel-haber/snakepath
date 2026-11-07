// toy.js - <snakepath-toy>, the snakepath logo as a toy: logo.c, as logo.wasm in a worker, draws the snake on a
// canvas off the page's thread, at any resolution in the same box. With the controls attribute, five knobs and an
// export float over it, small and translucent. What is
// inside the element (say, snakepath.png at rest) shows until the toy draws, and stays without a worker, an
// OffscreenCanvas or WebAssembly, or for readers who prefer reduced motion. snakepath.dev uses it, and so does
// toy.html, which other pages embed.

// The knobs, in the order of logo.c's LogoSettings: name, and the values at the left end, in the middle (where each
// starts) and at the right end, the two ends as far from the middle on either side.
const KNOBS = [
  ['speed', 0.03, 0.3, 0.57],
  ['sway', 0, 1.12, 2.24],
  ['waves', 0, 1, 2],
  ['resolution', 80, 96, 112],
  ['rattle', 0, 2, 4],
];
const value = ([, low, , high], x) => low + (high - low) * x;

// Runs logo.wasm and draws its frames: only the rows that changed, each 'c' from one drawing of the glyph. The page's
// first message hands the canvas over, and every message waits until logo.wasm is loaded.
const WORKER = `
  let wasm, settings, canvas, ctx, box, big, bctx, glyph, cell, loaded, shown = [];

  function text() {
    const bytes = new Uint8Array(wasm.memory.buffer, wasm.logo_out.value);
    return new TextDecoder().decode(bytes.subarray(0, bytes.indexOf(0)));
  }
  // The frame is drawn as snakepath.png is: each 'c' a whole glyph, on a canvas of its own with cells at least 8
  // pixels wide (a row is two character widths tall, and a monospace character is 0.6 of its font size wide), then
  // scaled down to the box with smoothing. Glyphs drawn straight at a few pixels each overlapped into stripes, and
  // blocks in their place read as dashes.
  function size(cols, rows) {
    canvas.width = Math.round(box.width * box.dpr);
    canvas.height = Math.round(box.height * box.dpr);
    const w = Math.max(8, Math.ceil(canvas.width / cols));
    cell = { w, h: 2 * w, cols, rows };
    big = new OffscreenCanvas(w * cols, 2 * w * rows);
    bctx = big.getContext('2d');
    glyph = new OffscreenCanvas(w + 2, 2 * w);
    const g = glyph.getContext('2d');
    g.font = 'bold ' + (2 * w) / 1.2 + 'px ' + box.fontFamily;
    const f = g.measureText('c');
    g.fillStyle = '#35f56a';
    g.fillText('c', 1, (2 * w - f.fontBoundingBoxAscent - f.fontBoundingBoxDescent) / 2 + f.fontBoundingBoxAscent);
    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';
    shown = [];
  }
  function draw() {
    const rows = text().split('\\n');
    if (!cell || rows.length !== cell.rows || rows[0].length !== cell.cols) size(rows[0].length, rows.length);
    rows.forEach((row, r) => {
      if (row === shown[r]) return;
      bctx.clearRect(0, r * cell.h, big.width, cell.h);
      for (let c = 0; c < row.length; c++)
        if (row[c] === 'c') bctx.drawImage(glyph, c * cell.w - 1, r * cell.h);
    });
    shown = rows;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.drawImage(big, 0, 0, canvas.width, canvas.height);
  }
  function tick(now) {
    if (wasm.logo_frame(now) || !cell) draw();
    (self.requestAnimationFrame || (f => setTimeout(() => f(performance.now()), 16)))(tick);
  }
  async function load(data) {
    const { instance } = await WebAssembly.instantiateStreaming(fetch(data.wasm));
    wasm = instance.exports;
    settings = new Float64Array(wasm.memory.buffer, wasm.logo_set.value, ${KNOBS.length});
    canvas = data.canvas;
    ctx = canvas.getContext('2d');
  }
  onmessage = async ({ data }) => {
    if (data.canvas) loaded = load(data);
    await loaded;
    if (data.width) {
      box = data;
      cell = null; // sized again on the next frame
    }
    if (data.set !== undefined) settings[data.set] = data.value;
    if (data.canvas) {
      tick(performance.now());
      postMessage('drawn');
    }
  };
`;

const STYLE = `
  :host { display: block; }
  .logo { position: relative; }
  canvas { position: absolute; left: 0; top: 0; width: 100%; height: 100%; pointer-events: none; }
  :host(.live) ::slotted(*) { visibility: hidden; }
  .knobs { display: none; position: absolute; right: 2%; top: 2%; width: 118px; padding: 3px 5px; box-sizing: border-box;
           font-size: 10px; line-height: 1.15; color: #ccc; background: #151515; border-radius: 4px;
           opacity: 0.98; transition: opacity 0.2s; }
  .knobs:hover, .knobs:focus-within { opacity: 1; }
  :host(.live) .knobs { display: block; }
  label { display: grid; grid-template-columns: 62px 1fr; align-items: center; column-gap: 0.25rem; }
  input { -webkit-appearance: none; appearance: none; width: 100%; height: 0.7rem; margin: 0; background: none; }
  input::-webkit-slider-runnable-track { height: 2px; background: #555; }
  input::-moz-range-track { height: 2px; background: #555; }
  input::-webkit-slider-thumb { -webkit-appearance: none; width: 6px; height: 6px; margin-top: -2px; border-radius: 50%;
                                background: #35f56a; }
  input::-moz-range-thumb { width: 6px; height: 6px; border: 0; border-radius: 50%; background: #35f56a; }
  .actions { display: flex; gap: 4px; margin-top: 3px; }
  button { font: inherit; white-space: nowrap; border-radius: 3px; padding: 0.05rem 0.3rem; cursor: pointer; }
  .export { color: #151515; background: #35f56a; border: 1px solid #35f56a; }
  .reset { color: #ccc; background: none; border: 1px solid #555; }
`;

class SnakepathToy extends HTMLElement {
  async connectedCallback() {
    const image = this.querySelector('img');
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) return;
    if (!window.Worker || !window.OffscreenCanvas || !WebAssembly.instantiateStreaming) return;
    if (image) await image.decode(); // so that the canvas has its size

    const root = this.attachShadow({ mode: 'open' });
    const knobs = KNOBS.map(([name]) => `<label>${name} <input type="range" min="0" max="1" step="0.001" value="0.5"></label>`);
    root.innerHTML = `<style>${STYLE}</style><div class="logo"><slot></slot><canvas aria-hidden="true"></canvas>` +
      (this.hasAttribute('controls') ? `<div class="knobs">${knobs.join('')}<div class="actions">` +
        '<button type="button" class="reset">reset</button>' +
        '<button type="button" class="export" title="Copies a command that compiles this sway in C, with snakepath, ' +
        'and runs it">export</button></div></div>' : '') +
      '</div>';

    const box = root.querySelector('.logo');
    const metrics = () => {
      const r = box.getBoundingClientRect();
      return { width: r.width, height: r.height, dpr: window.devicePixelRatio || 1,
               fontFamily: getComputedStyle(this).fontFamily };
    };
    const worker = new Worker(URL.createObjectURL(new Blob([WORKER], { type: 'text/javascript' })));
    const offscreen = root.querySelector('canvas').transferControlToOffscreen();
    const wasm = new URL('logo.wasm', import.meta.url).href;
    worker.postMessage({ canvas: offscreen, wasm, ...metrics() }, [offscreen]);
    worker.onmessage = () => this.classList.add('live');
    new ResizeObserver(() => worker.postMessage(metrics())).observe(box);

    const inputs = [...root.querySelectorAll('input')];
    const send = (input, i) => worker.postMessage({ set: i, value: value(KNOBS[i], +input.value) });
    inputs.forEach((input, i) => input.addEventListener('input', () => send(input, i)));
    root.querySelector('.reset')?.addEventListener('click', () => inputs.forEach((input, i) => {
      input.value = input.defaultValue;
      send(input, i);
    }));

    // export: a command that fetches snake.c (logo.c with the snakepath it uses), compiles it with these settings and
    // runs it
    root.querySelector('.export')?.addEventListener('click', async event => {
      const button = event.target; // after the await, the event's target is this element, not the button
      const settings = inputs.map((input, i) => +value(KNOBS[i], +input.value).toFixed(3));
      const line = 'curl -sSL https://snakepath.dev/snake.c | cc -std=c99 ' +
        `'-DLOGO_SETTINGS={${settings.join(',')}}' -xc - -o snake && ./snake`;
      try {
        await navigator.clipboard.writeText(line);
      } catch {
        window.prompt('Copy this command:', line); // where the page may not write to the clipboard
        return;
      }
      button.textContent = 'copied';
      setTimeout(() => { button.textContent = 'export'; }, 1500);
    });
  }
}

customElements.define('snakepath-toy', SnakepathToy);
