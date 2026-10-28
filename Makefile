.PHONY: configure build test clean sanitize

configure:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

build: configure
	cmake --build build --parallel

test: build
	ctest --test-dir build --output-on-failure

sanitize:
	cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DREVASM_ENABLE_SANITIZERS=ON
	cmake --build build-sanitize --parallel
	ctest --test-dir build-sanitize --output-on-failure

clean:
	rm -rf build build-sanitize
