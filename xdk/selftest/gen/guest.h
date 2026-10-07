#ifndef m5_guestHeader
#define m5_guestHeader

#ifdef __cplusplus
extern "C" {
#endif

#include "w2c2_base.h"

#if W2C2_RUNTIME_CHECKS
extern const wasmFuncType m5_guestFuncType0;
extern const wasmFuncType m5_guestFuncType1;
extern const wasmFuncType m5_guestFuncType2;
extern const wasmFuncType m5_guestFuncType3;
extern const wasmFuncType m5_guestFuncType4;
extern const wasmFuncType m5_guestFuncType5;
extern const wasmFuncType m5_guestFuncType6;
extern const wasmFuncType m5_guestFuncType7;
extern const wasmFuncType m5_guestFuncType8;
extern const wasmFuncType m5_guestFuncType9;
extern const wasmFuncType m5_guestFuncType10;
extern const wasmFuncType m5_guestFuncType11;
extern const wasmFuncType m5_guestFuncType12;
extern const wasmFuncType m5_guestFuncType13;
extern const wasmFuncType m5_guestFuncType14;
extern const wasmFuncType m5_guestFuncType15;
extern const wasmFuncType m5_guestFuncType16;
extern const wasmFuncType m5_guestFuncType17;
#endif

extern const U8 m5_guestData0[];
extern const U8 m5_guestData1[];
extern const U8 m5_guestData2[];
extern const U8 m5_guestData3[];
extern const U8 m5_guestData4[];
extern const U8 m5_guestData5[];
extern const U8 m5_guestData6[];
extern const U8 m5_guestData7[];
extern const U8 m5_guestData8[];
extern const U8 m5_guestData9[];
extern const U8 m5_guestData10[];
extern const U8 m5_guestData11[];
extern const U8 m5_guestData12[];
extern const U8 m5_guestData13[];
extern const U8 m5_guestData14[];
extern const U8 m5_guestData15[];
extern const U8 m5_guestData16[];
extern const U8 m5_guestData17[];
extern const U8 m5_guestData18[];
extern const U8 m5_guestData19[];
extern const U8 m5_guestData20[];
extern const U8 m5_guestData21[];
extern const U8 m5_guestData22[];
extern const U8 m5_guestData23[];
extern const U8 m5_guestData24[];

typedef struct m5_guestInstance {
wasmModuleInstance common;
wasmMemory* m0;
wasmTable t0;
U32 g0;
#if W2C2_RUNTIME_CHECKS
U32 dataSegmentSizes[25];
#endif
} m5_guestInstance;

U64 i33_wasiX3AclocksX2FmonotonicX2DclockX400X2E2X2E03_now(wasmModuleInstance*);

U32 i33_wasiX3AclocksX2FmonotonicX2DclockX400X2E2X2E018_subscribeX2Dduration(wasmModuleInstance*,U64);

U32 i18_wasiX3AioX2FpollX400X2E2X2E022_X5BmethodX5DpollableX2Eready(wasmModuleInstance*,U32);

void i18_wasiX3AioX2FpollX400X2E2X2E04_poll(wasmModuleInstance*,U32,U32,U32);

void i18_wasiX3AioX2FpollX400X2E2X2E023_X5BresourceX2DdropX5Dpollable(wasmModuleInstance*,U32);

U32 i10_spireX5Ftest7_missing(wasmModuleInstance*,U32);

U32 i22_wasiX5FsnapshotX5Fpreview18_argsX5Fget(wasmModuleInstance*,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview114_argsX5FsizesX5Fget(wasmModuleInstance*,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview111_environX5Fget(wasmModuleInstance*,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview117_environX5FsizesX5Fget(wasmModuleInstance*,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview114_clockX5FtimeX5Fget(wasmModuleInstance*,U32,U64,U32);

U32 i22_wasiX5FsnapshotX5Fpreview18_fdX5Fclose(wasmModuleInstance*,U32);

U32 i22_wasiX5FsnapshotX5Fpreview113_fdX5FfdstatX5Fget(wasmModuleInstance*,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview119_fdX5FfdstatX5FsetX5Fflags(wasmModuleInstance*,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview114_fdX5FprestatX5Fget(wasmModuleInstance*,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview119_fdX5FprestatX5FdirX5Fname(wasmModuleInstance*,U32,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview17_fdX5Fread(wasmModuleInstance*,U32,U32,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview110_fdX5Freaddir(wasmModuleInstance*,U32,U32,U32,U64,U32);

U32 i22_wasiX5FsnapshotX5Fpreview17_fdX5Fseek(wasmModuleInstance*,U32,U64,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview18_fdX5Fwrite(wasmModuleInstance*,U32,U32,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview121_pathX5FcreateX5Fdirectory(wasmModuleInstance*,U32,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview117_pathX5FfilestatX5Fget(wasmModuleInstance*,U32,U32,U32,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview19_pathX5Fopen(wasmModuleInstance*,U32,U32,U32,U32,U32,U64,U64,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview121_pathX5FremoveX5Fdirectory(wasmModuleInstance*,U32,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview111_pathX5Frename(wasmModuleInstance*,U32,U32,U32,U32,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview116_pathX5FunlinkX5Ffile(wasmModuleInstance*,U32,U32,U32);

U32 i22_wasiX5FsnapshotX5Fpreview111_pollX5Foneoff(wasmModuleInstance*,U32,U32,U32,U32);

void i22_wasiX5FsnapshotX5Fpreview19_procX5Fexit(wasmModuleInstance*,U32);

void f28(wasmModuleInstance*);

U32 f29(wasmModuleInstance*,U32);

void f30(wasmModuleInstance*,U32);

U32 f31(wasmModuleInstance*,U32,U32);

U32 f32(wasmModuleInstance*,U32,U32);

void f33(wasmModuleInstance*,U32,U32);

void f34(wasmModuleInstance*,U32);

void f35(wasmModuleInstance*,U32);

U32 f36(wasmModuleInstance*,U32);

U32 f37(wasmModuleInstance*,U32,U64,U32);

U32 f38(wasmModuleInstance*,U32,U32);

U32 f39(wasmModuleInstance*,U32,U32,U32,U32);

U32 f40(wasmModuleInstance*,U32,U32,U32,U64,U32);

U32 f41(wasmModuleInstance*,U32,U32,U32,U32);

void f42(wasmModuleInstance*,U32);

U32 f43(wasmModuleInstance*,U32,U32,U32);

U32 f44(wasmModuleInstance*,U32,U32,U32,U32);

U32 f45(wasmModuleInstance*,U32);

void f46(wasmModuleInstance*);

U32 f47(wasmModuleInstance*,U32,U32,U32,U32);

U32 f48(wasmModuleInstance*,U32);

void f49(wasmModuleInstance*,U32);

U32 f50(wasmModuleInstance*,U32);

void f51(wasmModuleInstance*);

U32 f52(wasmModuleInstance*,U32);

U32 f53(wasmModuleInstance*,U32,U32);

U32 f54(wasmModuleInstance*,U32,U32,U32);

U32 f55(wasmModuleInstance*,U32);

U64 f56(wasmModuleInstance*,U32,U64,U32);

U32 f57(wasmModuleInstance*,U32,U32,U32);

U32 f58(wasmModuleInstance*,U32,U32,U32);

U32 f59(wasmModuleInstance*,U32,U32,U32);

U32 f60(wasmModuleInstance*,U32);

U32 f61(wasmModuleInstance*,U32,U32);

U32 f62(wasmModuleInstance*,U32);

void f63(wasmModuleInstance*,U32,U32);

U32 f64(wasmModuleInstance*,U32,U32,U32);

void f65(wasmModuleInstance*,U32,U32,U32);

U32 f66(wasmModuleInstance*,U32,U32,U32,U32);

void f67(wasmModuleInstance*,U32,U32);

U32 f68(wasmModuleInstance*,U32,U32);

void f69(wasmModuleInstance*,U32);

U32 f70(wasmModuleInstance*,U32,U32,U32);

U32 f71(wasmModuleInstance*,U32,U32);

F64 f72(wasmModuleInstance*,F64,U32);

U32 f73(wasmModuleInstance*,U32,U32,U32,U32,U32);

void f74(wasmModuleInstance*,U32,U32,U32);

void f75(wasmModuleInstance*,U32,U32,U32);

U32 f76(wasmModuleInstance*,U32,U32,U32);

U32 f77(wasmModuleInstance*,U32,U32,U32);

U32 f78(wasmModuleInstance*,U32,U32,U32);

U32 f79(wasmModuleInstance*,U32,U32,U32);

U32 f80(wasmModuleInstance*,U32,U32);

U32 f81(wasmModuleInstance*,U32,U32);

U32 f82(wasmModuleInstance*,U32);

wasmMemory*m5_guestExport6_memory(m5_guestInstance* i);

void m5_guestExport6_X5Fstart(m5_guestInstance*i);

void m5_guestInstantiate(m5_guestInstance* instance, wasmImportResolver resolve);

void m5_guestFreeInstance(m5_guestInstance* instance);

#ifdef __cplusplus
}
#endif

#endif /* m5_guestHeader */

