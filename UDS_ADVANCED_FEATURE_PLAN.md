# UDS 后置功能修改建议

## 目标

在当前 UDS 诊断页面已有基础能力之上，继续增强项目适配能力、刷写灵活性、数据可读性和测试交付能力。

当前已完成的基础能力包括：

- UDS 响应/NRC 可读解释
- 常用服务树模板
- Hex 输入校验
- 自动化流程断言与失败策略
- 流程 JSON 导入/导出
- 日志导出
- 基础 DTC 诊断页面

后续建议分批实施，避免一次性改动过大。

## 总体容错原则

### 输入容错

- 所有 Hex 输入必须支持空格、小写，并统一转为大写空格分隔格式。
- 所有地址、长度、DID、Routine ID、DTC 掩码必须做格式和范围校验。
- 配置项为空时必须使用明确默认值，不能让空值进入刷写流程。
- 用户输入非法时只阻止当前动作，不清空已有配置。

### 配置容错

- 导入配置失败时，不覆盖当前配置。
- 配置文件版本不匹配时给出提示，保留向后兼容策略。
- 未识别字段应忽略，不应导致整体导入失败。
- 导出失败时不改变 UI 状态。

### 通信容错

- UDS 请求发送失败时立即停止等待状态并记录错误。
- 响应超时、负响应、流控超时、多帧序号错误都必须清理当前传输状态。
- `0x78 Response Pending` 应延长等待，但必须有最大等待次数或总超时时间。
- 短响应、畸形响应不能导致越界访问。

### 刷写容错

- 启动刷写前必须检查固件文件、地址、长度、CRC、Routine ID、安全等级配置。
- 刷写期间锁定关键配置，避免中途修改通道、协议、请求 ID、响应 ID。
- 任一阶段失败后必须恢复 UI 控件和 Tester Present 状态。
- 中止刷写必须停止 ISO-TP 发送定时器、响应等待定时器和升级状态机。

### 文件容错

- 文件读取失败、写入失败、路径为空、文件被占用时必须提示。
- 导入 JSON 时先解析到临时对象，全部校验完成后再更新 UI。
- 部分非法记录可以跳过，但必须提示跳过数量。
- 不允许导入失败后留下半更新状态。

### 日志容错

- 错误日志应包含步骤号、服务号、NRC、文件路径或配置项名称。
- 不记录密钥、Token、私钥等敏感信息。
- 日志解释失败时保留原始 Hex。

## Batch A：刷写参数配置化

### 目标

让固件升级流程适配不同 ECU，减少代码内写死参数。

### 建议功能

- 安全等级配置：
  - 请求 Seed 子功能，例如 `0x01`、`0x03`
  - 发送 Key 子功能，例如 `0x02`、`0x04`
- 请求下载参数：
  - DataFormatIdentifier
  - AddressAndLengthFormatIdentifier
  - 地址字段长度
  - 长度字段长度
- 传输参数：
  - 默认 block size
  - 是否采用 ECU 返回的 block size
- Routine 配置：
  - 校验 Routine ID
  - 是否追加 CRC
  - CRC 大小端
- Reset 配置：
  - `11 01`
  - `11 03`

### 建议涉及文件

- `qt-example_32/CAN/udswidget.cpp`
- `qt-example_32/CAN/udswidget.h`
- `qt-example_32/CAN/udsclient.cpp`
- `qt-example_32/CAN/udsclient.h`

### 执行步骤

1. 在升级页面增加“刷写参数配置”区域。
2. 在 `UdsClient` 增加刷写配置结构体或 setter。
3. 将当前写死的安全等级、`0x34` 参数、`0x31` Routine、`0x11` Reset 改为读取配置。
4. 保留当前默认值，确保旧流程不受影响。
5. 启动升级前统一校验配置。

### 容错处理

- 安全等级必须成对出现，例如 Seed=`0x01` 时 Key 默认应为 `0x02`。
- 地址字段长度和长度字段长度只允许 1 到 4 字节。
- Routine ID 必须为 2 字节。
- CRC 关闭时不追加 CRC 字段。
- CRC 开启但固件为空时禁止启动。
- ECU 返回 block size 非法时使用默认 block size 并记录日志。
- 配置非法时阻止启动升级，不进入升级状态机。

## Batch B：Seed-Key 算法配置化

### 目标

替换当前示例 Seed-Key 算法，使安全访问更接近真实项目。

### 建议功能

- 算法模式：
  - 示例算法
  - 固定 Key
  - XOR
  - 加偏移
- Key 长度：
  - 2 字节
  - 4 字节
- 大小端：
  - Big Endian
  - Little Endian
- 与安全等级配置联动。

第一版不建议直接做 DLL/脚本插件，先预留接口即可。

### 建议涉及文件

- `qt-example_32/CAN/udswidget.cpp`
- `qt-example_32/CAN/udswidget.h`
- `qt-example_32/CAN/udsclient.cpp`
- `qt-example_32/CAN/udsclient.h`

### 执行步骤

1. 增加 Seed-Key 配置 UI。
2. 在 `UdsClient` 增加 Key 计算配置。
3. 替换当前固定 `calculateKey()` 逻辑。
4. 根据 key 长度生成对应字节。
5. 日志中只记录 seed 和算法模式，不记录完整 key，或提供脱敏显示。

### 容错处理

- 固定 Key 模式下，Key 长度必须和配置一致。
- XOR/偏移参数必须是合法 Hex 或数字。
- Seed 长度不足时中止安全访问。
- 算法计算失败时中止升级流程。
- 不记录完整密钥，避免敏感信息泄露。

## Batch C：DID 数据结构解析

### 目标

将 `22 DID` 响应从原始 Hex 转换为可读字段。

### 建议功能

- DID 配置表：
  - DID
  - 名称
  - 数据类型：ASCII、HEX、uint8、uint16、uint32
  - 偏移
  - 长度
  - 大小端
- `62 DID` 正响应自动解析。
- DID 配置 JSON 导入/导出。
- 未配置 DID 保留原始 Hex 显示。

### 建议涉及文件

- `qt-example_32/CAN/udswidget.cpp`
- `qt-example_32/CAN/udswidget.h`
- 可选新增配置文件：`did_profiles/*.json`

### 执行步骤

1. 增加 DID 解析配置表。
2. 增加 DID 配置导入/导出。
3. 在 UDS 响应入口识别 `0x62`。
4. 按配置解析字段并显示在表格或日志中。
5. 未匹配配置时保持当前显示。

### 容错处理

- DID 配置导入失败时不覆盖当前配置。
- 偏移和长度超出响应长度时标记解析失败，但保留原始 Hex。
- 数值解析失败时显示 `N/A`。
- ASCII 字段遇到不可打印字符时用 `.` 替代或显示 Hex。
- 重复 DID 配置时提示并保留第一条或让用户选择覆盖。

## Batch D：测试报告生成

### 目标

让自动化流程执行结果可交付、可归档。

### 建议功能

- 导出 Markdown 或 HTML 报告。
- 报告内容：
  - 执行时间
  - 请求 ID / 响应 ID
  - 协议类型
  - 通道
  - 每步名称、请求、响应、期望、匹配模式、结果
  - 总测试数、通过数、失败数
  - 失败原因

### 建议涉及文件

- `qt-example_32/CAN/udswidget.cpp`
- `qt-example_32/CAN/udswidget.h`

### 执行步骤

1. 增加“导出报告”按钮。
2. 复用当前流程表和统计数据。
3. 生成 Markdown 报告。
4. 可选支持 HTML 报告。
5. 导出完成后记录日志。

### 容错处理

- 流程为空时禁止导出。
- 文件写入失败时提示，不改变当前数据。
- 缺失单元格使用空字符串或默认值。
- 报告中对 HTML 特殊字符做转义。
- 导出报告不应清空日志或流程状态。

## Batch E：DTC 增强

### 目标

增强当前基础 DTC 页面，支持更多诊断场景。

### 建议功能

- DTC 状态位解释。
- 支持 `19 0A` 读取支持的 DTC。
- 支持按 DTC 读取快照数据。
- 支持按 DTC 读取扩展数据。
- DTC 表导出 CSV。

### 建议涉及文件

- `qt-example_32/CAN/udswidget.cpp`
- `qt-example_32/CAN/udswidget.h`

### 执行步骤

1. 增加 DTC 状态位解释函数。
2. 扩展 DTC 表格列。
3. 增加 `19 0A` 按钮。
4. 增加 DTC 表导出 CSV。
5. 后续再做快照和扩展数据。

### 容错处理

- 非 `59 02` / `59 0A` 响应不进入当前解析分支。
- DTC 响应长度不是 4 字节对齐时，解析完整记录并提示尾部未解析字节。
- DTC 表为空时禁止导出。
- CSV 导出时字段必须转义逗号和引号。

## Batch F：ECU Profile

### 目标

支持多 ECU、多项目快速切换配置。

### 建议功能

Profile 保存内容：

- 请求 ID
- 响应 ID
- 功能 ID
- 协议类型
- 通道
- Tester Present 配置
- 刷写参数
- Seed-Key 配置
- DID 配置
- DTC 配置

### 建议涉及文件

- `qt-example_32/CAN/udswidget.cpp`
- `qt-example_32/CAN/udswidget.h`
- 可选新增目录：`profiles/`

### 执行步骤

1. 定义 Profile JSON 结构。
2. 增加保存 Profile。
3. 增加加载 Profile。
4. 加载前校验全部字段。
5. 加载后调用 `onApplyConfig()` 更新 UDS Client。

### 容错处理

- Profile 导入失败时不覆盖当前配置。
- 部分字段缺失时使用默认值并记录日志。
- ID 字段非法时阻止导入。
- 协议/通道越界时使用当前 UI 配置。
- 加载 Profile 时如果正在刷写或流程运行，禁止加载。

## Batch G：CAN-FD 动态 DLC 策略切换

### 目标

在保持兼容的前提下，支持 CAN-FD 自动 DLC，减少无意义 padding。

### 建议功能

- 策略选项：
  - 固定 64 字节
  - 自动 DLC
- 默认保持固定 64 字节。
- 自动 DLC 按 CAN-FD 合法长度档位取整：
  - 8、12、16、20、24、32、48、64

### 建议涉及文件

- `qt-example_32/CAN/udsclient.cpp`
- `qt-example_32/CAN/udsclient.h`
- `qt-example_32/CAN/udswidget.cpp`
- `qt-example_32/CAN/udswidget.h`

### 执行步骤

1. UI 增加 CAN-FD DLC 策略选项。
2. `UdsClient` 增加策略配置。
3. 新增 `getCanFdFrameLength(int payloadLength)`。
4. 发送 SF/FF/CF/FC 时按策略选择长度。
5. 默认值保持固定 64 字节。

### 容错处理

- Classic CAN 不受此配置影响。
- 自动 DLC 算法必须保证不小于实际数据长度。
- 非法策略值回退到固定 64 字节。
- 切换策略时记录日志。
- 刷写过程中禁止切换策略。

## 推荐实施顺序

1. Batch A：刷写参数配置化
2. Batch B：Seed-Key 算法配置化
3. Batch C：DID 数据结构解析
4. Batch D：测试报告生成
5. Batch E：DTC 增强
6. Batch F：ECU Profile
7. Batch G：CAN-FD 动态 DLC 策略切换

## 每批验证建议

- `git diff --check`
- Qt/MinGW 编译
- 手动发送正常 UDS 请求
- 手动发送非法 Hex 输入
- 自动化流程运行
- 自动化流程失败策略
- 相关导入/导出文件不存在、无权限、格式非法场景
- 刷写相关批次必须额外验证中止、超时、负响应、发送失败路径
