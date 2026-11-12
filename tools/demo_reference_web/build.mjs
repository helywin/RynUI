import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { createRequire } from 'node:module';

const [preparedPath, output, appPath] = process.argv.slice(2);
const prepared = JSON.parse(fs.readFileSync(preparedPath, 'utf8'));
const require = createRequire(path.join(prepared.install, 'package.json'));
const esbuild = require('esbuild');
const source = path.resolve(prepared.source);
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const closure = {};
const marker = { name: 'locked-official-source', setup(build) {
  build.onResolve({ filter: /^\.\/version$/ }, args => {
    if (args.importer === path.join(source, 'components/version/index.tsx')) return { path: 'version', namespace: 'official-generated' };
  });
  build.onLoad({ filter: /.*/, namespace: 'official-generated' }, () => {
    const script = fs.readFileSync(path.join(source, 'scripts/generate-version.ts'));
    const packageBytes = fs.readFileSync(path.join(source, 'package.json'));
    closure['scripts/generate-version.ts'] = hash(script);
    closure['package.json'] = hash(packageBytes);
    const version = JSON.parse(packageBytes).version;
    return { contents: `export default '${version}';`, loader: 'ts' };
  });
  build.onResolve({ filter: /^official-demo:/ }, args => ({ path: path.join(source, args.path.slice(14)) }));
  build.onResolve({ filter: /^antd(?:\/|$)/ }, args => {
    const relative = args.path === 'antd' ? 'components/index.ts' : 'components/' + args.path.slice(5) + '.ts';
    return { path: path.join(source, relative) };
  });
  build.onLoad({ filter: /\.[cm]?[jt]sx?$/ }, args => {
    if (!args.path.startsWith(source + path.sep)) return;
    const bytes = fs.readFileSync(args.path);
    const name = path.relative(source, args.path).split(path.sep).join('/');
    closure[name] = hash(bytes);
    const receipt = JSON.stringify({ path: name, sha256: closure[name] });
    return { contents: bytes.toString('utf8') + `\n;globalThis.__officialImports ??= []; globalThis.__officialImports.push(${receipt});\n`,
      loader: args.path.endsWith('tsx') ? 'tsx' : args.path.endsWith('ts') ? 'ts' : 'jsx',
      resolveDir: path.dirname(args.path) };
  });
} };
fs.mkdirSync(output, { recursive: false });
const result = await esbuild.build({ entryPoints: [appPath], outfile: path.join(output, 'app.js'), bundle: true,
  format: 'iife', platform: 'browser', target: 'chrome141', jsx: 'transform', metafile: true,
  nodePaths: [path.join(prepared.install, 'node_modules')], plugins: [marker],
  define: { 'process.env.NODE_ENV': '"development"' }, logLevel: 'warning',
  loader: { '.svg': 'dataurl', '.png': 'dataurl', '.css': 'css' } });
const imports = Object.keys(result.metafile.inputs).filter(name => path.resolve(name).startsWith(source + path.sep));
if (!imports.length || !closure['components/button/Button.tsx'] || !closure['components/input/Input.tsx'] ||
    !closure['components/select/index.tsx']) throw new Error('official implementation missing from bundle closure');
fs.writeFileSync(path.join(output, 'closure.json'), JSON.stringify(closure, null, 2) + '\n');
fs.writeFileSync(path.join(output, 'metafile.json'), JSON.stringify(result.metafile, null, 2) + '\n');
