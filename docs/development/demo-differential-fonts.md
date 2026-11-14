# 差分验收专用字体

唯一资源锁为 `cmake/dependencies/RynUIDependencyLock.cmake`。普通产品的系统字体默认链与 CMake 下载入口不变；以下入口仅在显式准备差分测试时下载、实例化字体，所有二进制、工具与原始验证结果留在 ignored `out/`。

## 实际资源与字重

| 有序文件链 | 实际 PostScript face | Weight / slant / face index | 来源 |
| --- | --- | --- | --- |
| NotoSans-Regular.ttf | NotoSans-Regular | 400 / upright / 0 | 已锁官方静态 2.008 |
| NotoSansCJKsc-Regular.otf | NotoSansCJKsc-Regular | 400 / upright / 0 | 已锁官方静态 2.004 |
| NotoEmoji-Regular.ttf | NotoEmoji-Regular | 400 / upright / 0 | 官方单色 3.002 variable 的真实 `wght=400` 静态实例 |
| NotoSans-SemiBold.ttf | NotoSans-SemiBold | 600 / upright / 0 | 官方静态 2.008 |
| NotoSansCJKsc-SemiBold.ttf | NotoSansCJKsc-SemiBold | 600 / upright / 0 | 官方 2.004 variable 的真实 `wght=600` 静态实例 |
| NotoSans-Medium.ttf | NotoSans-Medium | 500 / upright / 0 | 同一已锁官方 commit 的静态 2.008 |
| NotoSansCJKsc-Medium.ttf | NotoSansCJKsc-Medium | 500 / upright / 0 | 同一已锁 2.004 variable 的真实 `wght=500` 静态实例 |

Latin 来源固定为 notofonts/noto-fonts `ffebf8c1ee449e544955a7e813c54f9b73848eac`。CJK 来源固定为 notofonts/noto-cjk `523d033d6cb47f4a80c58a35753646f5c3608a78`，其完整官方 SimplifiedChinese 静态目录只有 Thin、Light、DemiLight、Regular、Medium、Bold、Black，没有精确 600；因此使用同 commit 的 `Sans/Variable/TTF/NotoSansCJKsc-VF.ttf`。Latin 静态 600 SHA256 为 `87a8b90ece1e89746b544e4e086f85a3710e41485a8078f9be874837dfad45d5`，CJK 600 结果为 `4137d864b16d2670304ca58d1f884e20a2656d19230b8f497e0ce53584fa3d2a`。

Emoji 来源固定为官方 google/fonts `8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5` 的 `ofl/notoemoji/NotoEmoji[wght].ttf`；结果 SHA256 为 `1ada997ccb67bf2404828e25528e5257d6db10c80287e77684abf6737a5d198f`。它具有普通 `glyf` 轮廓，不依赖 COLR/CBDT/sbix/SVG color tables。U+1F642 的 Unicode cmap / nominal glyph 为 961，轮廓数为 5。两条原 regular 文件仍不覆盖该 scalar；旧 `abc🙂` 失败记录继续有效，新增文件的 CPU 检查不表示旧失败已经通过浏览器或 native 回归。

所有 source URL、输入与输出 SHA256、OFL-1.1 license URL/hash、version、face/weight/slant 均在中央锁。实例工具复用中央 `RYNUI_ICON_FONTTOOLS_*` 的 fontTools 4.60.1 / MIT / source SHA256，未新增产品依赖。BUNDLED 只从该校验过的 source archive 加载纯 Python `Lib/fontTools`；SYSTEM 必须显式指定该版本库目录，逐文件比对锁定源并拒绝未锁定编译扩展，不隐式退到系统安装版本。

实例 recipe 在 `font_instance.py`：固定全部 variable axes，`TTFont(recalcTimestamp=False)`，保留 source 时间，按确定顺序保存。CJK 先插值真实 gvar/HVAR outlines，再设置具体 Medium/SemiBold name records；不是将 regular OS/2 重标为 500/600。recipe 只接受 CJK 500/600 和 Emoji 400，不提供任意字重生成。结果必须匹配中央锁的实际输出 hash，SFNT 必须为对应静态 upright 字重。原 600 与 Emoji 400 输出 hash 不变。

Gallery 的 source/support paragraph 实际请求 upright 500，缺文件会使页面构建失败。因此默认显式链扩展至七文件，按原五文件顺序追加两条 Medium，未改 Theme 字重或普通 Gallery 默认链。Latin 500 SHA256 为 `7bbe267354704c6ad18bde24b1dbc756c8e4380ca1c3f3c25c45ec5c4471510b`，CJK 500 为 `feef2a772298a640a5d9a68185b12c068b8ce824b6edaa96d0979a290fe5a251`；source/license 仍取同一中央锁。700 不在本次明确集合内。

## 准备与 CPU 验证

使用受支持的 Python 3.10+；不需要配置或构建 C++。以下输出目录必须尚不存在。

```text
python -B tools/demo_reference_web/reference.py fonts --output out/demo-differential/fonts --fonttools-mode BUNDLED
python -B -m unittest discover -s tests -p demo_differential_font_resources_tests.py -v
python -B -m unittest discover -s tests -p demo_differential_font_requirements_tests.py -v
```

默认显式准备上表七个文件；`--weights` 仅接受明确的 400/500/600 集合，重复或未锁字重失败。`--weights 400 --without-emoji` 只准备旧两条静态 regular，不加载实例工具。已有校验归档可以用 `--offline` 重建；SYSTEM 使用 `--fonttools-mode SYSTEM --fonttools-lib <exact-source-compatible-Lib-directory>`。`font-files.json` 保持原四个身份字段加绝对 `path`，可直接交给两侧 adapter；`font-sources.json` 保留真实输入来源及派生记录。自定义目录通过环境变量 `RYNUI_WEB_FONTS=<out-relative-path>/font-files.json` 传给测试。

Windows PowerShell：

```powershell
$env:RYNUI_WEB_FONTS = 'out/demo-differential/fonts/font-files.json'
python -B -m unittest discover -s tests -p demo_differential_font_resources_tests.py -v
```

Linux shell：

```sh
RYNUI_WEB_FONTS=out/demo-differential/fonts/font-files.json python -B -m unittest discover -s tests -p demo_differential_font_resources_tests.py -v
```

测试读取真实字体文件，核验 hash、SFNT face/weight/slant/name、cmap 与单色轮廓，并从 variable 的 400/500/600 glyph sets 独立检查 CJK 500/600 轮廓；Latin 500 核对官方静态源与不同字重轮廓。缺字、缺 500/600/emoji、字重重标、重复 identity/weight、错 face/hash/slant/name、缺显式文件必须失败。纯 worker helper 测试使用 Node，不启动浏览器；纯 Python 受限运行应排除该单个方法。

## Web 与 native 对接边界

native scenario JSON 保持 16 MiB 上限；显式字体资源另限每文件 32 MiB、最多 16 文件，并保留 checkout/out 路径与实际 SHA256 校验。两者分开，才能接受锁定的约 21.6 MB CJK 600 文件；不接受路径逃逸或超限字体，也不改变普通产品字体读取接口。

相同 identity 的 400/500/600 文件共享同一个 CSS family alias，`@font-face` 使用各文件的真实 weight，关闭 synthesis。字体加载列表只证明资源加载；每个 checkpoint 还通过 Chromium `CSS.getPlatformFontsForNode` 按实际文本节点（含 input shadow tree）查询 selected PostScript faces，并按该节点实际 weight/slant 关联到唯一显式文件 SHA256/face。未知 face、system fallback、缺实际字体选择证据或只有 regular 的 500/600 选择都失败。system profile 可以使用真实且匹配的显式系统 face，保留其差异，不能冒称中央 pinned face。

native 继续使用既有显式 preferred file resolver 的 weight/slant 筛选。Emoji 这里只有真实 400，服务 Input 的实际 400 文本；若后续真实 600 文本需要 emoji，必须另锁真实 600 并声明相应需求。不能声称当前 emoji400 会在 600 链消费。初始 fixture / `font_files` 的有序四字段必须一致，新增链通过各自 owner 的正式 case 生成入口接入，不能从旧失败图猜 face 或删除 emoji 消红。

本小阶段在 Windows / CPython 3.10.11 验证了实际资源与 Python/worker-helper 合同；没有运行 C++、CTest、GPU、GUI、浏览器或 paired pilot。父侧还须执行两平台各自的真实字体 resolver、Web selected-face、原 `abc🙂` 与完整 pilot 回归。原有平台失败记录、tasks 和 support 状态不变。

父侧实际浏览器字体合同为 `python -B -m unittest discover -s tests -p demo_differential_web_font_tests.py -v`。该独立文件包含真实 400/500/600/emoji400 来源、缺 500/600、缺 emoji 的系统回退拒绝与 emoji400 不冒充 600 六例，缺工具非 skip；500 消费探针只改变测试请求的 strong token，生产 Theme 不变。新增两例未在本小阶段执行，不能并入 CPU 测试通过数量。中央字体锁与源码指纹改变后，父必须重新准备两平台七字体链和 reference cache，并重建 native manifest/二进制；Web toolchain 的 dependencies.lock.json 和 comparison runtime 的独立 lock 本次未改，同一 checkout 下既有安装仍需按原 verifier 检查，不因字体锁变化自动失效。
