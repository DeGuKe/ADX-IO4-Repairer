# Tests

`fixtures/capture.jsonl` 保存已脱敏的 64 字节 HID 报告测试向量，覆盖空闲、八圆环和侧键状态。每行只包含报告时间与十六进制数据。

`test_original.c` 模拟 `mai2hook.dll` 导出，用于隔离验证键盘输入合并、触摸 API 和 LED API 转发。它的运行位置限定在独立 `dist` 测试目录。

自动测试命令见仓库根目录 README。
