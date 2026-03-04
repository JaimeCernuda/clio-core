#!/bin/bash
#SBATCH -N 1
#SBATCH -J dt_inttest
#SBATCH -o /mnt/common/jcernudagarcia/clio-core/context-exploration-engine/agent-interceptor/deploy/integration_test.out
#SBATCH -e /mnt/common/jcernudagarcia/clio-core/context-exploration-engine/agent-interceptor/deploy/integration_test.err

echo "=== Running on $(hostname), job $SLURM_JOB_ID ==="
cd /mnt/common/jcernudagarcia/clio-core/context-exploration-engine/agent-interceptor

# Kill any leftover servers from previous runs
pkill -f dt_demo_server 2>/dev/null || true
sleep 1

unset CLAUDECODE
source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

timeout 300 deploy/.venv/bin/python deploy/integration_test.py \
  --build-dir /mnt/common/jcernudagarcia/clio-core/build \
  --proxy-port 9090
echo "Exit code: $?"
