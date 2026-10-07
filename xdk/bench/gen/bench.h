#ifndef m5_benchHeader
#define m5_benchHeader

#ifdef __cplusplus
extern "C" {
#endif

#include "w2c2_base.h"

#if W2C2_RUNTIME_CHECKS
extern const wasmFuncType m5_benchFuncType0;
extern const wasmFuncType m5_benchFuncType1;
extern const wasmFuncType m5_benchFuncType2;
extern const wasmFuncType m5_benchFuncType3;
extern const wasmFuncType m5_benchFuncType4;
extern const wasmFuncType m5_benchFuncType5;
extern const wasmFuncType m5_benchFuncType6;
extern const wasmFuncType m5_benchFuncType7;
extern const wasmFuncType m5_benchFuncType8;
extern const wasmFuncType m5_benchFuncType9;
extern const wasmFuncType m5_benchFuncType10;
extern const wasmFuncType m5_benchFuncType11;
extern const wasmFuncType m5_benchFuncType12;
extern const wasmFuncType m5_benchFuncType13;
extern const wasmFuncType m5_benchFuncType14;
extern const wasmFuncType m5_benchFuncType15;
#endif

extern const U8 m5_benchData0[];
extern const U8 m5_benchData1[];
extern const U8 m5_benchData2[];
extern const U8 m5_benchData3[];
extern const U8 m5_benchData4[];
extern const U8 m5_benchData5[];
extern const U8 m5_benchData6[];
extern const U8 m5_benchData7[];
extern const U8 m5_benchData8[];
extern const U8 m5_benchData9[];
extern const U8 m5_benchData10[];
extern const U8 m5_benchData11[];
extern const U8 m5_benchData12[];
extern const U8 m5_benchData13[];
extern const U8 m5_benchData14[];
extern const U8 m5_benchData15[];
extern const U8 m5_benchData16[];
extern const U8 m5_benchData17[];
extern const U8 m5_benchData18[];
extern const U8 m5_benchData19[];
extern const U8 m5_benchData20[];
extern const U8 m5_benchData21[];
extern const U8 m5_benchData22[];
extern const U8 m5_benchData23[];

typedef struct m5_benchInstance {
wasmModuleInstance common;
wasmMemory* m0;
wasmTable t0;
U32 g0;
#if W2C2_RUNTIME_CHECKS
U32 dataSegmentSizes[24];
#endif
} m5_benchInstance;

U32 i22_wasiX5FsnapshotX5Fpreview114_clockX5FtimeX5Fget(wasmModuleInstance*,U32,U64,U32);

U32 i22_wasiX5FsnapshotX5Fpreview18_fdX5Fclose(wasmModuleInstance*,U32);

U32 i22_wasiX5FsnapshotX5Fpreview113_fdX5FfdstatX5Fget(wasmModuleInstance*,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview17_fdX5Fseek(wasmModuleInstance*,U32,U64,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview18_fdX5Fwrite(wasmModuleInstance*,U32,U32,U32,U32);

void f5(wasmModuleInstance*);

U32 f6(wasmModuleInstance*,U32,U32);

U32 f7(wasmModuleInstance*,U32,U32);

U32 f8(wasmModuleInstance*,U32,U32);

U32 f9(wasmModuleInstance*,U32,U32);

U32 f10(wasmModuleInstance*,U32,U32);

void f11(wasmModuleInstance*,U32);

F64 f12(wasmModuleInstance*,F64,F64,U32);

F64 f13(wasmModuleInstance*,F64,U32);

F64 f14(wasmModuleInstance*,F64,F64);

void f15(wasmModuleInstance*,U32,U32);

U32 f16(wasmModuleInstance*,U32);

void f17(wasmModuleInstance*,U32,U32,U32);

U32 f18(wasmModuleInstance*,U32,U32);

void f19(wasmModuleInstance*,U32);

U32 f20(wasmModuleInstance*,U32);

U32 f21(wasmModuleInstance*,U32,U32,U32);

U32 f22(wasmModuleInstance*,U32,U32,U32);

U32 f23(wasmModuleInstance*,U32,U32,U32);

U64 f24(wasmModuleInstance*,U32,U64,U32);

U32 f25(wasmModuleInstance*,U32,U32);

F64 f26(wasmModuleInstance*,F64,U32);

void f27(wasmModuleInstance*,U32,U32,U32);

U32 f28(wasmModuleInstance*,U32,U32,U32,U32,U32);

void f29(wasmModuleInstance*,U32,U32,U32);

void f30(wasmModuleInstance*,U32,U32,U32);

U32 f31(wasmModuleInstance*,U32,U32,U32);

void f32(wasmModuleInstance*,U32,U32,U32);

void f33(wasmModuleInstance*,U32,U32,U32,U32,U32);

void f34(wasmModuleInstance*);

U32 f35(wasmModuleInstance*,U32,U32,U32);

U32 f36(wasmModuleInstance*,U32,U32,U32);

U32 f37(wasmModuleInstance*,U32,U32,U32);

wasmMemory*m5_benchExport6_memory(m5_benchInstance* i);

void m5_benchExport6_X5Fstart(m5_benchInstance*i);

void m5_benchInstantiate(m5_benchInstance* instance, wasmImportResolver resolve);

void m5_benchFreeInstance(m5_benchInstance* instance);

#ifdef __cplusplus
}
#endif

#endif /* m5_benchHeader */

