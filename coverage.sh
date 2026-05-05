test_binary=./tests/can_to_pwm_firmware_tests

cd build
cmake .. -DCOVERAGE_ENABLED=ON
ninja
$test_binary
llvm-profdata merge --output="default.profdata" "default.profraw"
llvm-cov show $test_binary -instr-profile="default.profdata" --format=html --output-dir=coverage_report --ignore-filename-regex="build.*|tests\.cpp"
cd ..
