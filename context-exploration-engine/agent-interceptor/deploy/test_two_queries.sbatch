#!/bin/bash
#SBATCH -N 1
#SBATCH -J dt_2q
#SBATCH -o /mnt/common/jcernudagarcia/clio-core/context-exploration-engine/agent-interceptor/deploy/test_two_queries.out
#SBATCH -e /mnt/common/jcernudagarcia/clio-core/context-exploration-engine/agent-interceptor/deploy/test_two_queries.err

cd /mnt/common/jcernudagarcia/clio-core/context-exploration-engine/agent-interceptor
unset CLAUDECODE
source /mnt/common/jcernudagarcia/spack/share/spack/setup-env.sh
spack env activate mchips

timeout 180 deploy/.venv/bin/python deploy/test_two_queries.py
echo "Exit code: $?"
