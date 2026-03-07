#!/bin/bash
#SBATCH -N 1
#SBATCH -J dt_inttest
#SBATCH -o /mnt/common/%u/clio-core/context-exploration-engine/agent-interceptor/deploy/integration_test.out
#SBATCH -e /mnt/common/%u/clio-core/context-exploration-engine/agent-interceptor/deploy/integration_test.err

echo "=== Running on $(hostname), job $SLURM_JOB_ID ==="
REPO_ROOT="/mnt/common/$(whoami)/clio-core"
cd "$REPO_ROOT/context-exploration-engine/agent-interceptor"

# Kill any leftover servers from previous runs
pkill -f dt_demo_server 2>/dev/null || true
sleep 1

unset CLAUDECODE
SPACK_PATH="${SPACK_PATH:-$HOME/spack}"
source "$SPACK_PATH/share/spack/setup-env.sh"
spack env activate mchips

timeout 300 deploy/.venv/bin/python deploy/integration_test.py \
  --build-dir "$REPO_ROOT/build" \
  --proxy-port 9090
echo "Exit code: $?"
