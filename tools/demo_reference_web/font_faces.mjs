// Font-only worker helpers. Selection proof comes from Chromium, never fonts.ready.
export function fontAlias(fonts, index) {
  return 'ReferenceFont' + fonts.findIndex(font => font.identity === fonts[index].identity);
}

export function fontStyle(fonts) {
  const faces = fonts.map((font, index) => `@font-face{font-family:${fontAlias(fonts, index)};src:url('/font/${index}');font-weight:${font.weight};font-style:normal;font-display:block;}`).join('');
  const families = [...new Set(fonts.map((_, index) => fontAlias(fonts, index)))].join(',');
  return `${faces}*{font-synthesis:none;}body{margin:0;font-family:${families};}`;
}

export function selectedSources(fonts, sources, weight, slant) {
  if (!Number.isInteger(weight) || !['normal', 'upright'].includes(slant)) throw new Error('unlocked font weight/slant');
  return fonts.map(font => {
    const matches = sources.filter(source => source.weight === weight && source.slant === 'upright' &&
      source.names['6'].includes(font.postScriptName));
    if (!font.isCustomFont || !Number.isInteger(font.glyphCount) || font.glyphCount <= 0 || matches.length !== 1) {
      throw new Error('unexpected font source/fallback or wrong actual weight: ' + font.postScriptName +
        ' (computed=' + weight + ', slant=' + slant + ')');
    }
    const source = matches[0];
    return { ...font, identity: source.identity, sha256: source.sha256, face_index: source.face_index,
      weight: source.weight, slant: source.slant };
  });
}

export async function platformFontRuns(cdp, nodeId, sources, hasText) {
  // Element queries also collect descendant layout text, which may have a
  // different weight. Query each real Text node and its owning element's style,
  // including UA input shadow roots, without changing the DOM or its layout.
  const { node } = await cdp.send('DOM.describeNode', { nodeId, depth: -1, pierce: true });
  const runs = [];
  async function visit(current, styleNodeId) {
    if (current.nodeType === 1) styleNodeId = current.nodeId;
    if (current.nodeType === 3) {
      const { fonts } = await cdp.send('CSS.getPlatformFontsForNode', { nodeId: current.nodeId });
      if (fonts.length) {
        if (!styleNodeId) throw new Error('actual text font has no owning computed style');
        const { computedStyle } = await cdp.send('CSS.getComputedStyleForNode', { nodeId: styleNodeId });
        const style = Object.fromEntries(computedStyle.map(row => [row.name, row.value]));
        const weight = Number(style['font-weight']);
        runs.push({ node_id: current.nodeId, style_node_id: styleNodeId, weight, slant: style['font-style'],
          sources: selectedSources(fonts, sources, weight, style['font-style']) });
      }
    }
    for (const child of [...(current.children ?? []), ...(current.shadowRoots ?? [])]) await visit(child, styleNodeId);
  }
  await visit(node);
  if (hasText && !runs.length) throw new Error('missing actual selected-font evidence for nonempty text');
  return runs;
}
