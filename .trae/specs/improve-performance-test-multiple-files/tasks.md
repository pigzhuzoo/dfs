# DFS 多文件性能测试 - 实施计划

## [x] Task 1: 在 performance_test.py 中添加多文件 PUT 测试功能
- **Priority**: P0
- **Depends On**: None
- **Description**: 
  - 添加 `measure_multi_put_operation` 方法
  - 添加 `run_multi_file_test` 方法
  - 使用 Python 线程/进程并发执行多个文件上传
- **Acceptance Criteria Addressed**: [AC-1]
- **Test Requirements**:
  - `programmatic` TR-1.1: 可以成功执行 N 个文件的并发 PUT 操作
  - `programmatic` TR-1.2: 测试结果包含总耗时、总吞吐量、平均单文件吞吐量
  - `programmatic` TR-1.3: 所有文件完整性验证通过
- **Notes**: 使用 `concurrent.futures` 或 `threading` 模块实现并发

## [x] Task 2: 在 performance_test.py 中添加多文件 GET 测试功能
- **Priority**: P0
- **Depends On**: [Task 1]
- **Description**:
  - 添加 `measure_multi_get_operation` 方法
  - 实现并发下载多个已上传文件
- **Acceptance Criteria Addressed**: [AC-2]
- **Test Requirements**:
  - `programmatic` TR-2.1: 可以成功执行 N 个文件的并发 GET 操作
  - `programmatic` TR-2.2: 测试结果包含总耗时、总吞吐量、平均单文件吞吐量
  - `programmatic` TR-2.3: 所有下载文件完整性验证通过
- **Notes**: 与 PUT 测试使用相同的并发机制

## [x] Task 3: 添加命令行参数支持多文件测试配置
- **Priority**: P0
- **Depends On**: [Task 2]
- **Description**:
  - 添加 `--num-files` 参数来配置测试文件数量
  - 添加 `--multi-only` 或 `--include-multi` 参数来控制是否运行多文件测试
  - 更新 `run_comprehensive_test` 方法以支持多文件测试
- **Acceptance Criteria Addressed**: [AC-3, AC-6]
- **Test Requirements**:
  - `programmatic` TR-3.1: 使用 `--num-files` 参数可以指定测试的文件数量
  - `programmatic` TR-3.2: 默认同时运行单文件和多文件测试
  - `programmatic` TR-3.3: 可以选择性运行仅单文件或仅多文件测试
- **Notes**: 默认测试文件数量为 [2, 5, 10]

## [x] Task 4: 更新测试结果 JSON 结构，确保兼容性
- **Priority**: P1
- **Depends On**: [Task 3]
- **Description**:
  - 在结果中添加 `multi_file_tests` 数组
  - 保持现有单文件测试结果不变
  - 更新 `save_results` 方法
- **Acceptance Criteria Addressed**: [AC-4]
- **Test Requirements**:
  - `programmatic` TR-4.1: JSON 输出文件包含完整的多文件测试数据
  - `programmatic` TR-4.2: 旧版单文件测试结果结构保持不变
  - `programmatic` TR-4.3: 多文件测试数据包含文件数量、文件大小、总吞吐量等字段

## [x] Task 5: 更新 generate_plots.py 以支持多文件测试图表
- **Priority**: P1
- **Depends On**: [Task 4]
- **Description**:
  - 解析多文件测试数据
  - 添加多文件测试结果对比图表
  - 更新图表生成逻辑
- **Acceptance Criteria Addressed**: [AC-5]
- **Test Requirements**:
  - `human-judgement` TR-5.1: 生成包含多文件测试结果的图表
  - `programmatic` TR-5.2: 图表生成代码可以处理旧版（仅单文件）和新版结果
  - `human-judgement` TR-5.3: 图表清晰展示单文件与多文件性能对比

## [x] Task 6: 更新 run_performance_test.sh 脚本
- **Priority**: P2
- **Depends On**: [Task 3]
- **Description**:
  - 更新脚本以支持多文件测试选项
  - 添加适当的参数传递
- **Acceptance Criteria Addressed**: [AC-3]
- **Test Requirements**:
  - `programmatic` TR-6.1: 运行脚本时可以正确执行多文件测试
  - `programmatic` TR-6.2: 快速/完整/学术测试模式都包含多文件测试

## [x] Task 7: 端到端测试和验证
- **Priority**: P0
- **Depends On**: [Task 6]
- **Description**:
  - 运行完整的测试流程，验证所有功能
  - 确保测试结果正确且有意义
- **Acceptance Criteria Addressed**: [AC-1, AC-2, AC-3, AC-4, AC-5]
- **Test Requirements**:
  - `programmatic` TR-7.1: 完整测试流程可以成功运行并生成所有结果
  - `programmatic` TR-7.2: 所有测试通过，无错误
  - `human-judgement` TR-7.3: 测试结果合理，符合预期
