#!/usr/bin/env bash
set -euo pipefail

COMMON_FILE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source "${COMMON_FILE}"

echo "[RUNME] PROJECT_DIR is ${PROJECT_DIR}"
echo "[RUNME] SCRIPTS_DIR is ${SCRIPTS_DIR}"

# 1. Install Third-Party Dependencies
bash "${SCRIPTS_DIR}/install/install_all.sh"

# 2. Datasets
bash "${SCRIPTS_DIR}/datasets/download_realworld_datasets.sh" # Download Datasets for X-Blossom

# 3. Build all Benchmarks and Profiling programs
bash "${SCRIPTS_DIR}/build_targets/build_all.sh"

# 4. Test RayJoin-Vk-Ns
#bash "${SCRIPTS_DIR}/test/run_test_ns_1.sh"
#bash "${SCRIPTS_DIR}/test/run_test_ns_2.sh"
#bash "${SCRIPTS_DIR}/test/run_test_ns_3.sh"
#bash "${SCRIPTS_DIR}/test/run_test_ns_1_dump.sh"
#bash "${SCRIPTS_DIR}/test/run_test_ns_2_dump.sh"

# 4. Test RayJoin-Vk
#bash "${SCRIPTS_DIR}/test/run_test_1.sh"
#bash "${SCRIPTS_DIR}/test/run_test_2.sh"
#bash "${SCRIPTS_DIR}/test/run_test_3.sh"
#bash "${SCRIPTS_DIR}/test/run_test_1_dump.sh"
#bash "${SCRIPTS_DIR}/test/run_test_2_dump.sh"

# 4. Test OptixNative
#bash "${SCRIPTS_DIR}/test/run_test_optix_native_basic_1.sh"
#bash "${SCRIPTS_DIR}/test/run_test_optix_native_basic_2.sh"

#bash "${SCRIPTS_DIR}/test/run_test_optix_native_1.sh"
#bash "${SCRIPTS_DIR}/test/run_test_optix_native_2.sh"
#bash "${SCRIPTS_DIR}/test/run_test_optix_native_3.sh" # OOM on laptop 4070
#bash "${SCRIPTS_DIR}/test/run_test_optix_native_4.sh" # OOM on laptop 4070

# 4. Test RayJoin-Vk-Ns & Optix-Ns
#bash "${SCRIPTS_DIR}/test/run_test_vk_optix_native_1.sh"
#bash "${SCRIPTS_DIR}/test/run_test_vk_optix_native_basic_1.sh"
#bash "${SCRIPTS_DIR}/test/run_test_vk_optix_native_basic_2.sh"

bash "${SCRIPTS_DIR}/test/run_vk_optix_native_1.sh"

# ==========================================================
# Start to produce results
#mkdir -p "tmp/logs"
#mkdir -p "data/results"

# 4. Table-1: GraphMetrics
#bash "${SCRIPTS_DIR}/experiments/table_1/RUN_graph_metrics.sh"
#
## 5. Table-2: Reuse Mechanism
#bash "${SCRIPTS_DIR}/experiments/table_2/RUN_xb_and_xb_pro.sh"
#bash "${SCRIPTS_DIR}/experiments/table_2/RUN_xb_pp_r_nr.sh"
#
## 6. Table-3: Load-Balancing Mechanism
#bash "${SCRIPTS_DIR}/experiments/table_3/run_cpu_loadbalance_all_datasets.sh"
#bash "${SCRIPTS_DIR}/experiments/table_3/run_gpu_loadbalance_all_datasets.sh"
#
## 7. Figure-6: Runtimes of BFS and XB
#bash "${SCRIPTS_DIR}/experiments/figure_6/RUN_xb_pro_time.sh"
#bash "${SCRIPTS_DIR}/experiments/figure_6/RUN_xb_pp_time.sh"
#bash "${SCRIPTS_DIR}/experiments/figure_6/RUN_bfs_ligra_time.sh"
#bash "${SCRIPTS_DIR}/experiments/figure_6/RUN_bfs_gunrock_time.sh"
#bash "${SCRIPTS_DIR}/experiments/figure_6/plot_figure_6.sh"
#
## 8. Table-5: Inst Rate for BFS-Ligra, XB-Pro, BFS-Gunrock, XB-PP
#bash "${SCRIPTS_DIR}/experiments/table_5/RUN_xb_pro_inst.sh"
#bash "${SCRIPTS_DIR}/experiments/table_5/RUN_bfs_ligra_inst.sh"
#bash "${SCRIPTS_DIR}/experiments/table_5/RUN_xb_pp_inst.sh"
#bash "${SCRIPTS_DIR}/experiments/table_5/RUN_bfs_gunrock_inst.sh"
#bash "${SCRIPTS_DIR}/experiments/table_5/generate_table_5.sh"
#
## 9. Figure-7: Inst Rate for XB-Pro-Node and XB-Pro-Edge
#bash "${SCRIPTS_DIR}/experiments/figure_7/RUN_xb_pro_inst_node.sh"
#bash "${SCRIPTS_DIR}/experiments/figure_7/RUN_xb_pro_inst_edge.sh"
#bash "${SCRIPTS_DIR}/experiments/figure_7/plot_figure_7.sh"
#
## 10. Table-6: CPU: LLC Miss Rate, LLC Frequency. GPU: Effective Memory Bandwidth
#bash "${SCRIPTS_DIR}/experiments/table_6/RUN_xb_pro_mem.sh"
#bash "${SCRIPTS_DIR}/experiments/table_6/RUN_bfs_ligra_mem.sh"
#bash "${SCRIPTS_DIR}/experiments/table_6/RUN_xb_pp_mem.sh"
#bash "${SCRIPTS_DIR}/experiments/table_6/RUN_bfs_gunrock_mem.sh"
#bash "${SCRIPTS_DIR}/experiments/table_6/generate_table_6.sh"