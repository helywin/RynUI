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
let changePolicy: (policy: string) => void;
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
  throw new Error('unsupported declared slot');
}

function Control({ identity, entry, includeCaption = true }: any) {
  const { component, width, ...publicProps } = entry;
  const data = fixture.data[identity] ?? {};
  const slots = fixture.slots[identity] ?? {};
  const controlled = Object.hasOwn(data, 'value');
  const initial = controlled ? data.value : data.defaultValue;
  const [value, setValue] = useState(initial);
  const [open, setOpen] = useState(Boolean(publicProps.open ?? publicProps.defaultOpen));
  const [proposedValue, setProposedValue] = useState(initial);
  const [, setPolicyVersion] = useState(0);
  changePolicy = () => setPolicyVersion(x => x + 1);
  const C = components[component];
  const props: any = { ...publicProps, 'data-testid': identity };
  if (width !== undefined) props.style = { width };
  if (component !== 'Button') {
    if (controlled) props.value = value;
    else if (initial !== undefined) props.defaultValue = initial;
    props.onChange = (event: any) => {
      const proposed = component === 'Input' ? event.target.value : event;
      const accepted = !controlled || parentPolicy === 'accept';
      flushSync(() => {
        setProposedValue(proposed);
        if (accepted) setValue(proposed);
      });
      emit(identity, 'change', proposed, accepted ? proposed : value, accepted);
    };
    props.onClear = () => emit(identity, 'clear', '', actualControls[identity]?.value ?? null, !controlled || parentPolicy === 'accept');
  }
  if (component === 'Button') {
    props.onClick = () => emit(identity, 'activate', null, null, true);
    if (slots.prefix != null) props.icon = slot(slots.prefix);
  }
  if (component === 'Input') {
    if (slots.prefix != null) props.prefix = slot(slots.prefix);
    if (slots.suffix != null) props.suffix = slot(slots.suffix);
    props.onPressEnter = () => emit(identity, 'submit', value, value, true);
  }
  if (component === 'Select') {
    props.options = data.options;
    props.onOpenChange = (next: boolean) => {
      const accepted = !Object.hasOwn(publicProps, 'open') || parentPolicy === 'accept';
      if (accepted) flushSync(() => setOpen(next));
      emit(identity, 'open-change', next, accepted ? next : open, accepted);
    };
    if (Object.hasOwn(publicProps, 'open')) props.open = open;
    if (publicProps.showSearch) props.onSearch = (query: string) => emit(identity, 'search', query, value, true);
  }
  actualControls[identity] = { component, width, publicProps, slots, data, value: value ?? null,
    proposed_value: proposedValue ?? null, controlled, open, consumedProps: props };
  const child = <C {...props}>{component === 'Button' ? slot(slots.content) : undefined}</C>;
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
    token: { ...theme.token, fontFamily: request.font_files.map((_: any, i: number) => 'ReferenceFont' + i).join(', '), ...(fixture.motion === 'disabled' ? { motion: false } : {}) } };
  function composition(node: any, identity = 'composition') : any {
    if (node.component === 'control') return <Control key={node.target} identity={node.target}
      entry={fixture.props.controls[node.target]} includeCaption={false} />;
    const types: any = { Flex, Space, 'Typography.Text': Typography.Text };
    const C = types[node.component];
    const content = node.slot ? fixture.slots[node.target][node.slot] : node.content;
    const children = node.component === 'Typography.Text' ? content :
      node.children.map((child: any, index: number) => composition(child, identity + '.' + index));
    return <C key={identity} {...node.props} data-reference-composition={identity} data-reference-caption={node.slot === 'caption' ? node.target : undefined}>{children}</C>;
  }
  const contents = definition.official.mode === 'official-demo' ? <Demo /> :
    fixture.container.layout === 'composition-v1' ? composition(fixture.props.composition) :
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

const root: any = createRoot(document.getElementById('root')!);
flushSync(() => root.render(<App />));

function fibers() {
  const result: any[] = [];
  function visit(fiber: any) {
    if (!fiber) return;
    const type = fiber.elementType ?? fiber.type;
    for (const [name, C] of Object.entries({ ...components, Flex, Space, 'Typography.Text': Typography.Text, ConfigProvider, OfficialDemo: Demo })) {
      if (type === C || fiber.type === C) {
        const props = fiber.memoizedProps ?? {};
        const consumed: any = {};
        for (const [key, value] of Object.entries(props)) {
          if (['prefix', 'suffix', 'icon'].includes(key) && React.isValidElement(value)) {
            const icon = Object.entries(Icons).find(([, C]) => C === value.type)?.[0];
            consumed[key] = icon ? { icon } : { unsupported_slot_type: true };
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
        result.push({ component: name, target: props['data-testid'] ?? null, props: consumed });
      }
    }
    visit(fiber.child);
    visit(fiber.sibling);
  }
  visit(root._internalRoot.current);
  return result;
}

function consumedInputs() {
  const committed = fibers();
  const controls: any = {};
  for (const fiber of committed) {
    if (!fiber.target || !Object.hasOwn(components, fiber.component)) continue;
    const props: any = {};
    const data: any = {};
    const slots: any = {};
    const actual = fiber.props;
    for (const [key, value] of Object.entries(actual)) {
      if (['value', 'defaultValue', 'options'].includes(key)) data[key] = value;
      else if (fiber.component === 'Button' && key === 'children') slots.content = value;
      else if (fiber.component === 'Button' && key === 'icon') slots.prefix = value;
      else if (fiber.component === 'Input' && ['prefix', 'suffix'].includes(key)) slots[key] = value;
      else if (!['children', 'style'].includes(key) && !key.startsWith('data-')) props[key] = value;
    }
    const caption = committed.find(node => node.component === 'Typography.Text' && node.props['data-reference-caption'] === fiber.target);
    if (caption) slots.caption = caption.props.children;
    controls[fiber.target] = { component: fiber.component, props, data, slots,
      ...(actual.style && Object.hasOwn(actual.style, 'width') ? { width: actual.style.width } : {}),
      source: 'committed React component memoizedProps' };
  }
  return { controls, parent_policy: parentPolicy, provider: committed.find(node => node.component === 'ConfigProvider')?.props ?? {} };
}

function element(target: string): Element | null {
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
  semantic: (node: Element | null) => {
    if (!node) return '';
    for (const identity of Object.keys(actualControls)) {
      const base = element(identity);
      if (!base?.contains(node) && base !== node) continue;
      const clear = element(identity + '.clear');
      if (clear && (clear === node || clear.contains(node))) return identity + '.clear';
      return identity;
    }
    return node.closest('[data-testid]')?.getAttribute('data-testid') ?? '';
  },
  step: (id: string, timestamp: number) => { step = id; time = timestamp; },
  external: (values: any) => { parentPolicy = values.parent_policy; if (changePolicy) flushSync(() => changePolicy(parentPolicy)); },
  revision: () => revision,
  generation: () => generation,
  unmount: () => { flushSync(() => root.unmount()); generation++; },
};
new MutationObserver(() => revision++).observe(document.getElementById('root')!,
  { subtree: true, childList: true, attributes: true, characterData: true });
new MutationObserver(() => revision++).observe(document.body, { childList: true, subtree: true });
w.__ready = true;
