# DFS 多文件性能测试 - 产品需求文档

## Overview
- **Summary**: 增强分布式文件系统 (DFS) 的性能测试能力，从仅测试单个文件扩展到支持多文件的并发和批量操作测试，从而获得更全面、更具说服力的性能评估结果。
- **Purpose**: 解决当前性能测试的局限性，提供更符合实际使用场景的测试数据，支持学术研究和系统优化。
- **Target Users**: 研究人员、系统开发者、测试人员

## Goals
- 添加多文件并发 PUT/GET 测试功能
- 支持配置不同数量文件的批量测试
- 保持与现有单文件测试的兼容性
- 生成包含多文件测试结果的图表和报告
- 确保测试结果的可重复性和可靠性

## Non-Goals (Out of Scope)
- 不修改 DFS 核心功能代码
- 不实现分布式多客户端测试（仅单客户端多文件测试）
- 不改变现有项目的构建流程

## Background & Context
当前性能测试 (`performance_test.py`) 仅支持单个文件的 PUT/GET 操作测试，虽然覆盖了不同文件大小和类型，但无法模拟真实场景中同时上传/下载多个文件的情况，这使得性能评估不够全面。

## Functional Requirements
- **FR-1**: 支持同时上传 N 个测试文件的并发性能测试
- **FR-2**: 支持同时下载 N 个测试文件的并发性能测试
- **FR-3**: 允许配置测试文件数量（如 2、5、10、20 等）
- **FR-4**: 测试结果中新增多文件测试的统计数据（总吞吐量、平均单文件吞吐量等）
- **FR-5**: 更新图表生成模块以支持多文件测试结果可视化
- **FR-6**: 添加命令行参数来控制是否运行多文件测试

## Non-Functional Requirements
- **NFR-1**: 测试执行时间在可接受范围内（对于 20 个 50MB 文件的测试不超过 5 分钟）
- **NFR-2**: 测试代码清晰易维护，符合现有代码风格
- **NFR-3**: 测试结果格式与现有 JSON 结构兼容

## Constraints
- **Technical**: 使用 Python 3，保留现有测试框架，不引入新的外部依赖（除已有的 psutil、matplotlib 等）
- **Business**: 保持项目的学术研究定位，确保测试结果的严谨性
- **Dependencies**: 依赖现有的 `non_interactive_client.py` 和 `generate_plots.py`

## Assumptions
- 单客户端足以模拟典型的多文件操作场景
- 测试文件数量上限合理（不超过 20-50 个，避免过长测试时间）
- 系统资源足够支持多文件测试

## Acceptance Criteria

### AC-1: 多文件并发 PUT 测试
- **Given**: 测试环境已正确配置，DFS 服务器正在运行
- **When**: 执行多文件 PUT 测试（N 个文件，每个文件大小为 S MB）
- **Then**: 所有 N 个文件成功上传，测试记录总吞吐量、平均单文件吞吐量、总耗时
- **Verification**: `programmatic`

### AC-2: 多文件并发 GET 测试
- **Given**: N 个测试文件已成功上传到 DFS
- **When**: 执行多文件 GET 测试
- **Then**: 所有 N 个文件成功下载，测试记录总吞吐量、平均单文件吞吐量、总耗时
- **Verification**: `programmatic`

### AC-3: 测试参数配置
- **Given**: 用户启动性能测试
- **When**: 使用命令行参数指定文件数量
- **Then**: 测试按指定数量的文件执行
- **Verification**: `programmatic`

### AC-4: 结果报告兼容性
- **Given**: 测试已完成
- **When**: 保存测试结果到 JSON 文件
- **Then**: JSON 结构包含多文件测试数据，且与现有单文件测试结果格式兼容
- **Verification**: `programmatic`

### AC-5: 图表可视化
- **Given**: 包含多文件测试数据的 JSON 结果文件
- **When**: 执行图表生成
- **Then**: 生成包含多文件测试结果对比的图表
- **Verification**: `human-judgment`

## Open Questions
- 多文件测试的文件数量默认选项应该是多少？（建议：2, 5, 10）
- 是否需要支持混合文件大小的多文件测试？（建议：第一阶段先测试相同大小文件）
