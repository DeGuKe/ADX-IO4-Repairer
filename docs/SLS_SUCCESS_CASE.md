# SLS success case — 2026-10-18

## 现场状态

ADX 配套 ConfigApp 和 Windows 都能读取实体按键，实体触摸与灯光也已工作。游戏通过 Segatools 虚拟 IO4 接收键盘输入，而实体圆环和侧键尚未进入游戏输入链路。

对照测试显示：

- 虚拟 IO4 开启时，AMDaemon 创建 `$io4\vid_0ca3`，键盘输入可用。
- 设置 `[io4] enable=0` 后，AMDaemon 输出 `NodeList::setup / ID 0 not found`。
- ADX `VID_0CA3 / PID_0021 / MI_04` 持续产生 64 字节 HID 报告。

## 实施

新增 `adx_io4_bridge.dll`，并只调整：

```ini
[mai2io]
path=adx_io4_bridge.dll
```

Segatools 虚拟 IO4保持开启。Bridge 读取 MI_04，把实体状态与原键盘状态合并，再交给既有虚拟 IO4。触摸和灯光继续由原 Hook 处理。

## 验收

| 验证项 | 结果 |
|---|---|
| 八个圆环 | INPUT TEST 逐一 ON/OFF 正常 |
| SELECT | 正常 |
| TEST | 实体长按进入 TEST 菜单 |
| SERVICE | 菜单导航正常 |
| COIN | 一次脉冲增加一次信用 |
| 触摸 | 正常 |
| 灯光 | 正常 |
| 游戏启动 | 进入待机与游戏流程 |

Bridge 1.0 历史 DLL SHA-256：

```text
00263fe9eb5bcdd849f37474d11caaf8b18859c5939b1d1b629af3692a3bd59a
```

## 结论

稳定方案是“实体 HID Bridge + 现有虚拟 IO4”。它保留原键盘、触摸与灯光路径，并补充实体按钮输入。
