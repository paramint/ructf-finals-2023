#include <gtest/gtest.h>

#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "utils.h"

void assertCompilationResult(const std::string &programText, const std::string &expectedAssembly, const std::string &expectedError) {
    auto tokens = TokenizeString(programText);
    ASSERT_TRUE(tokens.success);
    auto parsed = ParseTokens(tokens.tokens);
    ASSERT_TRUE(parsed.success);
    auto compiled = CompileToAssembly(parsed.programNode);

    if (!expectedError.empty()) {
        EXPECT_FALSE(compiled.success);
        ASSERT_EQ(compiled.errorMessage, expectedError);
        return;
    }

    Trim(compiled.assemblyCode);
    auto ea = TrimCopy(expectedAssembly);

    EXPECT_TRUE(compiled.success);
    EXPECT_EQ(compiled.errorMessage, "");
    ASSERT_EQ(compiled.assemblyCode, ea);
}

TEST(Compiler, OnlyConstants) {
    assertCompilationResult(R"(
pi = 3.1415927;
x2 = -234234.123123;
e = 2.7;
x1 = 1.23123123;

fun main() { return 0; }
)", R"(
.section .text
.globl main

main:
    push    %rbp
    mov     %rsp,%rbp
    movsd   _c_const_main_0(%rip),%xmm0
    leaveq
    retq


_c_const_main_0: .double 0
e: .double 2.7
pi: .double 3.1415927
x1: .double 1.23123123
x2: .double -234234.123123

)", "");
}

TEST(Compiler, RedifinitionsOfConstant) {
    assertCompilationResult(R"(
pi = 3.1415927;
_x = 42;
x2 = -234234.123123;
e = 2.7;
x1 = 1.23123123;
_x = 43;
)", "", "constant '_x' is defined twice");
}

TEST(Compiler, RedifinitionOfFunction) {
    assertCompilationResult(R"(
fun f() {}
fun main() {}
fun f() {}
)", "", "function 'f' is defined twice");
}

TEST(Compiler, FunctionCallWithInvalidArgumentsAmountLess) {
    assertCompilationResult(R"(
fun f() {}

fun main() { return f(1.0); }
)", "", "invalid arguments count for function call 'f': expected 0, but got 1 (in function 'main')");
}

TEST(Compiler, FunctionCallWithInvalidArgumentsAmountMore) {
    assertCompilationResult(R"(
fun f(x, y) { return x + y; }

fun main() { return f(1.0, 2.0, 3.0); }
)", "", "invalid arguments count for function call 'f': expected 2, but got 3 (in function 'main')");
}

TEST(Compiler, ConstantsFromFunctions) {
    assertCompilationResult(R"(
pi = 3.1415927;
x2 = -234234.123123;
e = 2.7;
x1 = 1.23123123;

fun lol(k) {
    l = 43;
    return 1 * 43 + 45 * k;
}

fun main() {
    return 42 / 1244.2234234;
}
)", R"(
.section .text
.globl main

lol:
    push    %rbp
    mov     %rsp,%rbp
    sub     $0x10,%rsp
    movsd   %xmm0,-0x8(%rbp)
    movsd   _c_const_lol_1(%rip),%xmm0
    sub     $0x10,%rsp
    movsd   %xmm0,(%rsp)
    movsd   _c_const_lol_2(%rip),%xmm0
    movaps  %xmm0,%xmm1
    movsd   (%rsp),%xmm0
    add     $0x10,%rsp
    mulsd   %xmm1,%xmm0
    sub     $0x10,%rsp
    movsd   %xmm0,(%rsp)
    movsd   _c_const_lol_3(%rip),%xmm0
    sub     $0x10,%rsp
    movsd   %xmm0,(%rsp)
    movsd   -0x8(%rbp),%xmm0
    movaps  %xmm0,%xmm1
    movsd   (%rsp),%xmm0
    add     $0x10,%rsp
    mulsd   %xmm1,%xmm0
    movaps  %xmm0,%xmm1
    movsd   (%rsp),%xmm0
    add     $0x10,%rsp
    addsd   %xmm1,%xmm0
    leaveq
    retq

main:
    push    %rbp
    mov     %rsp,%rbp
    movsd   _c_const_main_0(%rip),%xmm0
    sub     $0x10,%rsp
    movsd   %xmm0,(%rsp)
    movsd   _c_const_main_1(%rip),%xmm0
    movaps  %xmm0,%xmm1
    movsd   (%rsp),%xmm0
    add     $0x10,%rsp
    divsd   %xmm1,%xmm0
    leaveq
    retq


_c_const_lol_0: .double 43
_c_const_lol_1: .double 1
_c_const_lol_2: .double 43
_c_const_lol_3: .double 45
_c_const_main_0: .double 42
_c_const_main_1: .double 1244.2234234
e: .double 2.7
pi: .double 3.1415927
x1: .double 1.23123123
x2: .double -234234.123123
)", "");
}

TEST(Compiler, RedefinitionOfConstantFromFunction) {
    assertCompilationResult(R"(
pi = 3.1415927;
x2 = -234234.123123;
e = 2.7;
x1 = 1.23123123;
_c_const_lol_1=1;

fun lol() {
    l = 43;
    return 1 * 43 + 45;
}

fun main() {
    return (42);
}
)", "", "cant define constant '_c_const_lol_1' (do not define it manually)");
}

TEST(Compiler, DefineFunctionWithConstantName) {
    assertCompilationResult(R"(
x = 42;
fun x() {}
)", R"()", "cant define function 'x': there is constant with that name");
}

TEST(Compiler, DefineVariableWithConstantName) {
    assertCompilationResult(R"(
x = 42;
fun main() {
    x = 43;
    return x;
}
)", "", "cant create local variable with name 'x': there is constant with that name");
}

TEST(Compiler, DefineVariableWithFunctionName) {
    assertCompilationResult(R"(
fun main() {
    x = 43;
    return x;
}

fun x() { return 42; }

)", "", "cant create local variable with name 'x': there is function with that name");
}

TEST(Compiler, DefineArgumentWithConstantName) {
    assertCompilationResult(R"(
x = 42;

fun f(x) {
    return x * x;
}
)", "", "cant create argument for 'f' with name 'x': there is constant with that name");
}

TEST(Compiler, DefineArgumentWithFunctionName) {
    assertCompilationResult(R"(
fun main(x) {
    return x * x;
}

fun x() { return 52; }
)", "", "cant create argument for 'main' with name 'x': there is function with that name");
}

TEST(Compiler, RedefinitionOfArgument) {
    assertCompilationResult(R"(
fun main(x, y, x) {
    return x * y * x;
}
)", "", "redefinition of argument 'x' in function 'main'");
}

TEST(Compiler, UnknownVariableInUsage) {
    assertCompilationResult(R"(
fun main(x) {
    return x * 1 / (y);
}
)", "", "unknown variable 'y' in function 'main'");
}

TEST(Compiler, UnknownVariableInFuctionCall) {
    assertCompilationResult(R"(
fun f(x) {
    return x;
}

fun main() {
    return f(y);
}
)", "", "unknown variable 'y' in function 'main'");
}

TEST(Compiler, UnknownFunctionCall) {
    assertCompilationResult(R"(
fun c(x, y) {
    return x + y;
}

fun main() {
    return 1 + c(42, l(44));
}
)", "", "unknown function call 'l' in 'main'");
}

TEST(Compiler, MainFunctionCantGetArguments) {
    assertCompilationResult(R"(
fun main(x) {
    return x;
}
)", "", "main function cant have any arguments");
}

TEST(Compiler, ReturnGlobalConstant) {
    assertCompilationResult(R"(
pi = 3.1415927;
fun main() {
    return pi;
}
)", R"(
.section .text
.globl main

main:
    push    %rbp
    mov     %rsp,%rbp
    movsd   pi(%rip),%xmm0
    leaveq
    retq


pi: .double 3.1415927
)", "");
}
