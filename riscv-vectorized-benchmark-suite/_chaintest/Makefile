#makefile

start:
	rm bin/*.*

vector:
	echo "vector"
	${RISCV}/bin/clang -march=rv64gcv -DUSE_RISCV_VECTOR -O1 -fno-vectorize -funroll-loops -I../../include/ -L../../util/m5/build/riscv/out  --static -o bin/chaintest src/main.c -lm5
	${RISCV}/bin/llvm-objdump  --mattr=+m,+f,+d,+a,+c,+v -S  bin/chaintest > bin/chaintest.dump

clean:
	rm bin/*