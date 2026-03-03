#!/bin/bash
#SBATCH -N 1
#SBATCH --exclusive
#SBATCH --cpus-per-task=16
#SBATCH --mem=32G
#SBATCH --time=02:00:00
#SBATCH --job-name=dt-setup
#SBATCH --output=logs/setup_%j.out
#SBATCH --error=logs/setup_%j.err

set -euo pipefail

echo "=== DTProvenance Setup on $(hostname) ==="
echo "Date: $(date)"

# Activate spack environment
spack env activate mchips

# Build clio-core with DTProvenance
cd clio-core
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja \
  -DWRP_CORE_ENABLE_CTE=ON \
  -DWRP_CORE_ENABLE_CEE=ON \
  -DDT_PROVENANCE_ENABLE_TESTS=ON

ninja -j$(nproc) dt_demo_server ctx_writer

# Run protocol library unit tests
echo "=== Running protocol library tests ==="
ctest --test-dir . -R "test_provider_detection|test_session_extraction|test_anthropic_parser|test_openai_parser|test_stream_reassembly|test_interaction_record|test_conversation_threading" --output-on-failure

# Run interception ChiMod unit tests (parsing + forwarding)
echo "=== Running interception ChiMod tests ==="
ctest --test-dir . -R "test_anthropic_parsing|test_anthropic_forwarding|test_openai_parsing|test_openai_forwarding|test_ollama_parsing|test_ollama_forwarding" --output-on-failure

echo "=== Setup complete ==="
