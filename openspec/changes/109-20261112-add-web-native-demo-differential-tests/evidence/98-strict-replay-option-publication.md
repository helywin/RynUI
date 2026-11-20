# 严格报告与重放参数

Windows 原生 D3D12 配对运行生成了 Button、Input、Select 的比较产物，终稿发布因 `provenance.options` 含进程内校准凭据而失败。原目录仍保留初始失败报告，不能用于判定三例通过。

报告只排除内存中的 `profile_proof`，保存原有 profile 路径、文件哈希和重放参数。每次重放仍重新验证 profile。锁定 CPython 3.14 运行 runner/meta 测试 42/42 通过，覆盖带运行期凭据的严格报告发布。
