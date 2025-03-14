// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/base/platform/elapsed-timer.h"

#include "src/wasm/wasm-macro-gen.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/test-signatures.h"
#include "test/cctest/wasm/wasm-run-utils.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

// for even shorter tests.
#define B2(a, b) kExprBlock, a, b, kExprEnd
#define B1(a) kExprBlock, a, kExprEnd
#define RET(x) x, kExprReturn, 1
#define RET_I8(x) kExprI8Const, x, kExprReturn, 1

TEST(Run_WasmInt8Const) {
  WasmRunner<int32_t> r;
  const byte kExpectedValue = 121;
  // return(kExpectedValue)
  BUILD(r, WASM_I8(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}


TEST(Run_WasmInt8Const_fallthru1) {
  WasmRunner<int32_t> r;
  const byte kExpectedValue = 122;
  // kExpectedValue
  BUILD(r, WASM_I8(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}


TEST(Run_WasmInt8Const_fallthru2) {
  WasmRunner<int32_t> r;
  const byte kExpectedValue = 123;
  // -99 kExpectedValue
  BUILD(r, WASM_I8(-99), WASM_I8(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}


TEST(Run_WasmInt8Const_all) {
  for (int value = -128; value <= 127; value++) {
    WasmRunner<int32_t> r;
    // return(value)
    BUILD(r, WASM_I8(value));
    int32_t result = r.Call();
    CHECK_EQ(value, result);
  }
}


TEST(Run_WasmInt32Const) {
  WasmRunner<int32_t> r;
  const int32_t kExpectedValue = 0x11223344;
  // return(kExpectedValue)
  BUILD(r, WASM_I32V_5(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}


TEST(Run_WasmInt32Const_many) {
  FOR_INT32_INPUTS(i) {
    WasmRunner<int32_t> r;
    const int32_t kExpectedValue = *i;
    // return(kExpectedValue)
    BUILD(r, WASM_I32V(kExpectedValue));
    CHECK_EQ(kExpectedValue, r.Call());
  }
}


TEST(Run_WasmMemorySize) {
  TestingModule module;
  WasmRunner<int32_t> r(&module);
  module.AddMemory(1024);
  BUILD(r, kExprMemorySize);
  CHECK_EQ(1024, r.Call());
}


TEST(Run_WasmInt32Param0) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // return(local[0])
  BUILD(r, WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}


TEST(Run_WasmInt32Param0_fallthru) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // local[0]
  BUILD(r, WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}


TEST(Run_WasmInt32Param1) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  // local[1]
  BUILD(r, WASM_GET_LOCAL(1));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(-111, *i)); }
}


TEST(Run_WasmInt32Add) {
  WasmRunner<int32_t> r;
  // 11 + 44
  BUILD(r, WASM_I32_ADD(WASM_I8(11), WASM_I8(44)));
  CHECK_EQ(55, r.Call());
}


TEST(Run_WasmInt32Add_P) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // p0 + 13
  BUILD(r, WASM_I32_ADD(WASM_I8(13), WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i + 13, r.Call(*i)); }
}


TEST(Run_WasmInt32Add_P_fallthru) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // p0 + 13
  BUILD(r, WASM_I32_ADD(WASM_I8(13), WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(*i + 13, r.Call(*i)); }
}


TEST(Run_WasmInt32Add_P2) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  //  p0 + p1
  BUILD(r, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      int32_t expected = static_cast<int32_t>(static_cast<uint32_t>(*i) +
                                              static_cast<uint32_t>(*j));
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}


TEST(Run_WasmFloat32Add) {
  WasmRunner<int32_t> r;
  // int(11.5f + 44.5f)
  BUILD(r,
        WASM_I32_SCONVERT_F32(WASM_F32_ADD(WASM_F32(11.5f), WASM_F32(44.5f))));
  CHECK_EQ(56, r.Call());
}


TEST(Run_WasmFloat64Add) {
  WasmRunner<int32_t> r;
  // return int(13.5d + 43.5d)
  BUILD(r, WASM_I32_SCONVERT_F64(WASM_F64_ADD(WASM_F64(13.5), WASM_F64(43.5))));
  CHECK_EQ(57, r.Call());
}


void TestInt32Binop(WasmOpcode opcode, int32_t expected, int32_t a, int32_t b) {
  {
    WasmRunner<int32_t> r;
    // K op K
    BUILD(r, WASM_BINOP(opcode, WASM_I32V(a), WASM_I32V(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
    // a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}

TEST(Run_WasmInt32Binops) {
  TestInt32Binop(kExprI32Add, 88888888, 33333333, 55555555);
  TestInt32Binop(kExprI32Sub, -1111111, 7777777, 8888888);
  TestInt32Binop(kExprI32Mul, 65130756, 88734, 734);
  TestInt32Binop(kExprI32DivS, -66, -4777344, 72384);
  TestInt32Binop(kExprI32DivU, 805306368, 0xF0000000, 5);
  TestInt32Binop(kExprI32RemS, -3, -3003, 1000);
  TestInt32Binop(kExprI32RemU, 4, 4004, 1000);
  TestInt32Binop(kExprI32And, 0xEE, 0xFFEE, 0xFF0000FF);
  TestInt32Binop(kExprI32Ior, 0xF0FF00FF, 0xF0F000EE, 0x000F0011);
  TestInt32Binop(kExprI32Xor, 0xABCDEF01, 0xABCDEFFF, 0xFE);
  TestInt32Binop(kExprI32Shl, 0xA0000000, 0xA, 28);
  TestInt32Binop(kExprI32ShrU, 0x07000010, 0x70000100, 4);
  TestInt32Binop(kExprI32ShrS, 0xFF000000, 0x80000000, 7);
  TestInt32Binop(kExprI32Ror, 0x01000000, 0x80000000, 7);
  TestInt32Binop(kExprI32Ror, 0x01000000, 0x80000000, 39);
  TestInt32Binop(kExprI32Rol, 0x00000040, 0x80000000, 7);
  TestInt32Binop(kExprI32Rol, 0x00000040, 0x80000000, 39);
  TestInt32Binop(kExprI32Eq, 1, -99, -99);
  TestInt32Binop(kExprI32Ne, 0, -97, -97);

  TestInt32Binop(kExprI32LtS, 1, -4, 4);
  TestInt32Binop(kExprI32LeS, 0, -2, -3);
  TestInt32Binop(kExprI32LtU, 1, 0, -6);
  TestInt32Binop(kExprI32LeU, 1, 98978, 0xF0000000);

  TestInt32Binop(kExprI32GtS, 1, 4, -4);
  TestInt32Binop(kExprI32GeS, 0, -3, -2);
  TestInt32Binop(kExprI32GtU, 1, -6, 0);
  TestInt32Binop(kExprI32GeU, 1, 0xF0000000, 98978);
}


void TestInt32Unop(WasmOpcode opcode, int32_t expected, int32_t a) {
  {
    WasmRunner<int32_t> r;
    // return op K
    BUILD(r, WASM_UNOP(opcode, WASM_I32V(a)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Int32());
    // return op a
    BUILD(r, WASM_UNOP(opcode, WASM_GET_LOCAL(0)));
    CHECK_EQ(expected, r.Call(a));
  }
}


TEST(Run_WasmInt32Clz) {
  TestInt32Unop(kExprI32Clz, 0, 0x80001000);
  TestInt32Unop(kExprI32Clz, 1, 0x40000500);
  TestInt32Unop(kExprI32Clz, 2, 0x20000300);
  TestInt32Unop(kExprI32Clz, 3, 0x10000003);
  TestInt32Unop(kExprI32Clz, 4, 0x08050000);
  TestInt32Unop(kExprI32Clz, 5, 0x04006000);
  TestInt32Unop(kExprI32Clz, 6, 0x02000000);
  TestInt32Unop(kExprI32Clz, 7, 0x010000a0);
  TestInt32Unop(kExprI32Clz, 8, 0x00800c00);
  TestInt32Unop(kExprI32Clz, 9, 0x00400000);
  TestInt32Unop(kExprI32Clz, 10, 0x0020000d);
  TestInt32Unop(kExprI32Clz, 11, 0x00100f00);
  TestInt32Unop(kExprI32Clz, 12, 0x00080000);
  TestInt32Unop(kExprI32Clz, 13, 0x00041000);
  TestInt32Unop(kExprI32Clz, 14, 0x00020020);
  TestInt32Unop(kExprI32Clz, 15, 0x00010300);
  TestInt32Unop(kExprI32Clz, 16, 0x00008040);
  TestInt32Unop(kExprI32Clz, 17, 0x00004005);
  TestInt32Unop(kExprI32Clz, 18, 0x00002050);
  TestInt32Unop(kExprI32Clz, 19, 0x00001700);
  TestInt32Unop(kExprI32Clz, 20, 0x00000870);
  TestInt32Unop(kExprI32Clz, 21, 0x00000405);
  TestInt32Unop(kExprI32Clz, 22, 0x00000203);
  TestInt32Unop(kExprI32Clz, 23, 0x00000101);
  TestInt32Unop(kExprI32Clz, 24, 0x00000089);
  TestInt32Unop(kExprI32Clz, 25, 0x00000041);
  TestInt32Unop(kExprI32Clz, 26, 0x00000022);
  TestInt32Unop(kExprI32Clz, 27, 0x00000013);
  TestInt32Unop(kExprI32Clz, 28, 0x00000008);
  TestInt32Unop(kExprI32Clz, 29, 0x00000004);
  TestInt32Unop(kExprI32Clz, 30, 0x00000002);
  TestInt32Unop(kExprI32Clz, 31, 0x00000001);
  TestInt32Unop(kExprI32Clz, 32, 0x00000000);
}


TEST(Run_WasmInt32Ctz) {
  TestInt32Unop(kExprI32Ctz, 32, 0x00000000);
  TestInt32Unop(kExprI32Ctz, 31, 0x80000000);
  TestInt32Unop(kExprI32Ctz, 30, 0x40000000);
  TestInt32Unop(kExprI32Ctz, 29, 0x20000000);
  TestInt32Unop(kExprI32Ctz, 28, 0x10000000);
  TestInt32Unop(kExprI32Ctz, 27, 0xa8000000);
  TestInt32Unop(kExprI32Ctz, 26, 0xf4000000);
  TestInt32Unop(kExprI32Ctz, 25, 0x62000000);
  TestInt32Unop(kExprI32Ctz, 24, 0x91000000);
  TestInt32Unop(kExprI32Ctz, 23, 0xcd800000);
  TestInt32Unop(kExprI32Ctz, 22, 0x09400000);
  TestInt32Unop(kExprI32Ctz, 21, 0xaf200000);
  TestInt32Unop(kExprI32Ctz, 20, 0xac100000);
  TestInt32Unop(kExprI32Ctz, 19, 0xe0b80000);
  TestInt32Unop(kExprI32Ctz, 18, 0x9ce40000);
  TestInt32Unop(kExprI32Ctz, 17, 0xc7920000);
  TestInt32Unop(kExprI32Ctz, 16, 0xb8f10000);
  TestInt32Unop(kExprI32Ctz, 15, 0x3b9f8000);
  TestInt32Unop(kExprI32Ctz, 14, 0xdb4c4000);
  TestInt32Unop(kExprI32Ctz, 13, 0xe9a32000);
  TestInt32Unop(kExprI32Ctz, 12, 0xfca61000);
  TestInt32Unop(kExprI32Ctz, 11, 0x6c8a7800);
  TestInt32Unop(kExprI32Ctz, 10, 0x8ce5a400);
  TestInt32Unop(kExprI32Ctz, 9, 0xcb7d0200);
  TestInt32Unop(kExprI32Ctz, 8, 0xcb4dc100);
  TestInt32Unop(kExprI32Ctz, 7, 0xdfbec580);
  TestInt32Unop(kExprI32Ctz, 6, 0x27a9db40);
  TestInt32Unop(kExprI32Ctz, 5, 0xde3bcb20);
  TestInt32Unop(kExprI32Ctz, 4, 0xd7e8a610);
  TestInt32Unop(kExprI32Ctz, 3, 0x9afdbc88);
  TestInt32Unop(kExprI32Ctz, 2, 0x9afdbc84);
  TestInt32Unop(kExprI32Ctz, 1, 0x9afdbc82);
  TestInt32Unop(kExprI32Ctz, 0, 0x9afdbc81);
}


TEST(Run_WasmInt32Popcnt) {
  TestInt32Unop(kExprI32Popcnt, 32, 0xffffffff);
  TestInt32Unop(kExprI32Popcnt, 0, 0x00000000);
  TestInt32Unop(kExprI32Popcnt, 1, 0x00008000);
  TestInt32Unop(kExprI32Popcnt, 13, 0x12345678);
  TestInt32Unop(kExprI32Popcnt, 19, 0xfedcba09);
}

TEST(Run_WasmI32Eqz) {
  TestInt32Unop(kExprI32Eqz, 0, 1);
  TestInt32Unop(kExprI32Eqz, 0, -1);
  TestInt32Unop(kExprI32Eqz, 0, -827343);
  TestInt32Unop(kExprI32Eqz, 0, 8888888);
  TestInt32Unop(kExprI32Eqz, 1, 0);
}

TEST(Run_WasmI32Shl) {
  WasmRunner<uint32_t> r(MachineType::Uint32(), MachineType::Uint32());
  BUILD(r, WASM_I32_SHL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_UINT32_INPUTS(i) {
    FOR_UINT32_INPUTS(j) {
      uint32_t expected = (*i) << (*j & 0x1f);
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

TEST(Run_WasmI32Shr) {
  WasmRunner<uint32_t> r(MachineType::Uint32(), MachineType::Uint32());
  BUILD(r, WASM_I32_SHR(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_UINT32_INPUTS(i) {
    FOR_UINT32_INPUTS(j) {
      uint32_t expected = (*i) >> (*j & 0x1f);
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

TEST(Run_WasmI32Sar) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_SAR(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      int32_t expected = (*i) >> (*j & 0x1f);
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

TEST(Run_WASM_Int32DivS_trap) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(kMin, -1));
  CHECK_TRAP(r.Call(kMin, 0));
}


TEST(Run_WASM_Int32RemS_trap) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_REMS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(33, r.Call(133, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(kMin, 0));
}


TEST(Run_WASM_Int32DivU_trap) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_DIVU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(kMin, 0));
}


TEST(Run_WASM_Int32RemU_trap) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_REMU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  CHECK_EQ(17, r.Call(217, 100));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(kMin, 0));
  CHECK_EQ(kMin, r.Call(kMin, -1));
}

TEST(Run_WASM_Int32DivS_asmjs) {
  TestingModule module;
  module.origin = kAsmJsOrigin;
  WasmRunner<int32_t> r(&module, MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(kMin, r.Call(kMin, -1));
  CHECK_EQ(0, r.Call(kMin, 0));
}

TEST(Run_WASM_Int32RemS_asmjs) {
  TestingModule module;
  module.origin = kAsmJsOrigin;
  WasmRunner<int32_t> r(&module, MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_REMS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(33, r.Call(133, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(0, r.Call(kMin, 0));
}

TEST(Run_WASM_Int32DivU_asmjs) {
  TestingModule module;
  module.origin = kAsmJsOrigin;
  WasmRunner<int32_t> r(&module, MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_DIVU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(0, r.Call(kMin, 0));
}

TEST(Run_WASM_Int32RemU_asmjs) {
  TestingModule module;
  module.origin = kAsmJsOrigin;
  WasmRunner<int32_t> r(&module, MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_REMU(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(17, r.Call(217, 100));
  CHECK_EQ(0, r.Call(100, 0));
  CHECK_EQ(0, r.Call(-1001, 0));
  CHECK_EQ(0, r.Call(kMin, 0));
  CHECK_EQ(kMin, r.Call(kMin, -1));
}


TEST(Run_WASM_Int32DivS_byzero_const) {
  for (int8_t denom = -2; denom < 8; denom++) {
    WasmRunner<int32_t> r(MachineType::Int32());
    BUILD(r, WASM_I32_DIVS(WASM_GET_LOCAL(0), WASM_I8(denom)));
    for (int32_t val = -7; val < 8; val++) {
      if (denom == 0) {
        CHECK_TRAP(r.Call(val));
      } else {
        CHECK_EQ(val / denom, r.Call(val));
      }
    }
  }
}


TEST(Run_WASM_Int32DivU_byzero_const) {
  for (uint32_t denom = 0xfffffffe; denom < 8; denom++) {
    WasmRunner<uint32_t> r(MachineType::Uint32());
    BUILD(r, WASM_I32_DIVU(WASM_GET_LOCAL(0), WASM_I32V_1(denom)));

    for (uint32_t val = 0xfffffff0; val < 8; val++) {
      if (denom == 0) {
        CHECK_TRAP(r.Call(val));
      } else {
        CHECK_EQ(val / denom, r.Call(val));
      }
    }
  }
}


TEST(Run_WASM_Int32DivS_trap_effect) {
  TestingModule module;
  module.AddMemoryElems<int32_t>(8);
  WasmRunner<int32_t> r(&module, MachineType::Int32(), MachineType::Int32());

  BUILD(r,
        WASM_IF_ELSE(WASM_GET_LOCAL(0),
                     WASM_I32_DIVS(WASM_STORE_MEM(MachineType::Int8(),
                                                  WASM_ZERO, WASM_GET_LOCAL(0)),
                                   WASM_GET_LOCAL(1)),
                     WASM_I32_DIVS(WASM_STORE_MEM(MachineType::Int8(),
                                                  WASM_ZERO, WASM_GET_LOCAL(0)),
                                   WASM_GET_LOCAL(1))));
  CHECK_EQ(0, r.Call(0, 100));
  CHECK_TRAP(r.Call(8, 0));
  CHECK_TRAP(r.Call(4, 0));
  CHECK_TRAP(r.Call(0, 0));
}

void TestFloat32Binop(WasmOpcode opcode, int32_t expected, float a, float b) {
  {
    WasmRunner<int32_t> r;
    // return K op K
    BUILD(r, WASM_BINOP(opcode, WASM_F32(a), WASM_F32(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float32(), MachineType::Float32());
    // return a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}


void TestFloat32BinopWithConvert(WasmOpcode opcode, int32_t expected, float a,
                                 float b) {
  {
    WasmRunner<int32_t> r;
    // return int(K op K)
    BUILD(r,
          WASM_I32_SCONVERT_F32(WASM_BINOP(opcode, WASM_F32(a), WASM_F32(b))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float32(), MachineType::Float32());
    // return int(a op b)
    BUILD(r, WASM_I32_SCONVERT_F32(
                 WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))));
    CHECK_EQ(expected, r.Call(a, b));
  }
}


void TestFloat32UnopWithConvert(WasmOpcode opcode, int32_t expected, float a) {
  {
    WasmRunner<int32_t> r;
    // return int(op(K))
    BUILD(r, WASM_I32_SCONVERT_F32(WASM_UNOP(opcode, WASM_F32(a))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float32());
    // return int(op(a))
    BUILD(r, WASM_I32_SCONVERT_F32(WASM_UNOP(opcode, WASM_GET_LOCAL(0))));
    CHECK_EQ(expected, r.Call(a));
  }
}


void TestFloat64Binop(WasmOpcode opcode, int32_t expected, double a, double b) {
  {
    WasmRunner<int32_t> r;
    // return K op K
    BUILD(r, WASM_BINOP(opcode, WASM_F64(a), WASM_F64(b)));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float64(), MachineType::Float64());
    // return a op b
    BUILD(r, WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
    CHECK_EQ(expected, r.Call(a, b));
  }
}


void TestFloat64BinopWithConvert(WasmOpcode opcode, int32_t expected, double a,
                                 double b) {
  {
    WasmRunner<int32_t> r;
    // return int(K op K)
    BUILD(r,
          WASM_I32_SCONVERT_F64(WASM_BINOP(opcode, WASM_F64(a), WASM_F64(b))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float64(), MachineType::Float64());
    BUILD(r, WASM_I32_SCONVERT_F64(
                 WASM_BINOP(opcode, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))));
    CHECK_EQ(expected, r.Call(a, b));
  }
}


void TestFloat64UnopWithConvert(WasmOpcode opcode, int32_t expected, double a) {
  {
    WasmRunner<int32_t> r;
    // return int(op(K))
    BUILD(r, WASM_I32_SCONVERT_F64(WASM_UNOP(opcode, WASM_F64(a))));
    CHECK_EQ(expected, r.Call());
  }
  {
    WasmRunner<int32_t> r(MachineType::Float64());
    // return int(op(a))
    BUILD(r, WASM_I32_SCONVERT_F64(WASM_UNOP(opcode, WASM_GET_LOCAL(0))));
    CHECK_EQ(expected, r.Call(a));
  }
}

TEST(Run_WasmFloat32Binops) {
  TestFloat32Binop(kExprF32Eq, 1, 8.125f, 8.125f);
  TestFloat32Binop(kExprF32Ne, 1, 8.125f, 8.127f);
  TestFloat32Binop(kExprF32Lt, 1, -9.5f, -9.0f);
  TestFloat32Binop(kExprF32Le, 1, -1111.0f, -1111.0f);
  TestFloat32Binop(kExprF32Gt, 1, -9.0f, -9.5f);
  TestFloat32Binop(kExprF32Ge, 1, -1111.0f, -1111.0f);

  TestFloat32BinopWithConvert(kExprF32Add, 10, 3.5f, 6.5f);
  TestFloat32BinopWithConvert(kExprF32Sub, 2, 44.5f, 42.5f);
  TestFloat32BinopWithConvert(kExprF32Mul, -66, -132.1f, 0.5f);
  TestFloat32BinopWithConvert(kExprF32Div, 11, 22.1f, 2.0f);
}

TEST(Run_WasmFloat32Unops) {
  TestFloat32UnopWithConvert(kExprF32Abs, 8, 8.125f);
  TestFloat32UnopWithConvert(kExprF32Abs, 9, -9.125f);
  TestFloat32UnopWithConvert(kExprF32Neg, -213, 213.125f);
  TestFloat32UnopWithConvert(kExprF32Sqrt, 12, 144.4f);
}

TEST(Run_WasmFloat64Binops) {
  TestFloat64Binop(kExprF64Eq, 1, 16.25, 16.25);
  TestFloat64Binop(kExprF64Ne, 1, 16.25, 16.15);
  TestFloat64Binop(kExprF64Lt, 1, -32.4, 11.7);
  TestFloat64Binop(kExprF64Le, 1, -88.9, -88.9);
  TestFloat64Binop(kExprF64Gt, 1, 11.7, -32.4);
  TestFloat64Binop(kExprF64Ge, 1, -88.9, -88.9);

  TestFloat64BinopWithConvert(kExprF64Add, 100, 43.5, 56.5);
  TestFloat64BinopWithConvert(kExprF64Sub, 200, 12200.1, 12000.1);
  TestFloat64BinopWithConvert(kExprF64Mul, -33, 134, -0.25);
  TestFloat64BinopWithConvert(kExprF64Div, -1111, -2222.3, 2);
}

TEST(Run_WasmFloat64Unops) {
  TestFloat64UnopWithConvert(kExprF64Abs, 108, 108.125);
  TestFloat64UnopWithConvert(kExprF64Abs, 209, -209.125);
  TestFloat64UnopWithConvert(kExprF64Neg, -209, 209.125);
  TestFloat64UnopWithConvert(kExprF64Sqrt, 13, 169.4);
}

TEST(Run_WasmFloat32Neg) {
  WasmRunner<float> r(MachineType::Float32());
  BUILD(r, WASM_F32_NEG(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    CHECK_EQ(0x80000000,
             bit_cast<uint32_t>(*i) ^ bit_cast<uint32_t>(r.Call(*i)));
  }
}


TEST(Run_WasmFloat64Neg) {
  WasmRunner<double> r(MachineType::Float64());
  BUILD(r, WASM_F64_NEG(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    CHECK_EQ(0x8000000000000000,
             bit_cast<uint64_t>(*i) ^ bit_cast<uint64_t>(r.Call(*i)));
  }
}


TEST(Run_Wasm_IfElse_P) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // if (p0) return 11; else return 22;
  BUILD(r, WASM_IF_ELSE(WASM_GET_LOCAL(0),  // --
                        WASM_I8(11),        // --
                        WASM_I8(22)));      // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 11 : 22;
    CHECK_EQ(expected, r.Call(*i));
  }
}

TEST(Run_Wasm_If_chain) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // if (p0) 13; if (p0) 14; 15
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_I8(13)),
        WASM_IF(WASM_GET_LOCAL(0), WASM_I8(14)), WASM_I8(15));
  FOR_INT32_INPUTS(i) { CHECK_EQ(15, r.Call(*i)); }
}

TEST(Run_Wasm_If_chain_set) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  // if (p0) p1 = 73; if (p0) p1 = 74; p1
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_SET_LOCAL(1, WASM_I8(73))),
        WASM_IF(WASM_GET_LOCAL(0), WASM_SET_LOCAL(1, WASM_I8(74))),
        WASM_GET_LOCAL(1));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 74 : *i;
    CHECK_EQ(expected, r.Call(*i, *i));
  }
}

TEST(Run_Wasm_IfElse_Unreachable1) {
  WasmRunner<int32_t> r;
  // if (0) unreachable; else return 22;
  BUILD(r, WASM_IF_ELSE(WASM_ZERO,         // --
                        WASM_UNREACHABLE,  // --
                        WASM_I8(27)));     // --
  CHECK_EQ(27, r.Call());
}


TEST(Run_Wasm_Return12) {
  WasmRunner<int32_t> r;

  BUILD(r, RET_I8(12));
  CHECK_EQ(12, r.Call());
}


TEST(Run_Wasm_Return17) {
  WasmRunner<int32_t> r;

  BUILD(r, B1(RET_I8(17)));
  CHECK_EQ(17, r.Call());
}


TEST(Run_Wasm_Return_I32) {
  WasmRunner<int32_t> r(MachineType::Int32());

  BUILD(r, RET(WASM_GET_LOCAL(0)));

  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, r.Call(*i)); }
}


TEST(Run_Wasm_Return_F32) {
  WasmRunner<float> r(MachineType::Float32());

  BUILD(r, RET(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    float expect = *i;
    float result = r.Call(expect);
    if (std::isnan(expect)) {
      CHECK(std::isnan(result));
    } else {
      CHECK_EQ(expect, result);
    }
  }
}


TEST(Run_Wasm_Return_F64) {
  WasmRunner<double> r(MachineType::Float64());

  BUILD(r, RET(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    double expect = *i;
    double result = r.Call(expect);
    if (std::isnan(expect)) {
      CHECK(std::isnan(result));
    } else {
      CHECK_EQ(expect, result);
    }
  }
}


TEST(Run_Wasm_Select) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // return select(11, 22, a);
  BUILD(r, WASM_SELECT(WASM_I8(11), WASM_I8(22), WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 11 : 22;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_Select_strict1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // select(a=0, a=1, a=2); return a
  BUILD(r, B2(WASM_SELECT(WASM_SET_LOCAL(0, WASM_I8(0)),
                          WASM_SET_LOCAL(0, WASM_I8(1)),
                          WASM_SET_LOCAL(0, WASM_I8(2))),
              WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(2, r.Call(*i)); }
}


TEST(Run_Wasm_Select_strict2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  r.AllocateLocal(kAstI32);
  r.AllocateLocal(kAstI32);
  // select(b=5, c=6, a)
  BUILD(r, WASM_SELECT(WASM_SET_LOCAL(1, WASM_I8(5)),
                       WASM_SET_LOCAL(2, WASM_I8(6)), WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 5 : 6;
    CHECK_EQ(expected, r.Call(*i));
  }
}

TEST(Run_Wasm_Select_strict3) {
  WasmRunner<int32_t> r(MachineType::Int32());
  r.AllocateLocal(kAstI32);
  r.AllocateLocal(kAstI32);
  // select(b=5, c=6, a=b)
  BUILD(r, WASM_SELECT(WASM_SET_LOCAL(1, WASM_I8(5)),
                       WASM_SET_LOCAL(2, WASM_I8(6)),
                       WASM_SET_LOCAL(0, WASM_GET_LOCAL(1))));
  FOR_INT32_INPUTS(i) {
    int32_t expected = 5;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_BrIf_strict) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(
      r,
      B2(B1(WASM_BRV_IF(0, WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_I8(99)))),
         WASM_GET_LOCAL(0)));

  FOR_INT32_INPUTS(i) { CHECK_EQ(99, r.Call(*i)); }
}

TEST(Run_Wasm_BrTable0a) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        B2(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 0, BR_TARGET(0))), WASM_I8(91)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(91, r.Call(*i)); }
}

TEST(Run_Wasm_BrTable0b) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        B2(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 1, BR_TARGET(0), BR_TARGET(0))),
           WASM_I8(92)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(92, r.Call(*i)); }
}

TEST(Run_Wasm_BrTable0c) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(
      r,
      B2(B2(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 1, BR_TARGET(0), BR_TARGET(1))),
            RET_I8(76)),
         WASM_I8(77)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i == 0 ? 76 : 77;
    CHECK_EQ(expected, r.Call(*i));
  }
}

TEST(Run_Wasm_BrTable1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 0, BR_TARGET(0))), RET_I8(93));
  FOR_INT32_INPUTS(i) { CHECK_EQ(93, r.Call(*i)); }
}

TEST(Run_Wasm_BrTable_loop) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        B2(WASM_LOOP(1, WASM_BR_TABLE(WASM_INC_LOCAL_BY(0, 1), 2, BR_TARGET(2),
                                      BR_TARGET(1), BR_TARGET(0))),
           RET_I8(99)),
        WASM_I8(98));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(98, r.Call(-1));
  CHECK_EQ(98, r.Call(-2));
  CHECK_EQ(98, r.Call(-3));
  CHECK_EQ(98, r.Call(-100));
}

TEST(Run_Wasm_BrTable_br) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        B2(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 1, BR_TARGET(1), BR_TARGET(0))),
           RET_I8(91)),
        WASM_I8(99));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(91, r.Call(1));
  CHECK_EQ(91, r.Call(2));
  CHECK_EQ(91, r.Call(3));
}

TEST(Run_Wasm_BrTable_br2) {
  WasmRunner<int32_t> r(MachineType::Int32());

  BUILD(r, B2(B2(B2(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 3, BR_TARGET(1),
                                     BR_TARGET(2), BR_TARGET(3), BR_TARGET(0))),
                    RET_I8(85)),
                 RET_I8(86)),
              RET_I8(87)),
        WASM_I8(88));
  CHECK_EQ(86, r.Call(0));
  CHECK_EQ(87, r.Call(1));
  CHECK_EQ(88, r.Call(2));
  CHECK_EQ(85, r.Call(3));
  CHECK_EQ(85, r.Call(4));
  CHECK_EQ(85, r.Call(5));
}

TEST(Run_Wasm_BrTable4) {
  for (int i = 0; i < 4; i++) {
    for (int t = 0; t < 4; t++) {
      uint32_t cases[] = {0, 1, 2, 3};
      cases[i] = t;
      byte code[] = {B2(B2(B2(B2(B1(WASM_BR_TABLE(
                                     WASM_GET_LOCAL(0), 3, BR_TARGET(cases[0]),
                                     BR_TARGET(cases[1]), BR_TARGET(cases[2]),
                                     BR_TARGET(cases[3]))),
                                 RET_I8(70)),
                              RET_I8(71)),
                           RET_I8(72)),
                        RET_I8(73)),
                     WASM_I8(75)};

      WasmRunner<int32_t> r(MachineType::Int32());
      r.Build(code, code + arraysize(code));

      for (int x = -3; x < 50; x++) {
        int index = (x > 3 || x < 0) ? 3 : x;
        int32_t expected = 70 + cases[index];
        CHECK_EQ(expected, r.Call(x));
      }
    }
  }
}

TEST(Run_Wasm_BrTable4x4) {
  for (byte a = 0; a < 4; a++) {
    for (byte b = 0; b < 4; b++) {
      for (byte c = 0; c < 4; c++) {
        for (byte d = 0; d < 4; d++) {
          for (int i = 0; i < 4; i++) {
            uint32_t cases[] = {a, b, c, d};
            byte code[] = {
                B2(B2(B2(B2(B1(WASM_BR_TABLE(
                                WASM_GET_LOCAL(0), 3, BR_TARGET(cases[0]),
                                BR_TARGET(cases[1]), BR_TARGET(cases[2]),
                                BR_TARGET(cases[3]))),
                            RET_I8(50)),
                         RET_I8(51)),
                      RET_I8(52)),
                   RET_I8(53)),
                WASM_I8(55)};

            WasmRunner<int32_t> r(MachineType::Int32());
            r.Build(code, code + arraysize(code));

            for (int x = -6; x < 47; x++) {
              int index = (x > 3 || x < 0) ? 3 : x;
              int32_t expected = 50 + cases[index];
              CHECK_EQ(expected, r.Call(x));
            }
          }
        }
      }
    }
  }
}

TEST(Run_Wasm_BrTable4_fallthru) {
  byte code[] = {
      B2(B2(B2(B2(B1(WASM_BR_TABLE(WASM_GET_LOCAL(0), 3, BR_TARGET(0),
                                   BR_TARGET(1), BR_TARGET(2), BR_TARGET(3))),
                  WASM_INC_LOCAL_BY(1, 1)),
               WASM_INC_LOCAL_BY(1, 2)),
            WASM_INC_LOCAL_BY(1, 4)),
         WASM_INC_LOCAL_BY(1, 8)),
      WASM_GET_LOCAL(1)};

  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  r.Build(code, code + arraysize(code));

  CHECK_EQ(15, r.Call(0, 0));
  CHECK_EQ(14, r.Call(1, 0));
  CHECK_EQ(12, r.Call(2, 0));
  CHECK_EQ(8, r.Call(3, 0));
  CHECK_EQ(8, r.Call(4, 0));

  CHECK_EQ(115, r.Call(0, 100));
  CHECK_EQ(114, r.Call(1, 100));
  CHECK_EQ(112, r.Call(2, 100));
  CHECK_EQ(108, r.Call(3, 100));
  CHECK_EQ(108, r.Call(4, 100));
}

TEST(Run_Wasm_F32ReinterpretI32) {
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  WasmRunner<int32_t> r(&module);

  BUILD(r, WASM_I32_REINTERPRET_F32(
               WASM_LOAD_MEM(MachineType::Float32(), WASM_ZERO)));

  FOR_INT32_INPUTS(i) {
    int32_t expected = *i;
    memory[0] = expected;
    CHECK_EQ(expected, r.Call());
  }
}


TEST(Run_Wasm_I32ReinterpretF32) {
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  WasmRunner<int32_t> r(&module, MachineType::Int32());

  BUILD(r, WASM_BLOCK(
               2, WASM_STORE_MEM(MachineType::Float32(), WASM_ZERO,
                                 WASM_F32_REINTERPRET_I32(WASM_GET_LOCAL(0))),
               WASM_I8(107)));

  FOR_INT32_INPUTS(i) {
    int32_t expected = *i;
    CHECK_EQ(107, r.Call(expected));
    CHECK_EQ(expected, memory[0]);
  }
}


TEST(Run_Wasm_ReturnStore) {
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  WasmRunner<int32_t> r(&module);

  BUILD(r, WASM_STORE_MEM(MachineType::Int32(), WASM_ZERO,
                          WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)));

  FOR_INT32_INPUTS(i) {
    int32_t expected = *i;
    memory[0] = expected;
    CHECK_EQ(expected, r.Call());
  }
}


TEST(Run_Wasm_VoidReturn1) {
  // We use a wrapper function because WasmRunner<void> does not exist.

  // Build the test function.
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.v_v(), &module);
  BUILD(t, kExprNop);
  uint32_t index = t.CompileAndAdd();

  const int32_t kExpected = -414444;
  // Build the calling function.
  WasmRunner<int32_t> r(&module);
  BUILD(r, B2(WASM_CALL_FUNCTION0(index), WASM_I32V_3(kExpected)));

  int32_t result = r.Call();
  CHECK_EQ(kExpected, result);
}


TEST(Run_Wasm_VoidReturn2) {
  // We use a wrapper function because WasmRunner<void> does not exist.
  // Build the test function.
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.v_v(), &module);
  BUILD(t, WASM_RETURN0);
  uint32_t index = t.CompileAndAdd();

  const int32_t kExpected = -414444;
  // Build the calling function.
  WasmRunner<int32_t> r(&module);
  BUILD(r, B2(WASM_CALL_FUNCTION0(index), WASM_I32V_3(kExpected)));

  int32_t result = r.Call();
  CHECK_EQ(kExpected, result);
}


TEST(Run_Wasm_Block_If_P) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // { if (p0) return 51; return 52; }
  BUILD(r, B2(                                     // --
               WASM_IF(WASM_GET_LOCAL(0),          // --
                       WASM_BRV(1, WASM_I8(51))),  // --
               WASM_I8(52)));                      // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 51 : 52;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_Block_BrIf_P) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B2(WASM_BRV_IF(0, WASM_I8(51), WASM_GET_LOCAL(0)), WASM_I8(52)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 51 : 52;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_Block_IfElse_P_assign) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // { if (p0) p0 = 71; else p0 = 72; return p0; }
  BUILD(r, B2(                                                // --
               WASM_IF_ELSE(WASM_GET_LOCAL(0),                // --
                            WASM_SET_LOCAL(0, WASM_I8(71)),   // --
                            WASM_SET_LOCAL(0, WASM_I8(72))),  // --
               WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 71 : 72;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_Block_IfElse_P_return) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // if (p0) return 81; else return 82;
  BUILD(r,                               // --
        WASM_IF_ELSE(WASM_GET_LOCAL(0),  // --
                     RET_I8(81),         // --
                     RET_I8(82)));       // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 81 : 82;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_Block_If_P_assign) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // { if (p0) p0 = 61; p0; }
  BUILD(r, WASM_BLOCK(
               2, WASM_IF(WASM_GET_LOCAL(0), WASM_SET_LOCAL(0, WASM_I8(61))),
               WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 61 : *i;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_DanglingAssign) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // { return 0; p0 = 0; }
  BUILD(r, B2(RET_I8(99), WASM_SET_LOCAL(0, WASM_ZERO)));
  CHECK_EQ(99, r.Call(1));
}


TEST(Run_Wasm_ExprIf_P) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // p0 ? 11 : 22;
  BUILD(r, WASM_IF_ELSE(WASM_GET_LOCAL(0),  // --
                        WASM_I8(11),        // --
                        WASM_I8(22)));      // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 11 : 22;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_ExprIf_P_fallthru) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // p0 ? 11 : 22;
  BUILD(r, WASM_IF_ELSE(WASM_GET_LOCAL(0),  // --
                        WASM_I8(11),        // --
                        WASM_I8(22)));      // --
  FOR_INT32_INPUTS(i) {
    int32_t expected = *i ? 11 : 22;
    CHECK_EQ(expected, r.Call(*i));
  }
}


TEST(Run_Wasm_CountDown) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        WASM_BLOCK(
            2, WASM_LOOP(
                   1, WASM_IF(WASM_GET_LOCAL(0),
                              WASM_BRV(1, WASM_SET_LOCAL(
                                              0, WASM_I32_SUB(WASM_GET_LOCAL(0),
                                                              WASM_I8(1)))))),
            WASM_GET_LOCAL(0)));
  CHECK_EQ(0, r.Call(1));
  CHECK_EQ(0, r.Call(10));
  CHECK_EQ(0, r.Call(100));
}


TEST(Run_Wasm_CountDown_fallthru) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        WASM_BLOCK(
            2, WASM_LOOP(3, WASM_IF(WASM_NOT(WASM_GET_LOCAL(0)), WASM_BREAK(1)),
                         WASM_SET_LOCAL(
                             0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(1))),
                         WASM_CONTINUE(0)),
            WASM_GET_LOCAL(0)));
  CHECK_EQ(0, r.Call(1));
  CHECK_EQ(0, r.Call(10));
  CHECK_EQ(0, r.Call(100));
}


TEST(Run_Wasm_WhileCountDown) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(
               2, WASM_WHILE(WASM_GET_LOCAL(0),
                             WASM_SET_LOCAL(0, WASM_I32_SUB(WASM_GET_LOCAL(0),
                                                            WASM_I8(1)))),
               WASM_GET_LOCAL(0)));
  CHECK_EQ(0, r.Call(1));
  CHECK_EQ(0, r.Call(10));
  CHECK_EQ(0, r.Call(100));
}


TEST(Run_Wasm_Loop_if_break1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B2(WASM_LOOP(2, WASM_IF(WASM_GET_LOCAL(0), WASM_BREAK(1)),
                        WASM_SET_LOCAL(0, WASM_I8(99))),
              WASM_GET_LOCAL(0)));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(3, r.Call(3));
  CHECK_EQ(10000, r.Call(10000));
  CHECK_EQ(-29, r.Call(-29));
}


TEST(Run_Wasm_Loop_if_break2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B2(WASM_LOOP(2, WASM_BR_IF(1, WASM_GET_LOCAL(0)),
                        WASM_SET_LOCAL(0, WASM_I8(99))),
              WASM_GET_LOCAL(0)));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(3, r.Call(3));
  CHECK_EQ(10000, r.Call(10000));
  CHECK_EQ(-29, r.Call(-29));
}


TEST(Run_Wasm_Loop_if_break_fallthru) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B1(WASM_LOOP(2, WASM_IF(WASM_GET_LOCAL(0), WASM_BREAK(1)),
                        WASM_SET_LOCAL(0, WASM_I8(93)))),
        WASM_GET_LOCAL(0));
  CHECK_EQ(93, r.Call(0));
  CHECK_EQ(3, r.Call(3));
  CHECK_EQ(10001, r.Call(10001));
  CHECK_EQ(-22, r.Call(-22));
}

TEST(Run_Wasm_IfBreak1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_SEQ(WASM_BR(0), WASM_UNREACHABLE)),
        WASM_I8(91));
  CHECK_EQ(91, r.Call(0));
  CHECK_EQ(91, r.Call(1));
  CHECK_EQ(91, r.Call(-8734));
}

TEST(Run_Wasm_IfBreak2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_SEQ(WASM_BR(0), RET_I8(77))),
        WASM_I8(81));
  CHECK_EQ(81, r.Call(0));
  CHECK_EQ(81, r.Call(1));
  CHECK_EQ(81, r.Call(-8734));
}

TEST(Run_Wasm_LoadMemI32) {
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  module.RandomizeMemory(1111);

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_I8(0)));

  memory[0] = 99999999;
  CHECK_EQ(99999999, r.Call(0));

  memory[0] = 88888888;
  CHECK_EQ(88888888, r.Call(0));

  memory[0] = 77777777;
  CHECK_EQ(77777777, r.Call(0));
}


TEST(Run_Wasm_LoadMemI32_oob) {
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  WasmRunner<int32_t> r(&module, MachineType::Uint32());
  module.RandomizeMemory(1111);

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(0)));

  memory[0] = 88888888;
  CHECK_EQ(88888888, r.Call(0u));
  for (uint32_t offset = 29; offset < 40; offset++) {
    CHECK_TRAP(r.Call(offset));
  }

  for (uint32_t offset = 0x80000000; offset < 0x80000010; offset++) {
    CHECK_TRAP(r.Call(offset));
  }
}


TEST(Run_Wasm_LoadMemI32_oob_asm) {
  TestingModule module;
  module.origin = kAsmJsOrigin;
  int32_t* memory = module.AddMemoryElems<int32_t>(8);
  WasmRunner<int32_t> r(&module, MachineType::Uint32());
  module.RandomizeMemory(1112);

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(0)));

  memory[0] = 999999;
  CHECK_EQ(999999, r.Call(0u));
  // TODO(titzer): offset 29-31 should also be OOB.
  for (uint32_t offset = 32; offset < 40; offset++) {
    CHECK_EQ(0, r.Call(offset));
  }

  for (uint32_t offset = 0x80000000; offset < 0x80000010; offset++) {
    CHECK_EQ(0, r.Call(offset));
  }
}


TEST(Run_Wasm_LoadMem_offset_oob) {
  TestingModule module;
  module.AddMemoryElems<int32_t>(8);

  static const MachineType machineTypes[] = {
      MachineType::Int8(),   MachineType::Uint8(),  MachineType::Int16(),
      MachineType::Uint16(), MachineType::Int32(),  MachineType::Uint32(),
      MachineType::Int64(),  MachineType::Uint64(), MachineType::Float32(),
      MachineType::Float64()};

  for (size_t m = 0; m < arraysize(machineTypes); m++) {
    module.RandomizeMemory(1116 + static_cast<int>(m));
    WasmRunner<int32_t> r(&module, MachineType::Uint32());
    uint32_t boundary = 24 - WasmOpcodes::MemSize(machineTypes[m]);

    BUILD(r, WASM_LOAD_MEM_OFFSET(machineTypes[m], 8, WASM_GET_LOCAL(0)),
          WASM_ZERO);

    CHECK_EQ(0, r.Call(boundary));  // in bounds.

    for (uint32_t offset = boundary + 1; offset < boundary + 19; offset++) {
      CHECK_TRAP(r.Call(offset));  // out of bounds.
    }
  }
}


TEST(Run_Wasm_LoadMemI32_offset) {
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(4);
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  module.RandomizeMemory(1111);

  BUILD(r, WASM_LOAD_MEM_OFFSET(MachineType::Int32(), 4, WASM_GET_LOCAL(0)));

  memory[0] = 66666666;
  memory[1] = 77777777;
  memory[2] = 88888888;
  memory[3] = 99999999;
  CHECK_EQ(77777777, r.Call(0));
  CHECK_EQ(88888888, r.Call(4));
  CHECK_EQ(99999999, r.Call(8));

  memory[0] = 11111111;
  memory[1] = 22222222;
  memory[2] = 33333333;
  memory[3] = 44444444;
  CHECK_EQ(22222222, r.Call(0));
  CHECK_EQ(33333333, r.Call(4));
  CHECK_EQ(44444444, r.Call(8));
}


#if !V8_TARGET_ARCH_MIPS && !V8_TARGET_ARCH_MIPS64

TEST(Run_Wasm_LoadMemI32_const_oob_misaligned) {
  const int kMemSize = 12;
  // TODO(titzer): Fix misaligned accesses on MIPS and re-enable.
  for (int offset = 0; offset < kMemSize + 5; offset++) {
    for (int index = 0; index < kMemSize + 5; index++) {
      TestingModule module;
      module.AddMemoryElems<byte>(kMemSize);

      WasmRunner<int32_t> r(&module);
      module.RandomizeMemory();

      BUILD(r,
            WASM_LOAD_MEM_OFFSET(MachineType::Int32(), offset, WASM_I8(index)));

      if ((offset + index) <= (kMemSize - sizeof(int32_t))) {
        CHECK_EQ(module.raw_val_at<int32_t>(offset + index), r.Call());
      } else {
        CHECK_TRAP(r.Call());
      }
    }
  }
}

#endif


TEST(Run_Wasm_LoadMemI32_const_oob) {
  const int kMemSize = 24;
  for (int offset = 0; offset < kMemSize + 5; offset += 4) {
    for (int index = 0; index < kMemSize + 5; index += 4) {
      TestingModule module;
      module.AddMemoryElems<byte>(kMemSize);

      WasmRunner<int32_t> r(&module);
      module.RandomizeMemory();

      BUILD(r,
            WASM_LOAD_MEM_OFFSET(MachineType::Int32(), offset, WASM_I8(index)));

      if ((offset + index) <= (kMemSize - sizeof(int32_t))) {
        CHECK_EQ(module.raw_val_at<int32_t>(offset + index), r.Call());
      } else {
        CHECK_TRAP(r.Call());
      }
    }
  }
}


TEST(Run_Wasm_StoreMemI32_offset) {
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(4);
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  const int32_t kWritten = 0xaabbccdd;

  BUILD(r, WASM_STORE_MEM_OFFSET(MachineType::Int32(), 4, WASM_GET_LOCAL(0),
                                 WASM_I32V_5(kWritten)));

  for (int i = 0; i < 2; i++) {
    module.RandomizeMemory(1111);
    memory[0] = 66666666;
    memory[1] = 77777777;
    memory[2] = 88888888;
    memory[3] = 99999999;
    CHECK_EQ(kWritten, r.Call(i * 4));
    CHECK_EQ(66666666, memory[0]);
    CHECK_EQ(i == 0 ? kWritten : 77777777, memory[1]);
    CHECK_EQ(i == 1 ? kWritten : 88888888, memory[2]);
    CHECK_EQ(i == 2 ? kWritten : 99999999, memory[3]);
  }
}


TEST(Run_Wasm_StoreMem_offset_oob) {
  TestingModule module;
  byte* memory = module.AddMemoryElems<byte>(32);

#if WASM_64
  static const MachineType machineTypes[] = {
      MachineType::Int8(),   MachineType::Uint8(),  MachineType::Int16(),
      MachineType::Uint16(), MachineType::Int32(),  MachineType::Uint32(),
      MachineType::Int64(),  MachineType::Uint64(), MachineType::Float32(),
      MachineType::Float64()};
#else
  static const MachineType machineTypes[] = {
      MachineType::Int8(),    MachineType::Uint8(),  MachineType::Int16(),
      MachineType::Uint16(),  MachineType::Int32(),  MachineType::Uint32(),
      MachineType::Float32(), MachineType::Float64()};
#endif

  for (size_t m = 0; m < arraysize(machineTypes); m++) {
    module.RandomizeMemory(1119 + static_cast<int>(m));
    WasmRunner<int32_t> r(&module, MachineType::Uint32());

    BUILD(r, WASM_STORE_MEM_OFFSET(machineTypes[m], 8, WASM_GET_LOCAL(0),
                                   WASM_LOAD_MEM(machineTypes[m], WASM_ZERO)),
          WASM_ZERO);

    byte memsize = WasmOpcodes::MemSize(machineTypes[m]);
    uint32_t boundary = 24 - memsize;
    CHECK_EQ(0, r.Call(boundary));  // in bounds.
    CHECK_EQ(0, memcmp(&memory[0], &memory[8 + boundary], memsize));

    for (uint32_t offset = boundary + 1; offset < boundary + 19; offset++) {
      CHECK_TRAP(r.Call(offset));  // out of bounds.
    }
  }
}


TEST(Run_Wasm_LoadMemI32_P) {
  const int kNumElems = 8;
  TestingModule module;
  int32_t* memory = module.AddMemoryElems<int32_t>(kNumElems);
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  module.RandomizeMemory(2222);

  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(0)));

  for (int i = 0; i < kNumElems; i++) {
    CHECK_EQ(memory[i], r.Call(i * 4));
  }
}


TEST(Run_Wasm_MemI32_Sum) {
  const int kNumElems = 20;
  TestingModule module;
  uint32_t* memory = module.AddMemoryElems<uint32_t>(kNumElems);
  WasmRunner<uint32_t> r(&module, MachineType::Int32());
  const byte kSum = r.AllocateLocal(kAstI32);

  BUILD(r, WASM_BLOCK(
               2, WASM_WHILE(
                      WASM_GET_LOCAL(0),
                      WASM_BLOCK(
                          2, WASM_SET_LOCAL(
                                 kSum, WASM_I32_ADD(
                                           WASM_GET_LOCAL(kSum),
                                           WASM_LOAD_MEM(MachineType::Int32(),
                                                         WASM_GET_LOCAL(0)))),
                          WASM_SET_LOCAL(
                              0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(4))))),
               WASM_GET_LOCAL(1)));

  // Run 4 trials.
  for (int i = 0; i < 3; i++) {
    module.RandomizeMemory(i * 33);
    uint32_t expected = 0;
    for (size_t j = kNumElems - 1; j > 0; j--) {
      expected += memory[j];
    }
    uint32_t result = r.Call(static_cast<int>(4 * (kNumElems - 1)));
    CHECK_EQ(expected, result);
  }
}


TEST(Run_Wasm_CheckMachIntsZero) {
  const int kNumElems = 55;
  TestingModule module;
  module.AddMemoryElems<uint32_t>(kNumElems);
  WasmRunner<uint32_t> r(&module, MachineType::Int32());

  BUILD(r, kExprLoop, kExprGetLocal, 0, kExprIf, kExprGetLocal, 0,
        kExprI32LoadMem, 0, 0, kExprIf, kExprI8Const, 255, kExprReturn, ARITY_1,
        kExprEnd, kExprGetLocal, 0, kExprI8Const, 4, kExprI32Sub, kExprSetLocal,
        0, kExprBr, ARITY_1, DEPTH_0, kExprEnd, kExprEnd, kExprI8Const, 0);

  module.BlankMemory();
  CHECK_EQ(0, r.Call((kNumElems - 1) * 4));
}


TEST(Run_Wasm_MemF32_Sum) {
  const int kSize = 5;
  TestingModule module;
  module.AddMemoryElems<float>(kSize);
  float* buffer = module.raw_mem_start<float>();
  buffer[0] = -99.25;
  buffer[1] = -888.25;
  buffer[2] = -77.25;
  buffer[3] = 66666.25;
  buffer[4] = 5555.25;
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  const byte kSum = r.AllocateLocal(kAstF32);

  BUILD(r, WASM_BLOCK(
               3, WASM_WHILE(
                      WASM_GET_LOCAL(0),
                      WASM_BLOCK(
                          2, WASM_SET_LOCAL(
                                 kSum, WASM_F32_ADD(
                                           WASM_GET_LOCAL(kSum),
                                           WASM_LOAD_MEM(MachineType::Float32(),
                                                         WASM_GET_LOCAL(0)))),
                          WASM_SET_LOCAL(
                              0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(4))))),
               WASM_STORE_MEM(MachineType::Float32(), WASM_ZERO,
                              WASM_GET_LOCAL(kSum)),
               WASM_GET_LOCAL(0)));

  CHECK_EQ(0, r.Call(4 * (kSize - 1)));
  CHECK_NE(-99.25, buffer[0]);
  CHECK_EQ(71256.0f, buffer[0]);
}


template <typename T>
T GenerateAndRunFold(WasmOpcode binop, T* buffer, size_t size,
                     LocalType astType, MachineType memType) {
  TestingModule module;
  module.AddMemoryElems<T>(size);
  for (size_t i = 0; i < size; i++) {
    module.raw_mem_start<T>()[i] = buffer[i];
  }
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  const byte kAccum = r.AllocateLocal(astType);

  BUILD(
      r,
      WASM_BLOCK(
          4, WASM_SET_LOCAL(kAccum, WASM_LOAD_MEM(memType, WASM_ZERO)),
          WASM_WHILE(
              WASM_GET_LOCAL(0),
              WASM_BLOCK(
                  2, WASM_SET_LOCAL(
                         kAccum,
                         WASM_BINOP(binop, WASM_GET_LOCAL(kAccum),
                                    WASM_LOAD_MEM(memType, WASM_GET_LOCAL(0)))),
                  WASM_SET_LOCAL(
                      0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(sizeof(T)))))),
          WASM_STORE_MEM(memType, WASM_ZERO, WASM_GET_LOCAL(kAccum)),
          WASM_GET_LOCAL(0)));
  r.Call(static_cast<int>(sizeof(T) * (size - 1)));
  return module.raw_mem_at<double>(0);
}


TEST(Run_Wasm_MemF64_Mul) {
  const size_t kSize = 6;
  double buffer[kSize] = {1, 2, 2, 2, 2, 2};
  double result = GenerateAndRunFold<double>(kExprF64Mul, buffer, kSize,
                                             kAstF64, MachineType::Float64());
  CHECK_EQ(32, result);
}


TEST(Build_Wasm_Infinite_Loop) {
  WasmRunner<int32_t> r(MachineType::Int32());
  // Only build the graph and compile, don't run.
  BUILD(r, WASM_INFINITE_LOOP);
}


TEST(Build_Wasm_Infinite_Loop_effect) {
  TestingModule module;
  module.AddMemoryElems<int8_t>(16);
  WasmRunner<int32_t> r(&module, MachineType::Int32());

  // Only build the graph and compile, don't run.
  BUILD(r, WASM_LOOP(1, WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)));
}


TEST(Run_Wasm_Unreachable0a) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B2(WASM_BRV(0, WASM_I8(9)), RET(WASM_GET_LOCAL(0))));
  CHECK_EQ(9, r.Call(0));
  CHECK_EQ(9, r.Call(1));
}


TEST(Run_Wasm_Unreachable0b) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B2(WASM_BRV(0, WASM_I8(7)), WASM_UNREACHABLE));
  CHECK_EQ(7, r.Call(0));
  CHECK_EQ(7, r.Call(1));
}


TEST(Build_Wasm_Unreachable1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_UNREACHABLE);
}


TEST(Build_Wasm_Unreachable2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_UNREACHABLE, WASM_UNREACHABLE);
}


TEST(Build_Wasm_Unreachable3) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_UNREACHABLE, WASM_UNREACHABLE, WASM_UNREACHABLE);
}


TEST(Build_Wasm_UnreachableIf1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_UNREACHABLE, WASM_IF(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0)));
}


TEST(Build_Wasm_UnreachableIf2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_UNREACHABLE,
        WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), WASM_UNREACHABLE));
}


TEST(Run_Wasm_Unreachable_Load) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B2(WASM_BRV(0, WASM_GET_LOCAL(0)),
              WASM_LOAD_MEM(MachineType::Int8(), WASM_GET_LOCAL(0))));
  CHECK_EQ(11, r.Call(11));
  CHECK_EQ(21, r.Call(21));
}


TEST(Run_Wasm_Infinite_Loop_not_taken1) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B2(WASM_IF(WASM_GET_LOCAL(0), WASM_INFINITE_LOOP), WASM_I8(45)));
  // Run the code, but don't go into the infinite loop.
  CHECK_EQ(45, r.Call(0));
}


TEST(Run_Wasm_Infinite_Loop_not_taken2) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B1(WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_BRV(1, WASM_I8(45)),
                           WASM_INFINITE_LOOP)));
  // Run the code, but don't go into the infinite loop.
  CHECK_EQ(45, r.Call(1));
}


TEST(Run_Wasm_Infinite_Loop_not_taken2_brif) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        B2(WASM_BRV_IF(0, WASM_I8(45), WASM_GET_LOCAL(0)), WASM_INFINITE_LOOP));
  // Run the code, but don't go into the infinite loop.
  CHECK_EQ(45, r.Call(1));
}


static void TestBuildGraphForSimpleExpression(WasmOpcode opcode) {
  Isolate* isolate = CcTest::InitIsolateOnce();
  Zone zone(isolate->allocator());
  HandleScope scope(isolate);
  // Enable all optional operators.
  CommonOperatorBuilder common(&zone);
  MachineOperatorBuilder machine(&zone, MachineType::PointerRepresentation(),
                                 MachineOperatorBuilder::kAllOptionalOps);
  Graph graph(&zone);
  JSGraph jsgraph(isolate, &graph, &common, nullptr, nullptr, &machine);
  FunctionSig* sig = WasmOpcodes::Signature(opcode);

  if (sig->parameter_count() == 1) {
    byte code[] = {WASM_NO_LOCALS, kExprGetLocal, 0, static_cast<byte>(opcode)};
    TestBuildingGraph(&zone, &jsgraph, nullptr, sig, nullptr, code,
                      code + arraysize(code));
  } else {
    CHECK_EQ(2, sig->parameter_count());
    byte code[] = {WASM_NO_LOCALS,           kExprGetLocal, 0, kExprGetLocal, 1,
                   static_cast<byte>(opcode)};
    TestBuildingGraph(&zone, &jsgraph, nullptr, sig, nullptr, code,
                      code + arraysize(code));
  }
}


TEST(Build_Wasm_SimpleExprs) {
// Test that the decoder can build a graph for all supported simple expressions.
#define GRAPH_BUILD_TEST(name, opcode, sig) \
  TestBuildGraphForSimpleExpression(kExpr##name);

  FOREACH_SIMPLE_OPCODE(GRAPH_BUILD_TEST);

#undef GRAPH_BUILD_TEST
}


TEST(Run_Wasm_Int32LoadInt8_signext) {
  TestingModule module;
  const int kNumElems = 16;
  int8_t* memory = module.AddMemoryElems<int8_t>(kNumElems);
  module.RandomizeMemory();
  memory[0] = -1;
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  BUILD(r, WASM_LOAD_MEM(MachineType::Int8(), WASM_GET_LOCAL(0)));

  for (size_t i = 0; i < kNumElems; i++) {
    CHECK_EQ(memory[i], r.Call(static_cast<int>(i)));
  }
}


TEST(Run_Wasm_Int32LoadInt8_zeroext) {
  TestingModule module;
  const int kNumElems = 16;
  byte* memory = module.AddMemory(kNumElems);
  module.RandomizeMemory(77);
  memory[0] = 255;
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  BUILD(r, WASM_LOAD_MEM(MachineType::Uint8(), WASM_GET_LOCAL(0)));

  for (size_t i = 0; i < kNumElems; i++) {
    CHECK_EQ(memory[i], r.Call(static_cast<int>(i)));
  }
}


TEST(Run_Wasm_Int32LoadInt16_signext) {
  TestingModule module;
  const int kNumBytes = 16;
  byte* memory = module.AddMemory(kNumBytes);
  module.RandomizeMemory(888);
  memory[1] = 200;
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  BUILD(r, WASM_LOAD_MEM(MachineType::Int16(), WASM_GET_LOCAL(0)));

  for (size_t i = 0; i < kNumBytes; i += 2) {
    int32_t expected = memory[i] | (static_cast<int8_t>(memory[i + 1]) << 8);
    CHECK_EQ(expected, r.Call(static_cast<int>(i)));
  }
}


TEST(Run_Wasm_Int32LoadInt16_zeroext) {
  TestingModule module;
  const int kNumBytes = 16;
  byte* memory = module.AddMemory(kNumBytes);
  module.RandomizeMemory(9999);
  memory[1] = 204;
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  BUILD(r, WASM_LOAD_MEM(MachineType::Uint16(), WASM_GET_LOCAL(0)));

  for (size_t i = 0; i < kNumBytes; i += 2) {
    int32_t expected = memory[i] | (memory[i + 1] << 8);
    CHECK_EQ(expected, r.Call(static_cast<int>(i)));
  }
}


TEST(Run_WasmInt32Global) {
  TestingModule module;
  int32_t* global = module.AddGlobal<int32_t>(MachineType::Int32());
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  // global = global + p0
  BUILD(r, WASM_STORE_GLOBAL(
               0, WASM_I32_ADD(WASM_LOAD_GLOBAL(0), WASM_GET_LOCAL(0))));

  *global = 116;
  for (int i = 9; i < 444444; i += 111111) {
    int32_t expected = *global + i;
    r.Call(i);
    CHECK_EQ(expected, *global);
  }
}


TEST(Run_WasmInt32Globals_DontAlias) {
  const int kNumGlobals = 3;
  TestingModule module;
  int32_t* globals[] = {module.AddGlobal<int32_t>(MachineType::Int32()),
                        module.AddGlobal<int32_t>(MachineType::Int32()),
                        module.AddGlobal<int32_t>(MachineType::Int32())};

  for (int g = 0; g < kNumGlobals; g++) {
    // global = global + p0
    WasmRunner<int32_t> r(&module, MachineType::Int32());
    BUILD(r, WASM_STORE_GLOBAL(
                 g, WASM_I32_ADD(WASM_LOAD_GLOBAL(g), WASM_GET_LOCAL(0))));

    // Check that reading/writing global number {g} doesn't alter the others.
    *globals[g] = 116 * g;
    int32_t before[kNumGlobals];
    for (int i = 9; i < 444444; i += 111113) {
      int32_t sum = *globals[g] + i;
      for (int j = 0; j < kNumGlobals; j++) before[j] = *globals[j];
      r.Call(i);
      for (int j = 0; j < kNumGlobals; j++) {
        int32_t expected = j == g ? sum : before[j];
        CHECK_EQ(expected, *globals[j]);
      }
    }
  }
}


TEST(Run_WasmFloat32Global) {
  TestingModule module;
  float* global = module.AddGlobal<float>(MachineType::Float32());
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  // global = global + p0
  BUILD(r, B2(WASM_STORE_GLOBAL(
                  0, WASM_F32_ADD(WASM_LOAD_GLOBAL(0),
                                  WASM_F32_SCONVERT_I32(WASM_GET_LOCAL(0)))),
              WASM_ZERO));

  *global = 1.25;
  for (int i = 9; i < 4444; i += 1111) {
    volatile float expected = *global + i;
    r.Call(i);
    CHECK_EQ(expected, *global);
  }
}


TEST(Run_WasmFloat64Global) {
  TestingModule module;
  double* global = module.AddGlobal<double>(MachineType::Float64());
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  // global = global + p0
  BUILD(r, B2(WASM_STORE_GLOBAL(
                  0, WASM_F64_ADD(WASM_LOAD_GLOBAL(0),
                                  WASM_F64_SCONVERT_I32(WASM_GET_LOCAL(0)))),
              WASM_ZERO));

  *global = 1.25;
  for (int i = 9; i < 4444; i += 1111) {
    volatile double expected = *global + i;
    r.Call(i);
    CHECK_EQ(expected, *global);
  }
}


TEST(Run_WasmMixedGlobals) {
  TestingModule module;
  int32_t* unused = module.AddGlobal<int32_t>(MachineType::Int32());
  byte* memory = module.AddMemory(32);

  int8_t* var_int8 = module.AddGlobal<int8_t>(MachineType::Int8());
  uint8_t* var_uint8 = module.AddGlobal<uint8_t>(MachineType::Uint8());
  int16_t* var_int16 = module.AddGlobal<int16_t>(MachineType::Int16());
  uint16_t* var_uint16 = module.AddGlobal<uint16_t>(MachineType::Uint16());
  int32_t* var_int32 = module.AddGlobal<int32_t>(MachineType::Int32());
  uint32_t* var_uint32 = module.AddGlobal<uint32_t>(MachineType::Uint32());
  float* var_float = module.AddGlobal<float>(MachineType::Float32());
  double* var_double = module.AddGlobal<double>(MachineType::Float64());

  WasmRunner<int32_t> r(&module, MachineType::Int32());

  BUILD(
      r,
      WASM_BLOCK(
          9,
          WASM_STORE_GLOBAL(1, WASM_LOAD_MEM(MachineType::Int8(), WASM_ZERO)),
          WASM_STORE_GLOBAL(2, WASM_LOAD_MEM(MachineType::Uint8(), WASM_ZERO)),
          WASM_STORE_GLOBAL(3, WASM_LOAD_MEM(MachineType::Int16(), WASM_ZERO)),
          WASM_STORE_GLOBAL(4, WASM_LOAD_MEM(MachineType::Uint16(), WASM_ZERO)),
          WASM_STORE_GLOBAL(5, WASM_LOAD_MEM(MachineType::Int32(), WASM_ZERO)),
          WASM_STORE_GLOBAL(6, WASM_LOAD_MEM(MachineType::Uint32(), WASM_ZERO)),
          WASM_STORE_GLOBAL(7,
                            WASM_LOAD_MEM(MachineType::Float32(), WASM_ZERO)),
          WASM_STORE_GLOBAL(8,
                            WASM_LOAD_MEM(MachineType::Float64(), WASM_ZERO)),
          WASM_ZERO));

  memory[0] = 0xaa;
  memory[1] = 0xcc;
  memory[2] = 0x55;
  memory[3] = 0xee;
  memory[4] = 0x33;
  memory[5] = 0x22;
  memory[6] = 0x11;
  memory[7] = 0x99;
  r.Call(1);

  CHECK(static_cast<int8_t>(0xaa) == *var_int8);
  CHECK(static_cast<uint8_t>(0xaa) == *var_uint8);
  CHECK(static_cast<int16_t>(0xccaa) == *var_int16);
  CHECK(static_cast<uint16_t>(0xccaa) == *var_uint16);
  CHECK(static_cast<int32_t>(0xee55ccaa) == *var_int32);
  CHECK(static_cast<uint32_t>(0xee55ccaa) == *var_uint32);
  CHECK(bit_cast<float>(0xee55ccaa) == *var_float);
  CHECK(bit_cast<double>(0x99112233ee55ccaaULL) == *var_double);

  USE(unused);
}


TEST(Run_WasmCallEmpty) {
  const int32_t kExpected = -414444;
  // Build the target function.
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.i_v(), &module);
  BUILD(t, WASM_I32V_3(kExpected));
  uint32_t index = t.CompileAndAdd();

  // Build the calling function.
  WasmRunner<int32_t> r(&module);
  BUILD(r, WASM_CALL_FUNCTION0(index));

  int32_t result = r.Call();
  CHECK_EQ(kExpected, result);
}


TEST(Run_WasmCallF32StackParameter) {
  // Build the target function.
  LocalType param_types[20];
  for (int i = 0; i < 20; i++) param_types[i] = kAstF32;
  FunctionSig sig(1, 19, param_types);
  TestingModule module;
  WasmFunctionCompiler t(&sig, &module);
  BUILD(t, WASM_GET_LOCAL(17));
  uint32_t index = t.CompileAndAdd();

  // Build the calling function.
  WasmRunner<float> r(&module);
  BUILD(r, WASM_CALL_FUNCTIONN(
               19, index, WASM_F32(1.0f), WASM_F32(2.0f), WASM_F32(4.0f),
               WASM_F32(8.0f), WASM_F32(16.0f), WASM_F32(32.0f),
               WASM_F32(64.0f), WASM_F32(128.0f), WASM_F32(256.0f),
               WASM_F32(1.5f), WASM_F32(2.5f), WASM_F32(4.5f), WASM_F32(8.5f),
               WASM_F32(16.5f), WASM_F32(32.5f), WASM_F32(64.5f),
               WASM_F32(128.5f), WASM_F32(256.5f), WASM_F32(512.5f)));

  float result = r.Call();
  CHECK_EQ(256.5f, result);
}


TEST(Run_WasmCallF64StackParameter) {
  // Build the target function.
  LocalType param_types[20];
  for (int i = 0; i < 20; i++) param_types[i] = kAstF64;
  FunctionSig sig(1, 19, param_types);
  TestingModule module;
  WasmFunctionCompiler t(&sig, &module);
  BUILD(t, WASM_GET_LOCAL(17));
  uint32_t index = t.CompileAndAdd();

  // Build the calling function.
  WasmRunner<double> r(&module);
  BUILD(r, WASM_CALL_FUNCTIONN(19, index, WASM_F64(1.0), WASM_F64(2.0),
                               WASM_F64(4.0), WASM_F64(8.0), WASM_F64(16.0),
                               WASM_F64(32.0), WASM_F64(64.0), WASM_F64(128.0),
                               WASM_F64(256.0), WASM_F64(1.5), WASM_F64(2.5),
                               WASM_F64(4.5), WASM_F64(8.5), WASM_F64(16.5),
                               WASM_F64(32.5), WASM_F64(64.5), WASM_F64(128.5),
                               WASM_F64(256.5), WASM_F64(512.5)));

  float result = r.Call();
  CHECK_EQ(256.5, result);
}

TEST(Run_WasmCallVoid) {
  const byte kMemOffset = 8;
  const int32_t kElemNum = kMemOffset / sizeof(int32_t);
  const int32_t kExpected = -414444;
  // Build the target function.
  TestSignatures sigs;
  TestingModule module;
  module.AddMemory(16);
  module.RandomizeMemory();
  WasmFunctionCompiler t(sigs.v_v(), &module);
  BUILD(t, WASM_STORE_MEM(MachineType::Int32(), WASM_I8(kMemOffset),
                          WASM_I32V_3(kExpected)));
  uint32_t index = t.CompileAndAdd();

  // Build the calling function.
  WasmRunner<int32_t> r(&module);
  BUILD(r, WASM_CALL_FUNCTION0(index),
        WASM_LOAD_MEM(MachineType::Int32(), WASM_I8(kMemOffset)));

  int32_t result = r.Call();
  CHECK_EQ(kExpected, result);
  CHECK_EQ(kExpected, module.raw_mem_start<int32_t>()[kElemNum]);
}


TEST(Run_WasmCall_Int32Add) {
  // Build the target function.
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.i_ii(), &module);
  BUILD(t, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  uint32_t index = t.CompileAndAdd();

  // Build the caller function.
  WasmRunner<int32_t> r(&module, MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_CALL_FUNCTION2(index, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      int32_t expected = static_cast<int32_t>(static_cast<uint32_t>(*i) +
                                              static_cast<uint32_t>(*j));
      CHECK_EQ(expected, r.Call(*i, *j));
    }
  }
}

TEST(Run_WasmCall_Float32Sub) {
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t(sigs.f_ff(), &module);

  // Build the target function.
  BUILD(t, WASM_F32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  uint32_t index = t.CompileAndAdd();

  // Builder the caller function.
  WasmRunner<float> r(&module, MachineType::Float32(), MachineType::Float32());
  BUILD(r, WASM_CALL_FUNCTION2(index, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) { CHECK_FLOAT_EQ(*i - *j, r.Call(*i, *j)); }
  }
}


TEST(Run_WasmCall_Float64Sub) {
  TestingModule module;
  double* memory = module.AddMemoryElems<double>(16);
  WasmRunner<int32_t> r(&module);

  BUILD(r, WASM_BLOCK(
               2, WASM_STORE_MEM(
                      MachineType::Float64(), WASM_ZERO,
                      WASM_F64_SUB(
                          WASM_LOAD_MEM(MachineType::Float64(), WASM_ZERO),
                          WASM_LOAD_MEM(MachineType::Float64(), WASM_I8(8)))),
               WASM_I8(107)));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) {
      memory[0] = *i;
      memory[1] = *j;
      double expected = *i - *j;
      CHECK_EQ(107, r.Call());
      if (expected != expected) {
        CHECK(memory[0] != memory[0]);
      } else {
        CHECK_EQ(expected, memory[0]);
      }
    }
  }
}

#define ADD_CODE(vec, ...)                                              \
  do {                                                                  \
    byte __buf[] = {__VA_ARGS__};                                       \
    for (size_t i = 0; i < sizeof(__buf); i++) vec.push_back(__buf[i]); \
  } while (false)


static void Run_WasmMixedCall_N(int start) {
  const int kExpected = 6333;
  const int kElemSize = 8;
  TestSignatures sigs;

#if WASM_64
  static MachineType mixed[] = {
      MachineType::Int32(),   MachineType::Float32(), MachineType::Int64(),
      MachineType::Float64(), MachineType::Float32(), MachineType::Int64(),
      MachineType::Int32(),   MachineType::Float64(), MachineType::Float32(),
      MachineType::Float64(), MachineType::Int32(),   MachineType::Int64(),
      MachineType::Int32(),   MachineType::Int32()};
#else
  static MachineType mixed[] = {
      MachineType::Int32(),   MachineType::Float32(), MachineType::Float64(),
      MachineType::Float32(), MachineType::Int32(),   MachineType::Float64(),
      MachineType::Float32(), MachineType::Float64(), MachineType::Int32(),
      MachineType::Int32(),   MachineType::Int32()};
#endif

  int num_params = static_cast<int>(arraysize(mixed)) - start;
  for (int which = 0; which < num_params; which++) {
    v8::base::AccountingAllocator allocator;
    Zone zone(&allocator);
    TestingModule module;
    module.AddMemory(1024);
    MachineType* memtypes = &mixed[start];
    MachineType result = memtypes[which];

    // =========================================================================
    // Build the selector function.
    // =========================================================================
    uint32_t index;
    FunctionSig::Builder b(&zone, 1, num_params);
    b.AddReturn(WasmOpcodes::LocalTypeFor(result));
    for (int i = 0; i < num_params; i++) {
      b.AddParam(WasmOpcodes::LocalTypeFor(memtypes[i]));
    }
    WasmFunctionCompiler t(b.Build(), &module);
    BUILD(t, WASM_GET_LOCAL(which));
    index = t.CompileAndAdd();

    // =========================================================================
    // Build the calling function.
    // =========================================================================
    WasmRunner<int32_t> r(&module);
    std::vector<byte> code;

    // Load the offset for the store.
    ADD_CODE(code, WASM_ZERO);

    // Load the arguments.
    for (int i = 0; i < num_params; i++) {
      int offset = (i + 1) * kElemSize;
      ADD_CODE(code, WASM_LOAD_MEM(memtypes[i], WASM_I8(offset)));
    }

    // Call the selector function.
    ADD_CODE(code, kExprCallFunction, static_cast<byte>(num_params),
             static_cast<byte>(index));

    // Store the result in memory.
    ADD_CODE(code,
             static_cast<byte>(WasmOpcodes::LoadStoreOpcodeOf(result, true)),
             ZERO_ALIGNMENT, ZERO_OFFSET);

    // Return the expected value.
    ADD_CODE(code, WASM_I32V_2(kExpected));

    r.Build(&code[0], &code[0] + code.size());

    // Run the code.
    for (int t = 0; t < 10; t++) {
      module.RandomizeMemory();
      CHECK_EQ(kExpected, r.Call());

      int size = WasmOpcodes::MemSize(result);
      for (int i = 0; i < size; i++) {
        int base = (which + 1) * kElemSize;
        byte expected = module.raw_mem_at<byte>(base + i);
        byte result = module.raw_mem_at<byte>(i);
        CHECK_EQ(expected, result);
      }
    }
  }
}


TEST(Run_WasmMixedCall_0) { Run_WasmMixedCall_N(0); }
TEST(Run_WasmMixedCall_1) { Run_WasmMixedCall_N(1); }
TEST(Run_WasmMixedCall_2) { Run_WasmMixedCall_N(2); }
TEST(Run_WasmMixedCall_3) { Run_WasmMixedCall_N(3); }

TEST(Run_Wasm_AddCall) {
  TestSignatures sigs;
  TestingModule module;
  WasmFunctionCompiler t1(sigs.i_ii(), &module);
  BUILD(t1, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t1.CompileAndAdd();

  WasmRunner<int32_t> r(&module, MachineType::Int32());
  byte local = r.AllocateLocal(kAstI32);
  BUILD(r, B2(WASM_SET_LOCAL(local, WASM_I8(99)),
              WASM_I32_ADD(
                  WASM_CALL_FUNCTION2(t1.function_index_, WASM_GET_LOCAL(0),
                                      WASM_GET_LOCAL(0)),
                  WASM_CALL_FUNCTION2(t1.function_index_, WASM_GET_LOCAL(1),
                                      WASM_GET_LOCAL(local)))));

  CHECK_EQ(198, r.Call(0));
  CHECK_EQ(200, r.Call(1));
  CHECK_EQ(100, r.Call(-49));
}

TEST(Run_Wasm_CountDown_expr) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_LOOP(
               3, WASM_IF(WASM_NOT(WASM_GET_LOCAL(0)),
                          WASM_BREAKV(1, WASM_GET_LOCAL(0))),
               WASM_SET_LOCAL(0, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(1))),
               WASM_CONTINUE(0)));
  CHECK_EQ(0, r.Call(1));
  CHECK_EQ(0, r.Call(10));
  CHECK_EQ(0, r.Call(100));
}


TEST(Run_Wasm_ExprBlock2a) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B2(WASM_IF(WASM_GET_LOCAL(0), WASM_BRV(1, WASM_I8(1))), WASM_I8(1)));
  CHECK_EQ(1, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}


TEST(Run_Wasm_ExprBlock2b) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B2(WASM_IF(WASM_GET_LOCAL(0), WASM_BRV(1, WASM_I8(1))), WASM_I8(2)));
  CHECK_EQ(2, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}


TEST(Run_Wasm_ExprBlock2c) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B2(WASM_BRV_IF(0, WASM_I8(1), WASM_GET_LOCAL(0)), WASM_I8(1)));
  CHECK_EQ(1, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}


TEST(Run_Wasm_ExprBlock2d) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, B2(WASM_BRV_IF(0, WASM_I8(1), WASM_GET_LOCAL(0)), WASM_I8(2)));
  CHECK_EQ(2, r.Call(0));
  CHECK_EQ(1, r.Call(1));
}


TEST(Run_Wasm_ExprBlock_ManualSwitch) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r, WASM_BLOCK(6, WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(1)),
                                 WASM_BRV(1, WASM_I8(11))),
                      WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(2)),
                              WASM_BRV(1, WASM_I8(12))),
                      WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(3)),
                              WASM_BRV(1, WASM_I8(13))),
                      WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(4)),
                              WASM_BRV(1, WASM_I8(14))),
                      WASM_IF(WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(5)),
                              WASM_BRV(1, WASM_I8(15))),
                      WASM_I8(99)));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(11, r.Call(1));
  CHECK_EQ(12, r.Call(2));
  CHECK_EQ(13, r.Call(3));
  CHECK_EQ(14, r.Call(4));
  CHECK_EQ(15, r.Call(5));
  CHECK_EQ(99, r.Call(6));
}


TEST(Run_Wasm_ExprBlock_ManualSwitch_brif) {
  WasmRunner<int32_t> r(MachineType::Int32());
  BUILD(r,
        WASM_BLOCK(6, WASM_BRV_IF(0, WASM_I8(11),
                                  WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(1))),
                   WASM_BRV_IF(0, WASM_I8(12),
                               WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(2))),
                   WASM_BRV_IF(0, WASM_I8(13),
                               WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(3))),
                   WASM_BRV_IF(0, WASM_I8(14),
                               WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(4))),
                   WASM_BRV_IF(0, WASM_I8(15),
                               WASM_I32_EQ(WASM_GET_LOCAL(0), WASM_I8(5))),
                   WASM_I8(99)));
  CHECK_EQ(99, r.Call(0));
  CHECK_EQ(11, r.Call(1));
  CHECK_EQ(12, r.Call(2));
  CHECK_EQ(13, r.Call(3));
  CHECK_EQ(14, r.Call(4));
  CHECK_EQ(15, r.Call(5));
  CHECK_EQ(99, r.Call(6));
}


TEST(Run_Wasm_nested_ifs) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());

  BUILD(r, WASM_IF_ELSE(
               WASM_GET_LOCAL(0),
               WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_I8(11), WASM_I8(12)),
               WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_I8(13), WASM_I8(14))));


  CHECK_EQ(11, r.Call(1, 1));
  CHECK_EQ(12, r.Call(1, 0));
  CHECK_EQ(13, r.Call(0, 1));
  CHECK_EQ(14, r.Call(0, 0));
}


TEST(Run_Wasm_ExprBlock_if) {
  WasmRunner<int32_t> r(MachineType::Int32());

  BUILD(r, B1(WASM_IF_ELSE(WASM_GET_LOCAL(0), WASM_BRV(0, WASM_I8(11)),
                           WASM_BRV(1, WASM_I8(14)))));

  CHECK_EQ(11, r.Call(1));
  CHECK_EQ(14, r.Call(0));
}


TEST(Run_Wasm_ExprBlock_nested_ifs) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());

  BUILD(r, WASM_BLOCK(
               1, WASM_IF_ELSE(
                      WASM_GET_LOCAL(0),
                      WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_BRV(0, WASM_I8(11)),
                                   WASM_BRV(1, WASM_I8(12))),
                      WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_BRV(0, WASM_I8(13)),
                                   WASM_BRV(1, WASM_I8(14))))));

  CHECK_EQ(11, r.Call(1, 1));
  CHECK_EQ(12, r.Call(1, 0));
  CHECK_EQ(13, r.Call(0, 1));
  CHECK_EQ(14, r.Call(0, 0));
}


TEST(Run_Wasm_ExprLoop_nested_ifs) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());

  BUILD(r, WASM_LOOP(
               1, WASM_IF_ELSE(
                      WASM_GET_LOCAL(0),
                      WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_BRV(1, WASM_I8(11)),
                                   WASM_BRV(3, WASM_I8(12))),
                      WASM_IF_ELSE(WASM_GET_LOCAL(1), WASM_BRV(1, WASM_I8(13)),
                                   WASM_BRV(3, WASM_I8(14))))));

  CHECK_EQ(11, r.Call(1, 1));
  CHECK_EQ(12, r.Call(1, 0));
  CHECK_EQ(13, r.Call(0, 1));
  CHECK_EQ(14, r.Call(0, 0));
}

TEST(Run_Wasm_SimpleCallIndirect) {
  TestSignatures sigs;
  TestingModule module;

  WasmFunctionCompiler t1(sigs.i_ii(), &module);
  BUILD(t1, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t1.CompileAndAdd(/*sig_index*/ 1);

  WasmFunctionCompiler t2(sigs.i_ii(), &module);
  BUILD(t2, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t2.CompileAndAdd(/*sig_index*/ 1);

  // Signature table.
  module.AddSignature(sigs.f_ff());
  module.AddSignature(sigs.i_ii());
  module.AddSignature(sigs.d_dd());

  // Function table.
  int table[] = {0, 1};
  module.AddIndirectFunctionTable(table, 2);
  module.PopulateIndirectFunctionTable();

  // Builder the caller function.
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  BUILD(r, WASM_CALL_INDIRECT2(1, WASM_GET_LOCAL(0), WASM_I8(66), WASM_I8(22)));

  CHECK_EQ(88, r.Call(0));
  CHECK_EQ(44, r.Call(1));
  CHECK_TRAP(r.Call(2));
}


TEST(Run_Wasm_MultipleCallIndirect) {
  TestSignatures sigs;
  TestingModule module;

  WasmFunctionCompiler t1(sigs.i_ii(), &module);
  BUILD(t1, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t1.CompileAndAdd(/*sig_index*/ 1);

  WasmFunctionCompiler t2(sigs.i_ii(), &module);
  BUILD(t2, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t2.CompileAndAdd(/*sig_index*/ 1);

  // Signature table.
  module.AddSignature(sigs.f_ff());
  module.AddSignature(sigs.i_ii());
  module.AddSignature(sigs.d_dd());

  // Function table.
  int table[] = {0, 1};
  module.AddIndirectFunctionTable(table, 2);
  module.PopulateIndirectFunctionTable();

  // Builder the caller function.
  WasmRunner<int32_t> r(&module, MachineType::Int32(), MachineType::Int32(),
                        MachineType::Int32());
  BUILD(r, WASM_I32_ADD(
               WASM_CALL_INDIRECT2(1, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1),
                                   WASM_GET_LOCAL(2)),
               WASM_CALL_INDIRECT2(1, WASM_GET_LOCAL(1), WASM_GET_LOCAL(2),
                                   WASM_GET_LOCAL(0))));

  CHECK_EQ(5, r.Call(0, 1, 2));
  CHECK_EQ(19, r.Call(0, 1, 9));
  CHECK_EQ(1, r.Call(1, 0, 2));
  CHECK_EQ(1, r.Call(1, 0, 9));

  CHECK_TRAP(r.Call(0, 2, 1));
  CHECK_TRAP(r.Call(1, 2, 0));
  CHECK_TRAP(r.Call(2, 0, 1));
  CHECK_TRAP(r.Call(2, 1, 0));
}

TEST(Run_Wasm_CallIndirect_NoTable) {
  TestSignatures sigs;
  TestingModule module;

  // One function.
  WasmFunctionCompiler t1(sigs.i_ii(), &module);
  BUILD(t1, WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));
  t1.CompileAndAdd(/*sig_index*/ 1);

  // Signature table.
  module.AddSignature(sigs.f_ff());
  module.AddSignature(sigs.i_ii());

  // Builder the caller function.
  WasmRunner<int32_t> r(&module, MachineType::Int32());
  BUILD(r, WASM_CALL_INDIRECT2(1, WASM_GET_LOCAL(0), WASM_I8(66), WASM_I8(22)));

  CHECK_TRAP(r.Call(0));
  CHECK_TRAP(r.Call(1));
  CHECK_TRAP(r.Call(2));
}

TEST(Run_Wasm_F32Floor) {
  WasmRunner<float> r(MachineType::Float32());
  BUILD(r, WASM_F32_FLOOR(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CHECK_FLOAT_EQ(floorf(*i), r.Call(*i)); }
}

TEST(Run_Wasm_F32Ceil) {
  WasmRunner<float> r(MachineType::Float32());
  BUILD(r, WASM_F32_CEIL(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CHECK_FLOAT_EQ(ceilf(*i), r.Call(*i)); }
}

TEST(Run_Wasm_F32Trunc) {
  WasmRunner<float> r(MachineType::Float32());
  BUILD(r, WASM_F32_TRUNC(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CHECK_FLOAT_EQ(truncf(*i), r.Call(*i)); }
}

TEST(Run_Wasm_F32NearestInt) {
  WasmRunner<float> r(MachineType::Float32());
  BUILD(r, WASM_F32_NEARESTINT(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) { CHECK_FLOAT_EQ(nearbyintf(*i), r.Call(*i)); }
}

TEST(Run_Wasm_F64Floor) {
  WasmRunner<double> r(MachineType::Float64());
  BUILD(r, WASM_F64_FLOOR(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CHECK_DOUBLE_EQ(floor(*i), r.Call(*i)); }
}

TEST(Run_Wasm_F64Ceil) {
  WasmRunner<double> r(MachineType::Float64());
  BUILD(r, WASM_F64_CEIL(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CHECK_DOUBLE_EQ(ceil(*i), r.Call(*i)); }
}

TEST(Run_Wasm_F64Trunc) {
  WasmRunner<double> r(MachineType::Float64());
  BUILD(r, WASM_F64_TRUNC(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CHECK_DOUBLE_EQ(trunc(*i), r.Call(*i)); }
}

TEST(Run_Wasm_F64NearestInt) {
  WasmRunner<double> r(MachineType::Float64());
  BUILD(r, WASM_F64_NEARESTINT(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) { CHECK_DOUBLE_EQ(nearbyint(*i), r.Call(*i)); }
}

TEST(Run_Wasm_F32Min) {
  WasmRunner<float> r(MachineType::Float32(), MachineType::Float32());
  BUILD(r, WASM_F32_MIN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) {
      float expected;
      if (*i < *j) {
        expected = *i;
      } else if (*j < *i) {
        expected = *j;
      } else if (*i != *i) {
        // If *i or *j is NaN, then the result is NaN.
        expected = *i;
      } else {
        expected = *j;
      }

      CHECK_FLOAT_EQ(expected, r.Call(*i, *j));
    }
  }
}


TEST(Run_Wasm_F64Min) {
  WasmRunner<double> r(MachineType::Float64(), MachineType::Float64());
  BUILD(r, WASM_F64_MIN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) {
      double expected;
      if (*i < *j) {
        expected = *i;
      } else if (*j < *i) {
        expected = *j;
      } else if (*i != *i) {
        // If *i or *j is NaN, then the result is NaN.
        expected = *i;
      } else {
        expected = *j;
      }

      CHECK_DOUBLE_EQ(expected, r.Call(*i, *j));
    }
  }
}


TEST(Run_Wasm_F32Max) {
  WasmRunner<float> r(MachineType::Float32(), MachineType::Float32());
  BUILD(r, WASM_F32_MAX(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) {
      float expected;
      if (*i > *j) {
        expected = *i;
      } else if (*j > *i) {
        expected = *j;
      } else if (*i != *i) {
        // If *i or *j is NaN, then the result is NaN.
        expected = *i;
      } else {
        expected = *j;
      }

      CHECK_FLOAT_EQ(expected, r.Call(*i, *j));
    }
  }
}


TEST(Run_Wasm_F64Max) {
  WasmRunner<double> r(MachineType::Float64(), MachineType::Float64());
  BUILD(r, WASM_F64_MAX(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) {
      double expected;
      if (*i > *j) {
        expected = *i;
      } else if (*j > *i) {
        expected = *j;
      } else if (*i != *i) {
        // If *i or *j is NaN, then the result is NaN.
        expected = *i;
      } else {
        expected = *j;
      }

      CHECK_DOUBLE_EQ(expected, r.Call(*i, *j));
    }
  }
}

// TODO(ahaas): Fix on mips and reenable.
#if !V8_TARGET_ARCH_MIPS && !V8_TARGET_ARCH_MIPS64

TEST(Run_Wasm_F32Min_Snan) {
  // Test that the instruction does not return a signalling NaN.
  {
    WasmRunner<float> r;
    BUILD(r,
          WASM_F32_MIN(WASM_F32(bit_cast<float>(0xff80f1e2)), WASM_F32(57.67)));
    CHECK_EQ(0xffc0f1e2, bit_cast<uint32_t>(r.Call()));
  }
  {
    WasmRunner<float> r;
    BUILD(r,
          WASM_F32_MIN(WASM_F32(45.73), WASM_F32(bit_cast<float>(0x7f80f1e2))));
    CHECK_EQ(0x7fc0f1e2, bit_cast<uint32_t>(r.Call()));
  }
}

TEST(Run_Wasm_F32Max_Snan) {
  // Test that the instruction does not return a signalling NaN.
  {
    WasmRunner<float> r;
    BUILD(r,
          WASM_F32_MAX(WASM_F32(bit_cast<float>(0xff80f1e2)), WASM_F32(57.67)));
    CHECK_EQ(0xffc0f1e2, bit_cast<uint32_t>(r.Call()));
  }
  {
    WasmRunner<float> r;
    BUILD(r,
          WASM_F32_MAX(WASM_F32(45.73), WASM_F32(bit_cast<float>(0x7f80f1e2))));
    CHECK_EQ(0x7fc0f1e2, bit_cast<uint32_t>(r.Call()));
  }
}

TEST(Run_Wasm_F64Min_Snan) {
  // Test that the instruction does not return a signalling NaN.
  {
    WasmRunner<double> r;
    BUILD(r, WASM_F64_MIN(WASM_F64(bit_cast<double>(0xfff000000000f1e2)),
                          WASM_F64(57.67)));
    CHECK_EQ(0xfff800000000f1e2, bit_cast<uint64_t>(r.Call()));
  }
  {
    WasmRunner<double> r;
    BUILD(r, WASM_F64_MIN(WASM_F64(45.73),
                          WASM_F64(bit_cast<double>(0x7ff000000000f1e2))));
    CHECK_EQ(0x7ff800000000f1e2, bit_cast<uint64_t>(r.Call()));
  }
}

TEST(Run_Wasm_F64Max_Snan) {
  // Test that the instruction does not return a signalling NaN.
  {
    WasmRunner<double> r;
    BUILD(r, WASM_F64_MAX(WASM_F64(bit_cast<double>(0xfff000000000f1e2)),
                          WASM_F64(57.67)));
    CHECK_EQ(0xfff800000000f1e2, bit_cast<uint64_t>(r.Call()));
  }
  {
    WasmRunner<double> r;
    BUILD(r, WASM_F64_MAX(WASM_F64(45.73),
                          WASM_F64(bit_cast<double>(0x7ff000000000f1e2))));
    CHECK_EQ(0x7ff800000000f1e2, bit_cast<uint64_t>(r.Call()));
  }
}

#endif

TEST(Run_Wasm_I32SConvertF32) {
  WasmRunner<int32_t> r(MachineType::Float32());
  BUILD(r, WASM_I32_SCONVERT_F32(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    if (*i < static_cast<float>(INT32_MAX) &&
        *i >= static_cast<float>(INT32_MIN)) {
      CHECK_EQ(static_cast<int32_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}


TEST(Run_Wasm_I32SConvertF64) {
  WasmRunner<int32_t> r(MachineType::Float64());
  BUILD(r, WASM_I32_SCONVERT_F64(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    if (*i < (static_cast<double>(INT32_MAX) + 1.0) &&
        *i > (static_cast<double>(INT32_MIN) - 1.0)) {
      CHECK_EQ(static_cast<int64_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}


TEST(Run_Wasm_I32UConvertF32) {
  WasmRunner<uint32_t> r(MachineType::Float32());
  BUILD(r, WASM_I32_UCONVERT_F32(WASM_GET_LOCAL(0)));

  FOR_FLOAT32_INPUTS(i) {
    if (*i < (static_cast<float>(UINT32_MAX) + 1.0) && *i > -1) {
      CHECK_EQ(static_cast<uint32_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}


TEST(Run_Wasm_I32UConvertF64) {
  WasmRunner<uint32_t> r(MachineType::Float64());
  BUILD(r, WASM_I32_UCONVERT_F64(WASM_GET_LOCAL(0)));

  FOR_FLOAT64_INPUTS(i) {
    if (*i < (static_cast<float>(UINT32_MAX) + 1.0) && *i > -1) {
      CHECK_EQ(static_cast<uint32_t>(*i), r.Call(*i));
    } else {
      CHECK_TRAP32(r.Call(*i));
    }
  }
}

TEST(Run_Wasm_F64CopySign) {
  WasmRunner<double> r(MachineType::Float64(), MachineType::Float64());
  BUILD(r, WASM_F64_COPYSIGN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT64_INPUTS(i) {
    FOR_FLOAT64_INPUTS(j) { CHECK_DOUBLE_EQ(copysign(*i, *j), r.Call(*i, *j)); }
  }
}


TEST(Run_Wasm_F32CopySign) {
  WasmRunner<float> r(MachineType::Float32(), MachineType::Float32());
  BUILD(r, WASM_F32_COPYSIGN(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  FOR_FLOAT32_INPUTS(i) {
    FOR_FLOAT32_INPUTS(j) { CHECK_FLOAT_EQ(copysignf(*i, *j), r.Call(*i, *j)); }
  }
}

void CompileCallIndirectMany(LocalType param) {
  // Make sure we don't run out of registers when compiling indirect calls
  // with many many parameters.
  TestSignatures sigs;
  for (byte num_params = 0; num_params < 40; num_params++) {
    v8::base::AccountingAllocator allocator;
    Zone zone(&allocator);
    HandleScope scope(CcTest::InitIsolateOnce());
    TestingModule module;
    FunctionSig* sig = sigs.many(&zone, kAstStmt, param, num_params);

    module.AddSignature(sig);
    module.AddSignature(sig);
    module.AddIndirectFunctionTable(nullptr, 0);

    WasmFunctionCompiler t(sig, &module);

    std::vector<byte> code;
    ADD_CODE(code, kExprI8Const, 0);
    for (byte p = 0; p < num_params; p++) {
      ADD_CODE(code, kExprGetLocal, p);
    }
    ADD_CODE(code, kExprCallIndirect, static_cast<byte>(num_params), 1);

    t.Build(&code[0], &code[0] + code.size());
    t.Compile();
  }
}


TEST(Compile_Wasm_CallIndirect_Many_i32) { CompileCallIndirectMany(kAstI32); }


#if WASM_64
TEST(Compile_Wasm_CallIndirect_Many_i64) { CompileCallIndirectMany(kAstI64); }
#endif


TEST(Compile_Wasm_CallIndirect_Many_f32) { CompileCallIndirectMany(kAstF32); }


TEST(Compile_Wasm_CallIndirect_Many_f64) { CompileCallIndirectMany(kAstF64); }

TEST(Run_WASM_Int32RemS_dead) {
  WasmRunner<int32_t> r(MachineType::Int32(), MachineType::Int32());
  BUILD(r, WASM_I32_REMS(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)), WASM_ZERO);
  const int32_t kMin = std::numeric_limits<int32_t>::min();
  CHECK_EQ(0, r.Call(133, 100));
  CHECK_EQ(0, r.Call(kMin, -1));
  CHECK_EQ(0, r.Call(0, 1));
  CHECK_TRAP(r.Call(100, 0));
  CHECK_TRAP(r.Call(-1001, 0));
  CHECK_TRAP(r.Call(kMin, 0));
}
