// Test host only. Component recipes and demo exports come from the locked official source alias.
import React, { useState } from 'react';
import { createRoot } from 'react-dom/client';
import { flushSync } from 'react-dom';
import * as Antd from 'antd';
import * as Icons from '@ant-design/icons';
import zhCN from 'antd/locale/zh_CN';
import enUS from 'antd/locale/en_US';
import ButtonBasic from 'official-demo:components/button/demo/basic.tsx';
import InputClear from 'official-demo:components/input/demo/allowClear.tsx';
import InputAffixes from 'official-demo:components/input/demo/presuffix.tsx';
import SelectBasic from 'official-demo:components/select/demo/basic.tsx';
import SelectSearch from 'official-demo:components/select/demo/search.tsx';

const demos = {
  'components/button/demo/basic.tsx': ButtonBasic,
  'components/input/demo/allowClear.tsx': InputClear,
  'components/input/demo/presuffix.tsx': InputAffixes,
  'components/select/demo/basic.tsx': SelectBasic,
  'components/select/demo/search.tsx': SelectSearch,
};
const w = window as any;
const request = w.__request;
const definition = request.definition;
const fixture = definition.fixture;
const callbacks: any[] = [];
let step = '';
let time = 0;
let parentPolicy = fixture.parent_policy;
let generation = 1;
let revision = 0;
let actualControls: any = {};
const changePolicies = new Set<() => void>();
const sourceControls: any = {};
const popupOrigins = new Map<string, { registration: object, props: WeakMap<object, { type: any, key: any }> }>();
let multiAccept = true;
let pendingOptions: (() => void) | undefined;
let feedback = '';
let setFeedback: (value: string) => void;
const { Button, Input, Select, ConfigProvider, Flex, Space, Typography } = Antd;
const components: any = { Button, Input, Select };
const Demo = demos[definition.official.demo_path];

function emit(target: string, event: string, proposed: any, committed: any, accepted: boolean) {
  callbacks.push({ seq: callbacks.length, step, time_us: time, target, key: '', event,
    payload: { proposed: proposed ?? null, committed: committed ?? null, accepted },
    owner_generation: generation, layer: 'browser-input' });
}

function slot(value: any): any {
  if (value == null || typeof value === 'string' || typeof value === 'number') return value;
  if (value.icon && Icons[value.icon]) return React.createElement(Icons[value.icon]);
  if (value.control) return <Control identity={value.control} entry={fixture.props.controls[value.control]} includeCaption={false} />;
  throw new Error('unsupported declared slot');
}

function Control({ identity, entry, includeCaption = true }: any) {
  const registration = React.useRef({});
  const { component, width, localTheme, recipe, marginBlock, ...publicProps } = entry;
  const data = fixture.data[identity] ?? {};
  const slots = fixture.slots[identity] ?? {};
  const controlled = Object.hasOwn(data, 'value');
  const initial = controlled ? data.value : data.defaultValue;
  const [value, setValue] = useState(initial);
  const [open, setOpen] = useState(Boolean(publicProps.open ?? publicProps.defaultOpen));
  const [proposedValue, setProposedValue] = useState(initial);
  const [options, setOptions] = useState(data.options);
  const [query, setQuery] = useState('');
  const asyncGeneration = React.useRef(0);
  React.useEffect(() => () => { asyncGeneration.current++; }, []);
  React.useEffect(() => () => {
    if (actualControls[identity]?.registration === registration.current) delete actualControls[identity];
    if (sourceControls[identity]?.registration === registration.current) delete sourceControls[identity];
    if (popupOrigins.get(identity)?.registration === registration.current) popupOrigins.delete(identity);
  }, [identity]);
  const [, setPolicyVersion] = useState(0);
  React.useEffect(() => { const update = () => setPolicyVersion(x => x + 1); changePolicies.add(update);
    return () => { changePolicies.delete(update); }; }, []);
  const C = components[component];
  const props: any = { ...publicProps, 'data-testid': identity };
  if (width !== undefined || marginBlock !== undefined) props.style = {
    ...(width !== undefined ? { width } : {}), ...(marginBlock !== undefined ? { marginBlock } : {}) };
  if (component !== 'Button') {
    if (controlled) props.value = value;
    else if (initial !== undefined) props.defaultValue = initial;
    props.onChange = (event: any) => {
      const proposed = component === 'Input' ? event.target.value : event;
      const accepted = recipe === 'select-multiple' ? multiAccept : !controlled || parentPolicy === 'accept';
      flushSync(() => {
        setProposedValue(proposed);
        if (accepted) setValue(proposed);
      });
      emit(identity, 'change', proposed, accepted ? proposed : value, accepted);
      if (recipe === 'select-multiple') setFeedback?.(accepted ? '已接受多选' : '已拒绝多选');
      if (recipe === 'select-tags') setFeedback?.('标签数量=' + proposed.length);
    };
    props.onClear = () => emit(identity, 'clear', component === 'Input' ? '' : null, actualControls[identity]?.value ?? null, !controlled || parentPolicy === 'accept');
  }
  if (component === 'Button') {
    props.onClick = () => {
      if (recipe === 'select-focus') sourceControls['gallery.select.controlled-search']?.ref?.focus();
      if (recipe === 'select-toggle-open') sourceControls['gallery.select.controlled-open']?.toggle();
      if (recipe === 'select-dynamic-options') sourceControls['gallery.select.controlled-search']?.expand();
      if (recipe === 'select-authority') multiAccept = !multiAccept;
      if (recipe === 'select-prefix') setFeedback?.('已执行前缀操作');
      if (recipe === 'select-deliver') { pendingOptions?.(); pendingOptions = undefined; setFeedback?.('已处理合成选项；已取消的请求不会更新选择器'); }
      emit(identity, 'activate', null, null, true);
    };
    if (slots.prefix != null) props.icon = slot(slots.prefix);
  }
  if (component === 'Input') {
    if (slots.prefix != null) props.prefix = slot(slots.prefix);
    if (slots.suffix != null) props.suffix = slot(slots.suffix);
    props.onPressEnter = () => emit(identity, 'submit', value, value, true);
  }
  if (component === 'Select') {
    props.options = options;
    // Native searches labels, including the Chinese business labels. This is
    // a declared public-prop adaptation of the pinned official component.
    if (slots.prefix != null) props.prefix = slot(slots.prefix);
    const ref: any = React.createRef();
    props.ref = ref;
    sourceControls[identity] = { ref, registration: registration.current, toggle: () => setOpen(x => !x), expand: () => setOptions((current: any[]) =>
      current.length === 2 ? [...data.options, { value: 'device', label: '设备' }] : data.options) };
    props.onOpenChange = (next: boolean) => {
      const accepted = !Object.hasOwn(publicProps, 'open') || parentPolicy === 'accept';
      if (accepted) flushSync(() => setOpen(next));
      if (!next && accepted && recipe === 'select-async') asyncGeneration.current++;
      emit(identity, 'open-change', next, accepted ? next : open, accepted);
    };
    if (Object.hasOwn(publicProps, 'open')) props.open = open;
    if (publicProps.showSearch || publicProps.mode) props.onSearch = (next: string) => {
      setQuery(next);
      emit(identity, 'search', next, recipe === 'select-virtual' || recipe === 'select-async' ? next : value, true);
      if (recipe === 'select-async') {
        const ticket = ++asyncGeneration.current;
        pendingOptions = () => { if (ticket === asyncGeneration.current)
          setOptions([{ value: next + ' · synthetic', label: next + ' · synthetic' }]); };
      }
    };
    if (recipe === 'select-virtual') props.optionRender = (option: any) => <Typography.Text>{option.label}</Typography.Text>;
    if (recipe === 'select-tags') props.tagRender = (info: any) =>
      <Button size="small" disabled={info.disabled} onClick={info.onClose}>{info.label}</Button>;
    if (slots.popup) {
      props.popupRender = (origin: any) => {
        if (!React.isValidElement(origin)) throw new Error('actual popup origin is not a React element');
        let origins = popupOrigins.get(identity);
        if (origins?.registration !== registration.current) {
          origins = { registration: registration.current, props: new WeakMap() };
          popupOrigins.set(identity, origins);
        }
        origins.props.set(origin.props, { type: origin.type, key: origin.key });
        return <Flex vertical gap={4}>{origin}{slot(slots.popup)}</Flex>;
      };
      props.popupRender.sourceControl = slots.popup.control;
    }
  }
  React.useLayoutEffect(() => {
    actualControls[identity] = { component, width, publicProps, slots, data, value: value ?? null,
      proposed_value: proposedValue ?? null, controlled, open, query, consumedProps: props, registration: registration.current };
  });
  const control = <C {...props} data-source-recipe={recipe}>{component === 'Button' ? slot(slots.content) : undefined}</C>;
  const child = localTheme ? <ConfigProvider data-reference-local-theme={identity}
    theme={{ ...localTheme, ...(localTheme.algorithm ? { algorithm: Antd.theme.defaultAlgorithm } : {}) }}>{control}</ConfigProvider> : control;
  if (includeCaption && slots.caption != null) return <Flex vertical gap={fixture.container.gap}>
    <Typography.Text data-reference-caption={identity}>{slot(slots.caption)}</Typography.Text>{child}
  </Flex>;
  return child;
}

function App() {
  const container = fixture.container;
  const algorithms: any = { default: Antd.theme.defaultAlgorithm, dark: Antd.theme.darkAlgorithm,
    compact: Antd.theme.compactAlgorithm };
  const { algorithm = 'default', ...theme } = fixture.theme;
  const themeInput = { ...theme, algorithm: algorithms[algorithm],
    token: { ...theme.token, fontFamily: request.font_files.map((font: any, i: number) => request.font_files.findIndex((other: any) => other.identity === font.identity) === i ? 'ReferenceFont' + i : null).filter(Boolean).join(', '), ...(fixture.motion === 'disabled' ? { motion: false } : {}) } };
  function composition(node: any, identity = 'composition') : any {
    if (node.component === 'source-feedback') return <SourceFeedback key={identity} identity={identity} initial={node.content} />;
    if (node.component === 'control') return <Control key={node.target} identity={node.target}
      entry={fixture.props.controls[node.target]} includeCaption={false} />;
    const types: any = { Flex, Space, 'Typography.Text': Typography.Text };
    const C = types[node.component];
    const content = node.slot ? fixture.slots[node.target][node.slot] : node.content;
    const children = node.component === 'Typography.Text' ? content :
      node.children.map((child: any, index: number) => composition(child, identity + '.' + index));
    return <C key={identity} {...node.props} style={node.width === undefined ? undefined : { width: node.width }}
      data-reference-composition={identity} data-reference-caption={node.slot === 'caption' ? node.target : undefined}>{children}</C>;
  }
  const contents = definition.official.mode === 'official-demo' ? <Demo /> :
    fixture.container.layout.startsWith('composition-v') ? composition(fixture.props.composition) :
    <Flex gap={container.gap} vertical={container.layout === 'column'} wrap={container.layout === 'wrap'} align="flex-start">
      {Object.entries(fixture.props.controls).map(([identity, entry]) => <Control key={identity} identity={identity} entry={entry} />)}
    </Flex>;
  return <ConfigProvider direction={fixture.direction} locale={fixture.locale === 'zh-CN' ? zhCN : enUS}
    theme={themeInput}>
    <div data-testid={definition.native.sample_id} style={{ width: container.width, height: container.height === 0 ? undefined : container.height,
      padding: container.padding, boxSizing: 'border-box' }}>
      {contents}
    </div>
  </ConfigProvider>;
}

function SourceFeedback({ identity, initial }: any) {
  const [value, update] = useState(initial);
  setFeedback = next => { feedback = next; update(next); };
  return <Typography.Text data-reference-composition={identity} data-source-feedback="select-multi">{value}</Typography.Text>;
}

const root: any = createRoot(document.getElementById('root')!);
flushSync(() => root.render(<App />));

function committedFibers() {
  const result: any[] = [];
  const parents = new Map<any, any>();
  function visit(fiber: any, parent: any = null) {
    if (!fiber) return;
    // A bailed-out child can retain a return link to an older parent object.
    // Record ancestry from the actual current tree's child/sibling traversal.
    parents.set(fiber, parent);
    const type = fiber.elementType ?? fiber.type;
    for (const [name, C] of Object.entries({ ...components, Flex, Space, 'Typography.Text': Typography.Text, ConfigProvider, OfficialDemo: Demo })) {
      if (type === C || fiber.type === C) {
        const props = fiber.memoizedProps ?? {};
        const consumed: any = {};
        for (const [key, value] of Object.entries(props)) {
          if (key === 'popupRender' && typeof value === 'function') {
            consumed.applied_popup = { control: (value as any).sourceControl };
          }
          if (['prefix', 'suffix', 'icon'].includes(key) && React.isValidElement(value)) {
            const icon = Object.entries(Icons).find(([, C]) => C === value.type)?.[0];
            consumed[key] = icon ? { icon } : value.type === Control ? { control: (value.props as any).identity } : { unsupported_slot_type: true };
            continue;
          }
          if ((key !== 'children' || typeof value === 'string' || typeof value === 'number') && typeof value !== 'function' && key !== 'locale' && key !== 'algorithm') {
            try { consumed[key] = JSON.parse(JSON.stringify(value)); } catch { /* React slot objects below */ }
          }
        }
        if (name === 'ConfigProvider') {
          consumed.actual_locale = props.locale?.locale ?? null;
          consumed.actual_algorithm = Object.entries({ default: Antd.theme.defaultAlgorithm, dark: Antd.theme.darkAlgorithm, compact: Antd.theme.compactAlgorithm }).find(([, algorithm]) => algorithm === props.theme?.algorithm)?.[0] ?? null;
        }
        result.push({ component: name, target: props['data-testid'] ?? null, props: consumed, fiber });
      }
    }
    visit(fiber.child, fiber);
    visit(fiber.sibling, parent);
  }
  visit(root._internalRoot.current);
  return { nodes: result, parents };
}

function fibers() {
  return committedFibers().nodes.map(({ fiber, ...value }) => value);
}

const hostTokens = new WeakMap<object, number>();
let nextHostToken = 0;
let querySequence = 0;
const firstObserved = new Map<string, number>();
const scopeVersions = new WeakMap<object, Map<string, { signature: string, generation: number }>>();
function hostToken(host: object): string {
  if (!hostTokens.has(host)) hostTokens.set(host, ++nextHostToken);
  return 'host-' + hostTokens.get(host);
}
function scopeGeneration(host: object, kind: string, applied: any): number {
  let versions = scopeVersions.get(host);
  if (!versions) { versions = new Map(); scopeVersions.set(host, versions); }
  const signature = JSON.stringify(applied);
  const old = versions.get(kind);
  const current = old?.signature === signature ? old : { signature, generation: (old?.generation ?? 0) + 1 };
  versions.set(kind, current);
  return current.generation;
}
function inputTargets(): string[] {
  const controls = fixture.props.controls ?? {};
  const result = definition.native.targets.filter((target: string) => Object.hasOwn(controls, target));
  function add(target: string) {
    if (result.includes(target)) return;
    result.push(target);
    for (const key of ['prefix', 'suffix', 'popup']) {
      const child = fixture.slots[target]?.[key]?.control;
      if (child) add(child);
    }
  }
  function visit(node: any) { if (node.component === 'control') add(node.target); node.children?.forEach(visit); }
  if (fixture.props.composition) visit(fixture.props.composition);
  else Object.keys(controls).forEach(add);
  for (let i = 0; i < result.length; ++i) for (const key of ['prefix', 'suffix', 'popup']) {
    const child = fixture.slots[result[i]]?.[key]?.control;
    if (child) add(child);
  }
  return result;
}
function fiberOwns(parent: any, child: any, parents: Map<any, any>): boolean {
  for (let current = child; current; current = parents.get(current)) if (current === parent) return true;
  return false;
}
function hosts(fiber: any): Element[] {
  const result: Element[] = [];
  function visit(node: any) {
    if (!node) return;
    if (node.stateNode instanceof Element) { if (node.stateNode.isConnected) result.push(node.stateNode); return; }
    for (let child = node.child; child; child = child.sibling) visit(child);
  }
  visit(fiber);
  return result;
}

function consumedInputs(queryStep = 'setup', initial = false) {
  const { nodes: committed, parents } = committedFibers();
  const controls: any = {};
  const live: any = {};
  const contexts: any = {};
  const themes: any = {};
  const raw: any = {};
  const current: any = {};
  const sequence = ++querySequence;
  const ordered = inputTargets();
  for (const fiber of committed) {
    if (!fiber.target || !Object.hasOwn(components, fiber.component)) continue;
    const connected = hosts(fiber.fiber).map(host => host.getAttribute('data-testid') === fiber.target ? host :
      host.querySelector(`[data-testid="${CSS.escape(fiber.target)}"]`)).find(host => host?.isConnected);
    if (!connected) continue;
    if (raw[fiber.target]) throw new Error('duplicate committed live input: ' + fiber.target);
    raw[fiber.target] = fiber;
    live[fiber.target] = connected;
    const props: any = {};
    const data: any = {};
    const slots: any = {};
    const actual = fiber.props;
    if (actual.style && Object.keys(actual.style).some(key => !['width', 'marginBlock'].includes(key)))
      throw new Error('unknown actual public layout style: ' + fiber.target);
    for (const [key, value] of Object.entries(actual)) {
      if (['value', 'defaultValue', 'options'].includes(key)) data[key] = value;
      else if (fiber.component === 'Button' && key === 'children') slots.content = value;
      else if (fiber.component === 'Button' && key === 'icon') slots.prefix = value;
      else if (['Input', 'Select'].includes(fiber.component) && ['prefix', 'suffix'].includes(key)) slots[key] = value;
      else if (key === 'applied_popup') slots.popup = value;
      else if (!['children', 'style'].includes(key) && !key.startsWith('data-')) props[key] = value;
    }
    const caption = committed.find(node => node.component === 'Typography.Text' && node.props['data-reference-caption'] === fiber.target);
    if (caption) slots.caption = caption.props.children;
    if (actual['data-source-recipe']) props.recipe = actual['data-source-recipe'];
    const local = committed.find(node => node.component === 'ConfigProvider' && node.props['data-reference-local-theme'] === fiber.target);
    if (local) {
      props.localTheme = { ...local.props.theme };
      if (local.props.actual_algorithm) props.localTheme.algorithm = local.props.actual_algorithm;
    }
    controls[fiber.target] = { component: fiber.component, props, data, slots,
      ...(actual.style && Object.hasOwn(actual.style, 'width') ? { width: actual.style.width } : {}),
      ...(actual.style && Object.hasOwn(actual.style, 'marginBlock') ? { marginBlock: actual.style.marginBlock } : {}),
      source: 'committed React component memoizedProps' };
    const providers: any[] = [];
    for (let ancestor = parents.get(fiber.fiber); ancestor; ancestor = parents.get(ancestor)) {
      const provider = committed.find(value => value.fiber === ancestor && value.component === 'ConfigProvider');
      if (provider) providers.unshift(provider);
    }
    if (!providers.length) throw new Error('actual input has no committed Provider ancestry');
    let locale = '', direction = '', algorithm = 'default', color = '#ffffff';
    let select: any = {};
    for (const provider of providers) {
      locale = provider.props.actual_locale ?? locale;
      direction = provider.props.direction ?? direction;
      algorithm = provider.props.actual_algorithm ?? algorithm;
      color = provider.props.theme?.token?.colorBgContainer ?? color;
      select = { ...select, ...provider.props.theme?.components?.Select };
    }
    const providerHost = hosts(providers[providers.length - 1].fiber)[0];
    if (!providerHost) throw new Error('Provider has no committed connected host');
    locale = ({ 'zh-cn': 'zh-CN', en: 'en-US', 'en-us': 'en-US' } as any)[locale] ?? locale;
    const config = { locale, format_locale: locale, direction };
    const theme = { algorithms: [algorithm], colorBgContainer: color, Select: select };
    contexts[fiber.target] = { config_scope: hostToken(providerHost) + ':config', theme_scope: hostToken(providerHost) + ':theme',
      config_generation: scopeGeneration(providerHost, 'config', config), theme_generation: scopeGeneration(providerHost, 'theme', theme), ...config };
    themes[fiber.target] = { source: 'committed ConfigProvider ancestry and applied public Theme', ...theme,
      select_option_height: select.optionHeight ?? 32 };
    const input: any = connected.matches('input,textarea') ? connected : connected.querySelector('input,textarea');
    const slotHost = fiber.component === 'Input' ? connected.closest('.ant-input-affix-wrapper') ?? connected : connected;
    if (fiber.component === 'Button' && Object.hasOwn(slots, 'content')) slots.content = connected.textContent ?? '';
    if (fiber.component === 'Input') {
      if (!input?.isConnected) throw new Error('actual Input has no connected editor: ' + fiber.target);
      for (const key of ['placeholder', 'disabled', 'readOnly', 'maxLength']) if (Object.hasOwn(props, key)) props[key] = input[key];
      for (const key of ['prefix', 'suffix']) if (typeof slots[key] === 'string') {
        const part = slotHost.querySelector('.ant-input-' + key);
        if (!part && slots[key] !== '') throw new Error('actual Input slot is absent: ' + fiber.target + '.' + key);
        slots[key] = part?.textContent ?? '';
      }
    }
    if (caption) {
      const captionHost = hosts(caption.fiber)[0];
      if (!captionHost?.isConnected) throw new Error('actual caption has no connected host');
      slots.caption = captionHost.textContent ?? '';
    }
    for (const value of Object.values(slots) as any[]) if (value?.icon) {
      const icon = value.icon.replace(/(Outlined|Filled|TwoTone)$/, '').replace(/([a-z])([A-Z])/g, '$1-$2').toLowerCase();
      if (!slotHost.querySelector(`[role="img"][aria-label="${CSS.escape(icon)}"] svg`))
        throw new Error('declared icon has no actual connected SVG slot: ' + fiber.target);
    }
    current[fiber.target] = { focused: connected === document.activeElement || connected.contains(document.activeElement) };
    if (fiber.component === 'Input') {
      current[fiber.target].value = input?.value ?? '';
      current[fiber.target].selection = input && typeof input.selectionStart === 'number'
        ? { start: input.selectionStart, end: input.selectionEnd, unit: 'UTF-16' } : null;
    }
    if (fiber.component === 'Select') {
      if (!input?.isConnected) throw new Error('actual Select has no connected editor: ' + fiber.target);
      const options: any[] = [];
      function flatten(rows: any[]) { for (const row of rows ?? []) { if (row.options) flatten(row.options); else options.push(row); } }
      flatten(data.options);
      // Pinned rc-component/select 1.10.1 renders the single value in Content;
      // SelectionItem is the multiple-mode tag, not the single-value node.
      const selectedNodes = connected.querySelectorAll(props.mode ? '.ant-select-selection-item' : '.ant-select-content-has-value');
      const selected = [...selectedNodes].map(item => {
        const label = item.getAttribute('title') ?? item.querySelector('.ant-select-selection-item-content')?.textContent ?? item.textContent;
        const matching = options.filter(option => option.label === label);
        if (matching.length !== 1) throw new Error('actual selected value is unobserved/ambiguous: ' + fiber.target);
        return matching[0].value;
      });
      current[fiber.target] = { ...current[fiber.target], value: props.mode ? selected : selected[0] ?? null,
        query: input.value, open: input.getAttribute('aria-expanded') === 'true' };
    }
  }
  const instances: any = {}, slotTargets: any = {}, slotComposition: any = {}, aliases: any = {};
  function project(fiber: any, origin?: any): any {
    if (fiber.stateNode?.nodeType === Node.TEXT_NODE && fiber.stateNode.isConnected)
      return { component: 'Typography.Text', content: fiber.stateNode.textContent };
    const official = committed.find(value => value.fiber === fiber);
    if (official?.target && live[official.target]) return { component: 'control', target: official.target };
    if (origin && fiber === origin) return { component: 'origin' };
    let childRoot = fiber;
    if (official && ['Flex', 'Space'].includes(official.component)) {
      const host = hosts(fiber)[0];
      function find(node: any): any {
        if (node.stateNode === host) return node;
        for (let child = node.child; child; child = child.sibling) { const found = find(child); if (found) return found; }
        return null;
      }
      childRoot = find(fiber) ?? fiber;
    }
    const children = [];
    for (let child = childRoot.child; child; child = child.sibling) {
      const projected = project(child, origin);
      if (projected !== null) children.push(projected);
    }
    if (official && ['Flex', 'Space'].includes(official.component)) {
      const p = official.props;
      const host = hosts(fiber)[0];
      if (!host) throw new Error('actual layout component has no connected host');
      const align = p.align ?? getComputedStyle(host).alignItems;
      return { component: official.component, ...(p.style?.width !== undefined ? { width: p.style.width } : {}),
        props: { vertical: p.vertical ?? p.direction === 'vertical', gap: p.gap ?? p.size ?? (official.component === 'Space' ? 8 : 0),
          wrap: p.wrap === true || p.wrap === 'wrap', align: ({ normal: 'stretch', 'flex-start': 'start', 'flex-end': 'end' } as any)[align] ?? align }, children };
    }
    if (official?.component === 'Typography.Text' && typeof official.props.children === 'string' && official.props['data-source-feedback']) {
      const host = hosts(fiber)[0];
      if (!host || host.textContent !== official.props.children) throw new Error('authored feedback has no current committed text');
      return { component: 'source-feedback', identity: official.props['data-source-feedback'], content: host.textContent };
    }
    if (official?.component === 'Typography.Text' && typeof official.props.children === 'string')
      return { component: 'Typography.Text', content: official.props.children };
    if (!children.length && !hosts(fiber).length) return null;
    return { component: 'retained', children };
  }
  function retainedPopup(parent: string): Element | null {
    const matches = [...parents.keys()].filter(fiber => {
      const host = fiber.stateNode;
      if (!(host instanceof Element) || !host.isConnected || !host.matches('.ant-select-dropdown')) return false;
      for (let owner = fiber; owner; owner = parents.get(owner)) {
        const control = ordered.find(target => raw[target]?.component === 'Select' && raw[target].fiber === owner);
        if (control) return control === parent;
      }
      return false;
    });
    if (matches.length > 1) throw new Error('multiple actual popup hosts for Select: ' + parent);
    return matches[0]?.stateNode ?? null;
  }
  for (const target of ordered) {
    const host = live[target];
    const instance: any = { status: host ? 'mounted' : 'missing', instance: '', owner_generation: generation,
      parent_target: '', slot: '', first_observed_query: 0 };
    if (host) {
      instance.instance = generation + ':' + hostToken(host);
      if (!firstObserved.has(instance.instance)) firstObserved.set(instance.instance, sequence);
      instance.first_observed_query = firstObserved.get(instance.instance);
      for (const parent of ordered) {
        if (parent === target || raw[parent]?.component !== 'Select' || !fiberOwns(raw[parent].fiber, raw[target].fiber, parents)) continue;
        const popup = retainedPopup(parent);
        const slot = popup?.isConnected && popup.contains(host) ? 'popup' : live[parent]?.contains(host) ? 'prefix' : '';
        if (slot) { instance.parent_target = parent; instance.slot = slot;
          (slotTargets[parent] ??= {})[slot] = [...(slotTargets[parent]?.[slot] ?? []), { control: target }]; }
      }
    } else {
      if (document.querySelector(`[data-testid="${CSS.escape(target)}"]`)?.isConnected)
        throw new Error('connected target has no actual official component instance: ' + target);
      instance.reason = 'no-live-host';
      for (const parent of ordered) {
        if (raw[parent]?.component === 'Select' && controls[parent]?.slots.popup?.control === target && current[parent]?.open === false) {
          instance.status = 'deferred'; instance.reason = 'closed-bound-popup';
        }
      }
    }
    instances[target] = instance;
  }
  const focusedOwner = w.__reference.semantic(document.activeElement);
  for (const target of ordered) if (current[target]) current[target].focused = focusedOwner === target;
  for (const parent of ordered) {
    if (!raw[parent] || raw[parent].component !== 'Select') continue;
    const popup = retainedPopup(parent);
    if (!popup?.isConnected) continue;
    if (controls[parent].slots.popup) aliases[parent + '.popup'] = { parent_target: parent,
      owner_generation: generation, instance: generation + ':' + hostToken(popup) };
    const wrapper = committed.find(value => value.component === 'Flex' && fiberOwns(raw[parent].fiber, value.fiber, parents) &&
      hosts(value.fiber).some(host => popup.contains(host)));
    if (!wrapper) continue;
    const originProps = popupOrigins.get(parent)?.props;
    if (!originProps) throw new Error('actual popup origin element is missing: ' + parent);
    const origins = [...parents.keys()].filter(fiber => {
      const registered = fiber.memoizedProps && originProps.get(fiber.memoizedProps);
      return registered && fiberOwns(raw[parent].fiber, fiber, parents) &&
        (fiber.elementType === registered.type || fiber.type === registered.type) && fiber.key === registered.key;
    });
    if (origins.length !== 1) throw new Error('actual committed popup origin boundary is missing/ambiguous: ' + parent);
    // Project the actual Flex, retaining its applied props and real child order.
    const tree = project(wrapper.fiber, origins[0]);
    slotComposition[parent] = { popup: tree };
  }
  const compositionRoot = committed.find(value => value.props['data-reference-composition'] === 'composition');
  const compositionTree = compositionRoot ? project(compositionRoot.fiber) : null;
  const feedbacks: any = {};
  function collectFeedback(node: any) {
    if (!node) return;
    if (node.component === 'source-feedback') {
      if (Object.hasOwn(feedbacks, node.identity)) throw new Error('duplicate authored feedback identity');
      feedbacks[node.identity] = node.content;
    }
    for (const child of node.children ?? []) collectFeedback(child);
  }
  collectFeedback(compositionTree);
  const actual = { version: 1, source: 'committed official fibers and connected hosts', phase: initial ? 'initial' : 'settled',
    query_seq: sequence, step: queryStep, page_generation: generation, ordered_targets: ordered, instances,
    controls: Object.fromEntries(Object.entries(controls).map(([id, c]: any) => [id, { component: c.component, ...c.props,
      ...(Object.hasOwn(c, 'width') ? { width: c.width } : {}), ...(Object.hasOwn(c, 'marginBlock') ? { marginBlock: c.marginBlock } : {}) }])),
    data: Object.fromEntries(Object.entries(controls).map(([id, c]: any) => [id, c.data])),
    slots: Object.fromEntries(Object.entries(controls).map(([id, c]: any) => [id, c.slots])),
    contexts, themes, current, aliases, feedbacks, slot_targets: slotTargets, slot_composition: slotComposition,
    composition: { source: 'retained component tree and applied layout models',
      host: compositionTree?.component === 'Flex' ? compositionTree.props : null,
      roots: compositionTree?.component === 'Flex' ? compositionTree.children : compositionTree ? [compositionTree] : [] } };
  return { controls, actual_inputs: actual, parent_policy: parentPolicy,
    provider: committed.find(node => node.component === 'ConfigProvider')?.props ?? {} };
}

function element(target: string): Element | null {
  if (target === 'sample') target = definition.native.sample_id;
  const direct = document.querySelector(`[data-testid="${CSS.escape(target)}"]`);
  if (direct) {
    if (actualControls[target]?.component === 'Input') return direct.closest('.ant-input-affix-wrapper') ?? direct;
    return direct;
  }
  const suffixes = ['.clear', '.popup', '.input'];
  for (const suffix of suffixes) {
    if (!target.endsWith(suffix)) continue;
    const base = element(target.slice(0, -suffix.length));
    if (!base) return null;
    if (suffix === '.clear') return base.closest('.ant-input-affix-wrapper')?.querySelector('.ant-input-clear-icon') ?? null;
    if (suffix === '.input') return base.querySelector('input') ?? (base.matches('input') ? base : null);
    const input = base.querySelector('input');
    const listbox = input?.getAttribute('aria-controls');
    return listbox ? document.getElementById(listbox)?.closest('.ant-select-dropdown') ?? null : null;
  }
  return null;
}

w.__reference = {
  callbacks, element, fibers, consumedInputs, controls: () => actualControls,
  instance: (host: Element) => generation + ':' + hostToken(host),
  semantic: (node: Element | null) => {
    if (!node) return '';
    let deepest: Element | null = null;
    let found = '';
    for (const identity of inputTargets()) {
      const base = element(identity);
      if (!base?.contains(node) && base !== node) continue;
      const clear = element(identity + '.clear');
      if (clear && (clear === node || clear.contains(node))) return identity + '.clear';
      if (!deepest || deepest.contains(base!)) { deepest = base; found = identity; }
    }
    return found || (node.closest('[data-testid]')?.getAttribute('data-testid') ?? '');
  },
  step: (id: string, timestamp: number) => { step = id; time = timestamp; },
  external: (values: any) => { parentPolicy = values.parent_policy; flushSync(() => { for (const update of changePolicies) update(); }); },
  revision: () => revision,
  generation: () => generation,
  unmount: () => { flushSync(() => root.unmount()); generation++; },
};
new MutationObserver(() => revision++).observe(document.getElementById('root')!,
  { subtree: true, childList: true, attributes: true, characterData: true });
new MutationObserver(() => revision++).observe(document.body, { childList: true, subtree: true });
w.__ready = true;
