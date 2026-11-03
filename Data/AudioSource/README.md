# 合成音效源文件

这些 11 个 WAV 由本项目 `Scripts/generate_combat_audio.py` 使用正弦波、啁啾、滤波噪声与音量包络原创合成，不包含第三方录音。固定随机种子可复现。

格式为单声道、22050 Hz、16-bit PCM，峰值约 0.58。运行脚本会重新生成本目录的源 WAV。用 `Scripts/create_combat_feedback_assets.py` 导入 UE4 后的资源位于 `/Game/AshenRift/Audio`；导入脚本不覆盖已有资产。

本机可使用 UE4 自带 Python：`D:/Epic Games/UE_4.27/Engine/Binaries/ThirdParty/Python3/Win64/python.exe`，不需要安装额外 Python 包。真实播放输出及混音测量见 `Docs/12_Combat_Feedback.md`。
