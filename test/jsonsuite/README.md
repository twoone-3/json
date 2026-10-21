# JSONTestSuite

本目录是 [nst/JSONTestSuite](https://github.com/nst/JSONTestSuite) 中
`test_parsing/` 的全部 318 个用例，原样复制，用于验证解析器的 JSON 符合性。

## 文件命名规则

| 前缀 | 含义 | 本项目的期望行为 |
| --- | --- | --- |
| `y_` | 合法 JSON | 必须解析成功 |
| `n_` | 非法 JSON | 必须被拒绝 |
| `i_` | 与实现相关（implementation defined） | 两种行为都允许，仅统计不判定 |

## 由谁使用

`main.cpp` 中的 `runJsonTestSuite()` 会遍历本目录：

- `y_*` 必须解析成功，且 `Value::dump()` 的结果必须能被重新解析（往返一致性）；
- `n_*` 必须被拒绝；
- `i_*` 只统计结果，不参与失败判定。

## 许可

JSONTestSuite 由 Nicolas Vasile（原 test262 团队）编写，置于公有领域
（public domain）。原始用例版权归其作者所有，此处仅作测试用途复制。
