#!/bin/bash
#SBATCH -N 1
#SBATCH -J dt_2q
#SBATCH -o /mnt/common/%u/clio-core/context-exploration-engine/agent-interceptor/deploy/test_two_queries.out
#SBATCH -e /mnt/common/%u/clio-core/context-exploration-engine/agent-interceptor/deploy/test_two_queries.err

REPO_ROOT="/mnt/common/$(whoami)/clio-core"
cd "$REPO_ROOT/context-exploration-engine/agent-interceptor"
unset CLAUDECODE
SPACK_PATH="${SPACK_PATH:-$HOME/spack}"
source "$SPACK_PATH/share/spack/setup-env.sh"
spack env activate mchips

timeout 180 deploy/.venv/bin/python deploy/test_two_queries.py
echo "Exit code: $?"
