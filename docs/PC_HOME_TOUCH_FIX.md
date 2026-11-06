# PC-HOME migration and touch-hold fix — 2026-10-19

## 迁移现象

同一块游戏硬盘迁移到 PC-HOME 后，游戏最终能够进入待机和游玩。TEST 触摸检测显示所有内屏区域在持续按住时快速亮灭，实际谱面的长按也会被切断。

## 单变量测试

Bridge 1.1、Segatools、AquaMai、COM 口、灵敏度和硬件固件保持原状态，只修改 F 盘 `mai2.ini`：

```diff
 [AM]
-DummyTouchPanel=1
+DummyTouchPanel=0
```

修复后 SHA-256：

```text
1ca7311bde5b5ee3b0022ce7f38759bdb1ffd4099c2a8501aa0692746526cacc
```

## 原因

本机游戏程序集的静态 IL 检查显示：DummyTouchPanel 开启时，`InputManager.UpdateTouchPanel` 每帧合并鼠标触摸，随后写入零状态并推进计数器。实体串口报告与游戏帧率独立，这个模拟分支会在真实报告之间插入释放状态。

设置为 `0` 后，状态更新依据真实触摸报告计数器，按住状态会持续保持到下一份真实数据。

## 验收

- TEST 中多个不同区域持续按住约 10 秒，指示稳定。
- 用户游玩含内屏长按的谱面，长按稳定。
- 实体圆环按键继续正常。
- Bridge 与 Segatools 文件哈希保持原值。

## 启动提示补充

AquaMai 指定版本中的 `NoAmDaemonAlert` 在游戏初始化超过 2000 ms 时显示提示，并在 `GameMain.LateInitialize` 销毁。迁移机器上短暂出现提示、随后进入游戏，符合这段初始化时序。

本案例把启动阶段、实体按钮 Bridge 与实体触摸配置分别记录，便于后续迁移快速定位。
