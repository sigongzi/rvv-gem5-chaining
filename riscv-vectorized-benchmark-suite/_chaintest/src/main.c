#include <stdint.h>
#include <riscv_vector.h>
#include <riscv_vector_v0p10.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include "gem5/m5ops.h"

#define ARRAY_SIZE 16

// 随机生成 double 类型数组
void generate_random_data(double *arr, size_t size) {
    
    for (size_t i = 0; i < size; i++) {
        arr[i] = (rand() % 100) * 100;
    }
}


    // 这一段编译器会换序，把v4换到v3前面
    // // 从 a1 和 a2 读取数据
    // vfloat64m1_t v1 = vle64_v_f64m1(a1, gvl);
    // vfloat64m1_t v2 = vle64_v_f64m1(a2, gvl);

    // // VMUL.v V3,V1,V2
    // vfloat64m1_t v3 = vfmul_vv_f64m1(v1, v2, gvl);

    // // VLW.v V4,a3
    // vfloat64m1_t v4 = vle64_v_f64m1(a3, gvl);

    
    // // VADD.v V5,V3,V4
    // vfloat64m1_t v5 = vfadd_vv_f64m1(v3, v4, gvl);

    // 将结果存储到 result
    //__riscv_vse64_v_f64m1(result, v5, ARRAY_SIZE);
void vector_f64_operation(double *a1, double *a2, double *a3, double *result) {
    m5_reset_stats(0, 0);    // 重置性能计数器




    asm volatile (
        "vsetvli x0, %[array_size], e64, m1, ta, ma\n"
        // 从 a1 和 a2 读取数据
        "vle64.v v1, (%[a1])\n"
        "vle64.v v2, (%[a2])\n"
        // VMUL.v V3,V1,V2
        "vfmul.vv v3, v1, v2\n"
        // VLW.v V4,a3
        // "vle64.v v4, (%[a3])\n"
        // // VADD.v V5,V3,V4
        // "vfadd.vv v5, v3, v4\n"
        // 将结果存储到 result
        "vse64.v v3, (%[result])\n"
        :
        : [a1] "r" (a1), [a2] "r" (a2), [a3] "r" (a3), [result] "r" (result), [array_size] "r" (ARRAY_SIZE)
        : "v1", "v2", "v3", "v4", "v5"
    );

    // //这一段编译器会换序，把v4换到v3前面
    // size_t gvl = __riscv_vsetvl_e64m1(ARRAY_SIZE);
    // // 从 a1 和 a2 读取数据
    // vfloat64m1_t v1 = vle64_v_f64m1(a1, gvl);
    // vfloat64m1_t v2 = vle64_v_f64m1(a2, gvl);

    // // VMUL.v V3,V1,V2
    // vfloat64m1_t v3 = vfmul_vv_f64m1(v1, v2, gvl);

    // // VLW.v V4,a3
    // vfloat64m1_t v4 = vle64_v_f64m1(a3, gvl);

    
    // // VADD.v V5,V3,V4
    // vfloat64m1_t v5 = vfadd_vv_f64m1(v3, v4, gvl);

    // //将结果存储到 result
    // __riscv_vse64_v_f64m1(result, v5, ARRAY_SIZE);
    m5_dump_stats(0, 0);     // 保存当前统计信息
}

int main() {
    srand(time(NULL));
    double a1[ARRAY_SIZE] __attribute__((aligned(8)));
    double a2[ARRAY_SIZE] __attribute__((aligned(8)));
    double a3[ARRAY_SIZE] __attribute__((aligned(8)));
    double result[ARRAY_SIZE] __attribute__((aligned(8)));

    // 生成随机数据
    generate_random_data(a1, ARRAY_SIZE);
    generate_random_data(a2, ARRAY_SIZE);
    generate_random_data(a3, ARRAY_SIZE);

    // 输出随机数据
    for (int i = 0 ; i < ARRAY_SIZE ; ++i) {
        printf("a1[%d] = %lf, a2[%d] = %lf, a3[%d] = %lf\n", i, a1[i],i, a2[i],i, a3[i]);
    }

    result[0] = 1.0;
    // 执行向量操作
    vector_f64_operation(a1, a2, a3, result);

    
    // 打印结果
    for (int i = 0; i < ARRAY_SIZE; i++) {
        printf("Result[%d]: %lf\n", i, result[i]);
    }

    return 0;
}