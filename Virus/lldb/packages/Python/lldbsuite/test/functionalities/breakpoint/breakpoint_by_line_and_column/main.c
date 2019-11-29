//===-- main.c --------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

int square(int x)
{
  return x * x;
}

int main (int argc, char const *argv[])
{
  int did_call = 0;

  // Line 20.                                    v Column 50.
  if(square(argc+1) != 0) { did_call = 1; return square(argc); }
  //                                             ^
  return square(0);
}