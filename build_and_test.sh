cmake --build build --target pbvfx_tests --parallel
ctest --test-dir build --output-on-failure