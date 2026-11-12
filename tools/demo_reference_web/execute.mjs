// Headless browser worker. No user session, DOM.click(), set-value input, or oracle generation.
import fs from 'node:fs';
import path from 'node:path';
import http from 'node:http';
import os from 'node:os';
import crypto from 'node:crypto';
import { createRequire } from 'node:module';

const [configPath] = process.argv.slice(2);
const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
const { prepared, request, output, build, provenance } = config;
const { definition, nonce, font_files: fonts } = request;
const fixture = definition.fixture;
const require = createRequire(path.join(prepared.install, 'package.json'));
const { chromium } = require('playwright');
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const fail = message => { throw new Error(message); };
const expect = (condition, message) => { if (!condition) fail(message); };
const records = [];
const errors = [];
const network = [];
const inputEvents = [];
let server, browser, context;
let sequence = 0;
let lastTimestamp = 0;
const completedSteps = [], completedCheckpoints = [];
const savedRects = new Map();
const outputStream = fs.createWriteStream(path.join(output, 'observations.jsonl'), { flags: 'wx' });

function record(type, step = '', values = {}) {
  const result = { schema_version: 1, type, nonce, case: definition.identity, step,
    adapter: definition.official.adapter_id, provenance,
    effective_inputs: {}, receipt: { dispatched: false, hit: '', consumed: false, layer: 'observer' },
    state: {}, callbacks: [], targets: [], captures: [], resources: {},
    completed_steps: [...completedSteps], completed_checkpoints: [...completedCheckpoints], ...values };
  records.push(result);
  outputStream.write(JSON.stringify(result) + '\n');
  return result;
}

const fontCSS = fonts.map((font, index) => `@font-face{font-family:ReferenceFont${index};src:url('/font/${index}');font-weight:${font.weight};font-style:normal;font-display:block;}`).join('');
const html = `<!doctype html><html><head><meta charset="utf-8"><style>${fontCSS}body{margin:0;font-family:${fonts.map((_, i) => 'ReferenceFont' + i).join(',')};}</style></head><body><div id="root"></div><script src="/request.js"></script><script src="/app.js"></script></body></html>`;
const assets = new Map([
  ['/', [Buffer.from(html), 'text/html; charset=utf-8']],
  ['/app.js', [fs.readFileSync(path.join(build, 'app.js')), 'application/javascript']],
  ['/request.js', [Buffer.from('window.__request=' + JSON.stringify(request).replaceAll('<', '\\u003c') + ';'), 'application/javascript']],
]);
if (fs.existsSync(path.join(build, 'app.css'))) assets.set('/app.css', [fs.readFileSync(path.join(build, 'app.css')), 'text/css']);
for (let i = 0; i < fonts.length; i++) assets.set('/font/' + i, [fs.readFileSync(fonts[i].path), 'font/ttf']);

try {
  server = http.createServer((req, res) => {
    const asset = assets.get(req.url);
    if (!asset) { errors.push('undeclared resource ' + req.url); res.writeHead(404).end(); return; }
    res.writeHead(200, { 'Content-Type': asset[1], 'Cache-Control': 'no-store' }).end(asset[0]);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const origin = 'http://127.0.0.1:' + server.address().port;
  const launchArgs = ['--force-color-profile=srgb', '--disable-lcd-text', '--disable-background-networking',
    '--disable-component-update', '--disable-default-apps', '--disable-smooth-scrolling', '--no-first-run', '--lang=' + fixture.locale];
  browser = await chromium.launch({ executablePath: prepared.browser, headless: true,
    args: launchArgs, downloadsPath: path.join(output, 'downloads'),
    env: { ...process.env, TMPDIR: path.join(output, 'tmp'), XDG_CACHE_HOME: path.join(output, 'cache'),
      XDG_CONFIG_HOME: path.join(output, 'config') }, timeout: 20000 });
  expect(browser.version() === '141.0.7390.37', 'runtime browser version mismatch');
  context = await browser.newContext({ viewport: { width: fixture.viewport.width, height: fixture.viewport.height },
    deviceScaleFactor: fixture.viewport.dpr, locale: fixture.locale, timezoneId: fixture.timezone,
    reducedMotion: fixture.motion === 'disabled' ? 'reduce' : 'no-preference', colorScheme: 'light',
    serviceWorkers: 'block', acceptDownloads: false });
  await context.route('**/*', async route => {
    const url = new URL(route.request().url());
    if (url.origin !== origin || !assets.has(url.pathname) || url.search || url.hash) {
      const message = 'execute network denied: ' + url.href;
      errors.push(message); network.push({ url: url.href, allowed: false });
      await route.abort('blockedbyclient');
    } else { network.push({ path: url.pathname, allowed: true }); await route.continue(); }
  });
  context.on('page', page => { if (context.pages().length > 1) { errors.push('unexpected popup page'); page.close(); } });
  const page = await context.newPage();
  page.setDefaultTimeout(5000);
  page.on('pageerror', error => errors.push('page error: ' + error.message));
  page.on('console', message => { if (message.type() === 'error') errors.push('console: ' + message.text()); });
  await page.exposeFunction('__recordInput', event => inputEvents.push(event));
  await page.addInitScript(seed => {
    let randomState = seed >>> 0;
    Math.random = () => { randomState = (Math.imul(1664525, randomState) + 1013904223) >>> 0; return randomState / 4294967296; };
    window.__seedProfile = { input: seed, algorithm: 'lcg32-v1' };
    window.__inputLog = [];
    for (const kind of ['pointermove', 'pointerdown', 'pointerup', 'keydown', 'keyup', 'beforeinput', 'input', 'wheel']) {
      window.addEventListener(kind, event => {
        const owner = event.target.closest?.('[data-testid]');
        const input = { type: event.type, trusted: event.isTrusted, target: window.__reference?.semantic(event.target) ?? owner?.getAttribute('data-testid') ?? '',
          key: event.key ?? '', text: event.data ?? null, defaultPrevented: event.defaultPrevented };
        window.__inputLog.push(input);
        queueMicrotask(() => { input.defaultPrevented = event.defaultPrevented; });
      }, true);
    }
  }, fixture.seed);
  await page.clock.install({ time: new Date(fixture.clock_epoch_us / 1000) });
  await page.clock.pauseAt(new Date(fixture.clock_epoch_us / 1000 + 1));
  await page.goto(origin, { waitUntil: 'load' });
  await page.waitForFunction(() => window.__ready === true);
  await page.evaluate(async () => { await document.fonts.ready; });
  const cdp = await context.newCDPSession(page);
  await cdp.send('DOM.enable');
  await cdp.send('CSS.enable');
  await cdp.send('Animation.enable');
  await cdp.send('Animation.setPlaybackRate', { playbackRate: 0 });
  const realRAF = await page.evaluate(() => ({ ratio: devicePixelRatio, locale: navigator.language,
    timezone: Intl.DateTimeFormat().resolvedOptions().timeZone, seed: window.__seedProfile,
    width: innerWidth, height: innerHeight, reduced_motion: matchMedia('(prefers-reduced-motion: reduce)').matches }));
  expect(realRAF.ratio === fixture.viewport.dpr, 'actual DPR mismatch');
  expect(realRAF.locale === fixture.locale, 'actual locale mismatch');
  expect(realRAF.timezone === fixture.timezone, 'actual timezone mismatch');
  expect(realRAF.seed.input === fixture.seed && realRAF.width === fixture.viewport.width && realRAF.height === fixture.viewport.height, 'actual seed/viewport mismatch');
  expect(realRAF.reduced_motion === (fixture.motion === 'disabled'), 'actual reduced-motion mismatch');

  // Independently exercise JS timers and a compositor-sampled CSS animation. No wall-clock sleep.
  const canary = await page.evaluate(() => {
    const node = document.createElement('div');
    Object.assign(node.style, { position: 'fixed', left: '-20px', top: '-20px', width: '2px', height: '2px' });
    document.body.append(node);
    window.__clockCanary = { timer: false, node,
      animation: node.animate([{ opacity: 0 }, { opacity: 1 }], { duration: 1000, fill: 'both' }) };
    window.__clockCanary.animation.pause();
    window.__clockCanary.animation.currentTime = 500;
    setTimeout(() => window.__clockCanary.timer = true, 10);
    return Number(getComputedStyle(node).opacity);
  });
  expect(Math.abs(canary - 0.5) < 0.001, 'CSS animation time control canary');
  await page.clock.runFor(16);
  expect(await page.evaluate(() => window.__clockCanary.timer), 'JS timer canary did not run');
  await page.evaluate(() => window.__clockCanary.node.remove());
  await cdp.send('Page.enable');

  async function paint() {
    // Screenshot is a fresh browser compositor readback. JS RAF is separately advanced by the installed clock.
    await page.clock.runFor(32);
    await page.evaluate(() => {
      for (const animation of document.getAnimations()) {
        if (window.__request.definition.fixture.motion === 'controlled') throw new Error('active controlled CSS sampling requires the later clock adapter');
        animation.pause();
        animation.currentTime = 0;
        const metadata = animation.effect?.getTiming();
        if (!metadata || typeof metadata.duration !== 'number') throw new Error('unsupported CSS animation timing');
      }
      document.documentElement.getBoundingClientRect();
    });
  }

  const identity = await page.evaluate(() => ({ imports: window.__officialImports,
    fibers: window.__reference.fibers(), ...window.__reference.consumedInputs(),
    container: (() => { const element = window.__reference.element(window.__request.definition.native.sample_id);
      const rect = element.getBoundingClientRect(); return { bounds: { x: rect.x, y: rect.y, width: rect.width, height: rect.height },
        declared_height_style: element.style.height, overflow: getComputedStyle(element).overflow }; })() }));
  const demoPath = definition.official.demo_path;
  expect(identity.imports.some(item => item.path === demoPath && item.sha256 === definition.official.demo_sha256), 'actual official demo import absent');
  if (definition.official.mode === 'official-demo') {
    expect(identity.fibers.some(fiber => fiber.component === 'OfficialDemo'), 'official demo export did not execute');
  } else {
    if (fixture.container.layout === 'composition-v1') {
      function verifyNode(node, identity = 'composition') {
        if (node.component === 'control') return;
        const actual = identityFibers.find(f => f.component === node.component && f.props['data-reference-composition'] === identity);
        expect(Boolean(actual), 'actual official composition did not execute: ' + identity);
        for (const [key, value] of Object.entries(node.props ?? {})) {
          expect(JSON.stringify(actual.props[key]) === JSON.stringify(value), 'effective composition prop mismatch: ' + identity + '.' + key);
        }
        if (node.component === 'Typography.Text') {
          const content = node.slot ? fixture.slots[node.target][node.slot] : node.content;
          expect(actual.props.children === content, 'effective context text mismatch: ' + identity);
        } else node.children.forEach((child, index) => verifyNode(child, identity + '.' + index));
      }
      const identityFibers = identity.fibers;
      verifyNode(fixture.props.composition);
    }
    for (const [target, entry] of Object.entries(fixture.props.controls)) {
      const actual = identity.fibers.find(fiber => fiber.component === entry.component && fiber.target === target);
      expect(Boolean(actual), 'actual official component did not execute: ' + target);
      const { component, width, ...props } = entry;
      for (const [key, expected] of Object.entries(props)) {
        expect(JSON.stringify(actual.props[key]) === JSON.stringify(expected), 'effective public prop mismatch: ' + target + '.' + key);
      }
      if (width !== undefined) expect(actual.props.style?.width === width, 'effective public width mismatch');
      const slots = fixture.slots[target] ?? {};
      if (entry.component === 'Button' && slots.content != null) expect(actual.props.children === slots.content, 'effective content mismatch');
      if (entry.component === 'Input') {
        for (const key of ['prefix', 'suffix']) if (typeof slots[key] === 'string') expect(actual.props[key] === slots[key], 'effective affix mismatch');
      }
      const data = fixture.data[target] ?? {};
      if (data.options) expect(JSON.stringify(actual.props.options) === JSON.stringify(data.options), 'effective options mismatch');
      for (const key of ['value', 'defaultValue']) {
        if (Object.hasOwn(data, key)) expect(JSON.stringify(actual.props[key]) === JSON.stringify(data[key]), 'effective initial data mismatch: ' + key);
      }
    }
  }
  const browserSession = await browser.newBrowserCDPSession();
  const systemInfo = await browserSession.send('SystemInfo.getInfo');
  const gpuIdentityKeys = ['glVendor', 'glRenderer', 'glVersion', 'glImplementationParts', 'displayType',
    'glWsVendor', 'glWsVersion', 'skiaBackendType', 'subpixelFontRendering', 'directRenderingVersion', 'sandboxed'];
  const actualGPU = { devices: systemInfo.gpu.devices,
    aux_attributes: Object.fromEntries(gpuIdentityKeys.filter(key => Object.hasOwn(systemInfo.gpu.auxAttributes, key)).map(key => [key, systemInfo.gpu.auxAttributes[key]])),
    feature_status: systemInfo.gpu.featureStatus };
  provenance.browser = { version: browser.version(), executable_sha256: prepared.browser_sha256,
    revision: '1194', headless: true, launch_args: launchArgs, renderer: systemInfo.gpu.auxAttributes?.glRenderer ?? 'Chromium headless backend not reported', actual_gpu: actualGPU,
    os: os.platform(), release: os.release(), arch: os.arch(), strict_mode: false, color_profile: 'sRGB-SDR' };
  provenance.actual_module_identity = identity.imports;
  provenance.executed_component_fibers = identity.fibers;
  provenance.clock_canary = { js_timer: true, css_animation_half_opacity: canary, css_control: 'Web Animations + CDP playbackRate=0',
    paint: 'fresh screenshot compositor readback', js_clock_is_not_css_clock: true };
  const canonical = value => JSON.stringify(value, (_, item) => item && typeof item === 'object' && !Array.isArray(item)
    ? Object.fromEntries(Object.keys(item).sort().map(key => [key, item[key]])) : item);
  const actualProps = Object.fromEntries(Object.entries(identity.controls).map(([id, control]) => [id,
    { component: control.component, ...control.props, ...(Object.hasOwn(control, 'width') ? { width: control.width } : {}) }]));
  const actualData = Object.fromEntries(Object.entries(identity.controls).filter(([, c]) => Object.keys(c.data).length).map(([id, c]) => [id, c.data]));
  const actualSlots = Object.fromEntries(Object.entries(identity.controls).filter(([, c]) => Object.keys(c.slots).length).map(([id, c]) => [id, c.slots]));
  if (definition.official.mode === 'official-component-adapted') {
    expect(canonical(actualProps) === canonical(fixture.props.controls), 'actual consumed props differ from fixture');
    expect(canonical(actualData) === canonical(fixture.data), 'actual consumed data differ from fixture');
    expect(canonical(actualSlots) === canonical(fixture.slots), 'actual consumed slots differ from fixture');
  }
  expect(identity.parent_policy === fixture.parent_policy, 'actual parent policy mismatch');
  expect(identity.provider.direction === fixture.direction, 'actual Provider direction mismatch');
  expect(identity.provider.actual_algorithm === (fixture.theme.algorithm ?? 'default'), 'actual Provider Theme algorithm mismatch');
  if (fixture.container.height === 0) expect(identity.container.declared_height_style === '', 'natural context height was forced');
  const effective = { ...fixture,
    ...(definition.official.mode === 'official-component-adapted' ? {
      props: { ...fixture.props, controls: actualProps }, data: actualData, slots: actualSlots } : {}),
    parent_policy: identity.parent_policy, actual_controls: identity.controls, actual_provider: identity.provider, actual_container: { ...identity.container,
      height_policy: fixture.container.height === 0 ? 'natural' : 'fixed' }, actual_environment: realRAF,
    actual_composition: identity.fibers.filter(f => f.props['data-reference-composition']) };
  record('hello', '', { effective_inputs: effective, resources: { network, isolated_context: true, desktop_fallback: false } });

  async function targetRect(target, allowSaved = false) {
    const result = await page.evaluate(target => {
      const element = window.__reference.element(target);
      if (!element) return null;
      const rect = element.getBoundingClientRect();
      const style = getComputedStyle(element);
      return { x: rect.x, y: rect.y, width: rect.width, height: rect.height,
        visible: style.visibility !== 'hidden' && style.display !== 'none' && rect.width > 0 && rect.height > 0 };
    }, target);
    if (result?.visible) { savedRects.set(target, result); return result; }
    if (allowSaved && savedRects.has(target)) return { ...savedRects.get(target), visible: false };
    fail('missing/hidden semantic target without saved geometry: ' + target);
  }

  async function snapshot(targets) {
    const observed = await page.evaluate(targets => {
      const r = window.__reference;
      const controls = r.controls();
      const rect = value => ({ x: value.x, y: value.y, width: value.width, height: value.height });
      const result = [];
      for (const identity of targets) {
        const element = r.element(identity);
        if (!element) throw new Error('missing checkpoint target: ' + identity);
        const bounds = element.getBoundingClientRect();
        const computed = getComputedStyle(element);
        const input = element.matches('input,textarea') ? element : element.querySelector('input,textarea');
        const control = controls[identity];
        const visible = computed.display !== 'none' && computed.visibility !== 'hidden' && bounds.width > 0 && bounds.height > 0;
        const selection = input && typeof input.selectionStart === 'number' ?
          { start: input.selectionStart, end: input.selectionEnd, direction: input.selectionDirection, unit: 'UTF-16' } : null;
        const state = { value: control?.component === 'Select' ? control.value : input?.value ?? control?.value ?? null,
          proposed_value: control?.proposed_value ?? null, focused: element === document.activeElement || element.contains(document.activeElement),
          disabled: Boolean(input?.disabled ?? element.disabled ?? element.getAttribute('aria-disabled') === 'true'),
          loading: element.classList.contains('ant-btn-loading') || element.classList.contains('ant-select-loading'),
          open: input?.getAttribute('aria-expanded') === 'true', selection, visible };
        const text = [];
        const walker = document.createTreeWalker(element, NodeFilter.SHOW_TEXT);
        while (walker.nextNode()) {
          const node = walker.currentNode;
          if (!node.textContent) continue;
          const range = document.createRange(); range.selectNodeContents(node);
          text.push({ content: node.textContent, boxes: [...range.getClientRects()].map(rect),
            font_family: getComputedStyle(node.parentElement).fontFamily, font_size: getComputedStyle(node.parentElement).fontSize,
            font_weight: getComputedStyle(node.parentElement).fontWeight, direction: getComputedStyle(node.parentElement).direction });
        }
        if (input) text.push({ content: input.value, editable: true, font_family: getComputedStyle(input).fontFamily,
          font_size: getComputedStyle(input).fontSize, font_weight: getComputedStyle(input).fontWeight });
        const icons = [...element.querySelectorAll('[role="img"],svg')].map(icon => ({ identity: icon.getAttribute('aria-label') ?? '',
          bounds: rect(icon.getBoundingClientRect()), markup: icon.outerHTML }));
        const tokens = {};
        for (const name of ['color', 'backgroundColor', 'borderColor', 'borderWidth', 'borderRadius', 'padding', 'gap',
          'fontFamily', 'fontSize', 'fontWeight', 'lineHeight', 'opacity', 'boxShadow', 'direction', 'overflow']) tokens[name] = computed[name];
        tokens.pseudo_before = { content: getComputedStyle(element, '::before').content,
          background: getComputedStyle(element, '::before').backgroundColor };
        let clip = { x: 0, y: 0, width: innerWidth, height: innerHeight };
        for (let parent = element.parentElement; parent; parent = parent.parentElement) {
          const style = getComputedStyle(parent);
          if (/(hidden|clip|scroll|auto)/.test(style.overflow + style.overflowX + style.overflowY)) {
            const bounds = parent.getBoundingClientRect();
            const x = Math.max(clip.x, bounds.x), y = Math.max(clip.y, bounds.y);
            clip = { x, y, width: Math.max(0, Math.min(clip.x + clip.width, bounds.right) - x),
              height: Math.max(0, Math.min(clip.y + clip.height, bounds.bottom) - y) };
          }
        }
        result.push({ identity, key: '', bounds: rect(bounds), clip,
          scroll: { x: element.scrollLeft, y: element.scrollTop, width: element.scrollWidth, height: element.scrollHeight },
          state, text, icons, tokens, owner_generation: r.generation(), scene_revision: r.revision() });
      }
      return { targets: result, callbacks: [...r.callbacks], revision: r.revision(),
        focus: window.__reference.semantic(document.activeElement),
        controls: Object.fromEntries(Object.entries(controls).map(([id, c]) => [id, { value: c.value, proposed_value: c.proposed_value }])) };
    }, targets);
    const documentNode = await cdp.send('DOM.getDocument', { depth: -1 });
    for (const target of observed.targets) {
      // Ask Chromium which actual faces painted this target; computed CSS alone is not font provenance.
      const selector = '[data-testid="' + target.identity.replaceAll('"', '\\"') + '"]';
      const node = await cdp.send('DOM.querySelector', { nodeId: documentNode.root.nodeId, selector });
      if (node.nodeId) {
        const actual = await cdp.send('CSS.getPlatformFontsForNode', { nodeId: node.nodeId });
        target.tokens.platform_fonts = actual.fonts;
        const knownFaces = provenance.actual_font_sources.flatMap(font => font.names['6']);
        expect(actual.fonts.every(font => font.isCustomFont && knownFaces.includes(font.postScriptName)), 'unexpected font source/fallback: ' + target.identity);
      }
    }
    return observed;
  }

  for (const action of definition.actions) {
    expect(action.time_us >= lastTimestamp, 'action clock regressed');
    if (action.time_us > lastTimestamp) await page.clock.runFor((action.time_us - lastTimestamp) / 1000);
    lastTimestamp = action.time_us;
    await page.evaluate(action => window.__reference.step(action.step, action.time_us), action);
    const before = await page.evaluate(() => window.__inputLog.length);
    const beforeEffects = await page.evaluate(target => {
      const element = window.__reference.element(target);
      return { callbacks: window.__reference.callbacks.length, focused: document.activeElement?.getAttribute('data-testid') ?? '',
        value: element?.value ?? null, scrollX: element?.scrollLeft ?? 0, scrollY: element?.scrollTop ?? 0,
        viewportScrollX: scrollX, viewportScrollY: scrollY };
    }, action.target);
    const parameters = action.parameters;
    let hit = '', consumed = false, sent = false;
    const pointer = ['pointer-move', 'pointer-down', 'pointer-up'].includes(action.kind);
    if (pointer || action.kind === 'wheel') {
      const rect = await targetRect(action.target, Boolean(action.preconditions.allow_hidden_saved));
      const inputRect = action.kind === 'wheel' ? { x: Math.max(0, rect.x), y: Math.max(0, rect.y),
        width: Math.max(0, Math.min(fixture.viewport.width, rect.x + rect.width) - Math.max(0, rect.x)),
        height: Math.max(0, Math.min(fixture.viewport.height, rect.y + rect.height) - Math.max(0, rect.y)) } : rect;
      expect(inputRect.width > 0 && inputRect.height > 0, 'no visible wheel/pointer target intersection');
      const x = inputRect.x + inputRect.width * (parameters.x_fraction ?? 0.5);
      const y = inputRect.y + inputRect.height * (parameters.y_fraction ?? 0.5);
      await page.mouse.move(x, y);
      hit = await page.evaluate(({ x, y }) => window.__reference.semantic(document.elementFromPoint(x, y)), { x, y });
      if (action.kind === 'pointer-down') await page.mouse.down({ button: parameters.button === 'primary' ? 'left' : 'right' });
      if (action.kind === 'pointer-up') await page.mouse.up({ button: parameters.button === 'primary' ? 'left' : 'right' });
      if (action.kind === 'wheel') {
        await page.evaluate(() => {
          window.__pendingWheelReceipt = new Promise(resolve => window.addEventListener('wheel', event =>
            resolve({ trusted: event.isTrusted }), { once: true, capture: true }));
        });
        await page.mouse.wheel(parameters.delta_x, parameters.delta_y);
        let timer;
        try {
          await Promise.race([page.evaluate(() => window.__pendingWheelReceipt), new Promise((_, reject) => {
            timer = setTimeout(() => reject(new Error('wheel delivery receipt timeout')), 5000);
          })]);
        } finally { clearTimeout(timer); }
      }
      sent = true;
    } else if (action.kind === 'key-down' || action.kind === 'key-up') {
      await targetRect(action.target, Boolean(action.preconditions.allow_hidden_saved));
      hit = await page.evaluate(() => window.__reference.semantic(document.activeElement));
      const names = { control: 'Control', shift: 'Shift', alt: 'Alt', meta: 'Meta' };
      const key = parameters.key === 'Space' ? ' ' : parameters.key;
      if (action.kind === 'key-down') {
        for (const modifier of parameters.modifiers) await page.keyboard.down(names[modifier]);
        await page.keyboard.down(key);
      } else {
        await page.keyboard.up(key);
        for (const modifier of [...parameters.modifiers].reverse()) await page.keyboard.up(names[modifier]);
      }
      sent = true;
    } else if (action.kind === 'text-commit') {
      await targetRect(action.target);
      hit = await page.evaluate(() => window.__reference.semantic(document.activeElement));
      expect(hit === action.target || hit === action.target.replace(/\.input$/, ''), 'text input target is not focused');
      await page.keyboard.insertText(parameters.text);
      sent = true;
    } else if (action.kind === 'external-props') {
      expect(Object.keys(parameters.values).length === 1 && ['accept', 'reject'].includes(parameters.values.parent_policy), 'unsupported external props');
      await page.evaluate(values => window.__reference.external(values), parameters.values);
      hit = action.target; sent = true; consumed = true;
    } else if (action.kind === 'advance-clock') {
      expect(parameters.to_us >= lastTimestamp, 'clock target regressed');
      await page.clock.runFor((parameters.to_us - lastTimestamp) / 1000);
      lastTimestamp = parameters.to_us; hit = action.target; sent = true; consumed = true;
    } else if (action.kind === 'checkpoint') {
      const declaration = definition.checkpoints.find(item => item.step === action.step);
      expect(Boolean(declaration), 'missing checkpoint declaration');
      await paint();
      const observed = await snapshot(declaration.targets);
      const pngPath = 'capture-' + sequence++ + '.png';
      const screenshot = await page.screenshot({ path: path.join(output, pngPath), animations: 'allow', caret: 'initial',
        scale: 'device', timeout: 5000 });
      expect(screenshot.readUInt32BE(16) === Math.round(fixture.viewport.width * fixture.viewport.dpr), 'capture width/DPR mismatch');
      expect(screenshot[24] === 8 && [2, 6].includes(screenshot[25]), 'unsupported PNG channel encoding');
      expect(screenshot.readUInt32BE(20) === Math.round(fixture.viewport.height * fixture.viewport.dpr), 'capture height/DPR mismatch');
      const capture = { path: pngPath, sha256: hash(screenshot), nonce, checkpoint: action.step,
        scene_revision: observed.revision, width: screenshot.readUInt32BE(16), height: screenshot.readUInt32BE(20),
        dpr: fixture.viewport.dpr, fence_completed: true, framebuffer_format: screenshot[25] === 2 ? 'PNG RGB8' : 'PNG RGBA8',
        channel_order: 'RGBA', alpha: screenshot[25] === 2 ? 'opaque' : 'straight', color_profile: 'sRGB-SDR' };
      completedCheckpoints.push(action.step);
      record('checkpoint', action.step, { effective_inputs: effective, targets: observed.targets, callbacks: observed.callbacks,
        state: { focused: observed.focus, controls: observed.controls }, captures: [capture],
        resources: { paint_receipt: { nonce, step: action.step, sequence, sha256: capture.sha256, fresh_readback: true, completion: 'Chromium screenshot Promise resolved; not a native GPU fence' },
          animations: await page.evaluate(() => document.getAnimations().map(a => ({ currentTime: a.currentTime,
            playState: a.playState, timing: a.effect?.getTiming(), id: a.id }))) } });
      hit = action.target; sent = true; consumed = true;
    } else if (action.kind === 'navigate') {
      expect(parameters.route === 'unmount', 'only explicit unmount navigation is implemented by reference adapter');
      await page.evaluate(() => window.__reference.unmount());
      sent = true; consumed = true; hit = action.target;
    } else fail('unsupported Web action: ' + action.kind);
    await paint();
    const inputs = await page.evaluate(before => window.__inputLog.slice(before), before);
    if (['pointer-move', 'pointer-down', 'pointer-up', 'key-down', 'key-up', 'text-commit', 'wheel'].includes(action.kind)) {
      const eventType = { 'pointer-move': 'pointermove', 'pointer-down': 'pointerdown', 'pointer-up': 'pointerup',
        'key-down': 'keydown', 'key-up': 'keyup', 'text-commit': 'beforeinput', wheel: 'wheel' }[action.kind];
      expect(inputs.some(event => event.type === eventType && event.trusted), 'input not executed/trusted: ' + action.step);
      const after = await page.evaluate(target => {
        const element = window.__reference.element(target);
        return { callbacks: window.__reference.callbacks.length, focused: document.activeElement?.getAttribute('data-testid') ?? '',
          value: element?.value ?? null, scrollX: element?.scrollLeft ?? 0, scrollY: element?.scrollTop ?? 0,
        viewportScrollX: scrollX, viewportScrollY: scrollY };
      }, action.target);
      consumed = inputs.some(event => event.defaultPrevented) || JSON.stringify(beforeEffects) !== JSON.stringify(after);
    }
    completedSteps.push(action.step);
    record('receipt', action.step, { receipt: { dispatched: sent, hit, consumed, layer: action.layer },
      resources: { input_events: inputs }, callbacks: await page.evaluate(() => [...window.__reference.callbacks]) });
    expect(errors.length === 0, errors.join('\n'));
  }
  expect(completedSteps.length === definition.actions.length, 'incomplete action execution');
  expect(completedCheckpoints.length === definition.checkpoints.length, 'incomplete checkpoint execution');
  record('complete', '', { effective_inputs: effective, callbacks: await page.evaluate(() => [...window.__reference.callbacks]),
    resources: { network, isolated_context: true, desktop_fallback: false, source_identity_verified: true } });
} catch (error) {
  fs.writeFileSync(path.join(output, 'error.json'), JSON.stringify({ error: String(error.stack ?? error), network, errors }, null, 2));
  process.stderr.write(String(error.stack ?? error) + '\n');
  process.exitCode = 4;
} finally {
  await context?.close().catch(() => {});
  await browser?.close().catch(() => {});
  await new Promise(resolve => server ? server.close(resolve) : resolve());
  await new Promise(resolve => outputStream.end(resolve));
}
