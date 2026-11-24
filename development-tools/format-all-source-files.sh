#!/bin/sh
# Applies the repository's layout to every C source and header:
#
#   1. clang-format aligns declarations, assignments and parameters,
#   2. apply-paragraph-spacing.py separates the steps of each function,
#   3. clang-format runs again so alignment follows the new paragraphs,
#   4. collapse-empty-parameter-lists.py writes "(void)" on one line.
set -eu

repository_root=$(dirname "$0")/..
cd "$repository_root"

source_files=$(find libraries programs development-tools -name '*.c' -o -name '*.h')

clang-format -i $source_files
python3 development-tools/apply-paragraph-spacing.py $source_files
clang-format -i $source_files
python3 development-tools/collapse-empty-parameter-lists.py $source_files
