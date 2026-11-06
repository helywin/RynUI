# Empty artwork

本目录仅保留 Ant Design Empty artwork 的 MIT license；不保存上游源码副本。

- 输入：Ant Design 6.6.5 commit `4a39f54842eade4e565ab336ef6097cd7e723cdd`，`components/empty/empty.tsx`。
- 输入 SHA256：`401c430b497cb837fe55036e240e9245f6657e07b8cc44ff9ce8c7676ca08eba`（UTF-8、LF）。
- 原生适配：`src/icons/empty_illustration.inc`。上游路径转换成现有 `IconVector` 命令，按角色分层并保留阴影透明度；不包含 SVG/React runtime。
- 再生成：`python tools/import_empty_illustration.py <pinned-upstream-checkout>`。本批使用已有开发工具 FontTools 4.39.3；运行时不依赖 Python/FontTools。生成器只接受固定 commit，支持的输入限本 artwork 的 fill 与 translate，不作为通用 SVG importer。

原生图示使用方形 Icon 度量，保留上游 viewBox 的宽高比及居中留白。颜色根据 Empty image token、当前容器背景和深浅模式派生；不声称浏览器 CSS token 或跨 rasterizer 像素完全一致。
