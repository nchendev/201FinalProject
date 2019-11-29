; RUN: llc -mcpu=pwr9 -verify-machineinstrs -ppc-vsr-nums-as-vr -ppc-asm-full-reg-names \
; RUN:    -mtriple=powerpc64le-unknown-linux-gnu < %s | FileCheck %s --check-prefix=P9LE
; RUN: llc -mcpu=pwr9 -verify-machineinstrs -ppc-vsr-nums-as-vr -ppc-asm-full-reg-names \
; RUN:    -mtriple=powerpc64-unknown-linux-gnu < %s | FileCheck %s --check-prefix=P9BE
; RUN: llc -mcpu=pwr8 -verify-machineinstrs -ppc-vsr-nums-as-vr -ppc-asm-full-reg-names \
; RUN:    -mtriple=powerpc64le-unknown-linux-gnu < %s | FileCheck %s --check-prefix=P8LE
; RUN: llc -mcpu=pwr8 -verify-machineinstrs -ppc-vsr-nums-as-vr -ppc-asm-full-reg-names \
; RUN:    -mtriple=powerpc64-unknown-linux-gnu < %s | FileCheck %s --check-prefix=P8BE

; Function Attrs: norecurse nounwind readonly
define <2 x i64> @s2v_test1(i32* nocapture readonly %int32, <2 x i64> %vec)  {
; P9LE-LABEL: s2v_test1:
; P9LE:       # %bb.0: # %entry
; P9LE-NEXT:    lfiwax f0, 0, r3
; P9LE-NEXT:    xxpermdi v3, f0, f0, 2
; P9LE-NEXT:    xxpermdi v2, v2, v3, 1
; P9LE-NEXT:    blr

; P9BE-LABEL: s2v_test1:
; P9BE:       # %bb.0: # %entry
; P9BE-NEXT:    lfiwax f0, 0, r3
; P9BE-NEXT:    xxpermdi v2, vs0, v2, 1
; P9BE-NEXT:    blr

; P8LE-LABEL: s2v_test1:
; P8LE:       # %bb.0: # %entry
; P8LE-NEXT:    lfiwax f0, 0, r3
; P8LE-NEXT:    xxpermdi v3, f0, f0, 2
; P8LE-NEXT:    xxpermdi v2, v2, v3, 1
; P8LE-NEXT:    blr

; P8BE-LABEL: s2v_test1:
; P8BE:       # %bb.0: # %entry
; P8BE-NEXT:    lfiwax f0, 0, r3
; P8BE-NEXT:    xxpermdi v2, vs0, v2, 1
; P8BE-NEXT:    blr
entry:
  %0 = load i32, i32* %int32, align 4
  %conv = sext i32 %0 to i64
  %vecins = insertelement <2 x i64> %vec, i64 %conv, i32 0
  ret <2 x i64> %vecins
}

; Function Attrs: norecurse nounwind readonly
define <2 x i64> @s2v_test2(i32* nocapture readonly %int32, <2 x i64> %vec)  {
; P9LE-LABEL: s2v_test2:
; P9LE:       # %bb.0: # %entry
; P9LE-NEXT:    addi r3, r3, 4
; P9LE-NEXT:    lfiwax f0, 0, r3
; P9LE-NEXT:    xxpermdi v3, f0, f0, 2
; P9LE-NEXT:    xxpermdi v2, v2, v3, 1
; P9LE-NEXT:    blr

; P9BE-LABEL: s2v_test2:
; P9BE:       # %bb.0: # %entry
; P9BE-NEXT:    addi r3, r3, 4
; P9BE-NEXT:    lfiwax f0, 0, r3
; P9BE-NEXT:    xxpermdi v2, vs0, v2, 1
; P9BE-NEXT:    blr

; P8LE-LABEL: s2v_test2:
; P8LE:       # %bb.0: # %entry
; P8LE-NEXT:    addi r3, r3, 4
; P8LE-NEXT:    lfiwax f0, 0, r3
; P8LE-NEXT:    xxpermdi v3, f0, f0, 2
; P8LE-NEXT:    xxpermdi v2, v2, v3, 1
; P8LE-NEXT:    blr

; P8BE-LABEL: s2v_test2:
; P8BE:       # %bb.0: # %entry
; P8BE-NEXT:    addi r3, r3, 4
; P8BE-NEXT:    lfiwax f0, 0, r3
; P8BE-NEXT:    xxpermdi v2, vs0, v2, 1
; P8BE-NEXT:    blr
entry:
  %arrayidx = getelementptr inbounds i32, i32* %int32, i64 1
  %0 = load i32, i32* %arrayidx, align 4
  %conv = sext i32 %0 to i64
  %vecins = insertelement <2 x i64> %vec, i64 %conv, i32 0
  ret <2 x i64> %vecins
}

; Function Attrs: norecurse nounwind readonly
define <2 x i64> @s2v_test3(i32* nocapture readonly %int32, <2 x i64> %vec, i32 signext %Idx)  {
; P9LE-LABEL: s2v_test3:
; P9LE:       # %bb.0: # %entry
; P9LE-NEXT:    sldi r4, r7, 2
; P9LE-NEXT:    lfiwax f0, r3, r4
; P9LE-NEXT:    xxpermdi v3, f0, f0, 2
; P9LE-NEXT:    xxpermdi v2, v2, v3, 1
; P9LE-NEXT:    blr

; P9BE-LABEL: s2v_test3:
; P9BE:       # %bb.0: # %entry
; P9BE-NEXT:    sldi r4, r7, 2
; P9BE-NEXT:    lfiwax f0, r3, r4
; P9BE-NEXT:    xxpermdi v2, vs0, v2, 1
; P9BE-NEXT:    blr

; P8LE-LABEL: s2v_test3:
; P8LE:       # %bb.0: # %entry
; P8LE-NEXT:    sldi r4, r7, 2
; P8LE-NEXT:    lfiwax f0, r3, r4
; P8LE-NEXT:    xxpermdi v3, f0, f0, 2
; P8LE-NEXT:    xxpermdi v2, v2, v3, 1
; P8LE-NEXT:    blr

; P8BE-LABEL: s2v_test3:
; P8BE:       # %bb.0: # %entry
; P8BE-NEXT:    sldi r4, r7, 2
; P8BE-NEXT:    lfiwax f0, r3, r4
; P8BE-NEXT:    xxpermdi v2, vs0, v2, 1
; P8BE-NEXT:    blr
entry:
  %idxprom = sext i32 %Idx to i64
  %arrayidx = getelementptr inbounds i32, i32* %int32, i64 %idxprom
  %0 = load i32, i32* %arrayidx, align 4
  %conv = sext i32 %0 to i64
  %vecins = insertelement <2 x i64> %vec, i64 %conv, i32 0
  ret <2 x i64> %vecins
}

; Function Attrs: norecurse nounwind readonly
define <2 x i64> @s2v_test4(i32* nocapture readonly %int32, <2 x i64> %vec)  {
; P9LE-LABEL: s2v_test4:
; P9LE:       # %bb.0: # %entry
; P9LE-NEXT:    addi r3, r3, 4
; P9LE-NEXT:    lfiwax f0, 0, r3
; P9LE-NEXT:    xxpermdi v3, f0, f0, 2
; P9LE-NEXT:    xxpermdi v2, v2, v3, 1
; P9LE-NEXT:    blr

; P9BE-LABEL: s2v_test4:
; P9BE:       # %bb.0: # %entry
; P9BE-NEXT:    addi r3, r3, 4
; P9BE-NEXT:    lfiwax f0, 0, r3
; P9BE-NEXT:    xxpermdi v2, vs0, v2, 1
; P9BE-NEXT:    blr

; P8LE-LABEL: s2v_test4:
; P8LE:       # %bb.0: # %entry
; P8LE-NEXT:    addi r3, r3, 4
; P8LE-NEXT:    lfiwax f0, 0, r3
; P8LE-NEXT:    xxpermdi v3, f0, f0, 2
; P8LE-NEXT:    xxpermdi v2, v2, v3, 1
; P8LE-NEXT:    blr

; P8BE-LABEL: s2v_test4:
; P8BE:       # %bb.0: # %entry
; P8BE-NEXT:    addi r3, r3, 4
; P8BE-NEXT:    lfiwax f0, 0, r3
; P8BE-NEXT:    xxpermdi v2, vs0, v2, 1
; P8BE-NEXT:    blr
entry:
  %arrayidx = getelementptr inbounds i32, i32* %int32, i64 1
  %0 = load i32, i32* %arrayidx, align 4
  %conv = sext i32 %0 to i64
  %vecins = insertelement <2 x i64> %vec, i64 %conv, i32 0
  ret <2 x i64> %vecins
}

; Function Attrs: norecurse nounwind readonly
define <2 x i64> @s2v_test5(<2 x i64> %vec, i32* nocapture readonly %ptr1)  {
; P9LE-LABEL: s2v_test5:
; P9LE:       # %bb.0: # %entry
; P9LE-NEXT:    lfiwax f0, 0, r5
; P9LE-NEXT:    xxpermdi v3, f0, f0, 2
; P9LE-NEXT:    xxpermdi v2, v2, v3, 1
; P9LE-NEXT:    blr

; P9BE-LABEL: s2v_test5:
; P9BE:       # %bb.0: # %entry
; P9BE-NEXT:    lfiwax f0, 0, r5
; P9BE-NEXT:    xxpermdi v2, vs0, v2, 1
; P9BE-NEXT:    blr

; P8LE-LABEL: s2v_test5:
; P8LE:       # %bb.0: # %entry
; P8LE-NEXT:    lfiwax f0, 0, r5
; P8LE-NEXT:    xxpermdi v3, f0, f0, 2
; P8LE-NEXT:    xxpermdi v2, v2, v3, 1
; P8LE-NEXT:    blr

; P8BE-LABEL: s2v_test5:
; P8BE:       # %bb.0: # %entry
; P8BE-NEXT:    lfiwax f0, 0, r5
; P8BE-NEXT:    xxpermdi v2, vs0, v2, 1
; P8BE-NEXT:    blr
entry:
  %0 = load i32, i32* %ptr1, align 4
  %conv = sext i32 %0 to i64
  %vecins = insertelement <2 x i64> %vec, i64 %conv, i32 0
  ret <2 x i64> %vecins
}

; Function Attrs: norecurse nounwind readonly
define <2 x i64> @s2v_test6(i32* nocapture readonly %ptr)  {
; P9LE-LABEL: s2v_test6:
; P9LE:       # %bb.0: # %entry
; P9LE-NEXT:    lfiwax f0, 0, r3
; P9LE-NEXT:    xxpermdi v2, f0, f0, 2
; P9LE-NEXT:    xxspltd v2, v2, 1
; P9LE-NEXT:    blr

; P9BE-LABEL: s2v_test6:
; P9BE:       # %bb.0: # %entry
; P9BE-NEXT:    lfiwax f0, 0, r3
; P9BE-NEXT:    xxspltd v2, vs0, 0
; P9BE-NEXT:    blr

; P8LE-LABEL: s2v_test6:
; P8LE:       # %bb.0: # %entry
; P8LE-NEXT:    lfiwax f0, 0, r3
; P8LE-NEXT:    xxpermdi v2, f0, f0, 2
; P8LE-NEXT:    xxspltd v2, v2, 1
; P8LE-NEXT:    blr

; P8BE-LABEL: s2v_test6:
; P8BE:       # %bb.0: # %entry
; P8BE-NEXT:    lfiwax f0, 0, r3
; P8BE-NEXT:    xxspltd v2, vs0, 0
; P8BE-NEXT:    blr
entry:
  %0 = load i32, i32* %ptr, align 4
  %conv = sext i32 %0 to i64
  %splat.splatinsert = insertelement <2 x i64> undef, i64 %conv, i32 0
  %splat.splat = shufflevector <2 x i64> %splat.splatinsert, <2 x i64> undef, <2 x i32> zeroinitializer
  ret <2 x i64> %splat.splat
}

; Function Attrs: norecurse nounwind readonly
define <2 x i64> @s2v_test7(i32* nocapture readonly %ptr)  {
; P9LE-LABEL: s2v_test7:
; P9LE:       # %bb.0: # %entry
; P9LE-NEXT:    lfiwax f0, 0, r3
; P9LE-NEXT:    xxpermdi v2, f0, f0, 2
; P9LE-NEXT:    xxspltd v2, v2, 1
; P9LE-NEXT:    blr

; P9BE-LABEL: s2v_test7:
; P9BE:       # %bb.0: # %entry
; P9BE-NEXT:    lfiwax f0, 0, r3
; P9BE-NEXT:    xxspltd v2, vs0, 0
; P9BE-NEXT:    blr

; P8LE-LABEL: s2v_test7:
; P8LE:       # %bb.0: # %entry
; P8LE-NEXT:    lfiwax f0, 0, r3
; P8LE-NEXT:    xxpermdi v2, f0, f0, 2
; P8LE-NEXT:    xxspltd v2, v2, 1
; P8LE-NEXT:    blr

; P8BE-LABEL: s2v_test7:
; P8BE:       # %bb.0: # %entry
; P8BE-NEXT:    lfiwax f0, 0, r3
; P8BE-NEXT:    xxspltd v2, vs0, 0
; P8BE-NEXT:    blr
entry:
  %0 = load i32, i32* %ptr, align 4
  %conv = sext i32 %0 to i64
  %splat.splatinsert = insertelement <2 x i64> undef, i64 %conv, i32 0
  %splat.splat = shufflevector <2 x i64> %splat.splatinsert, <2 x i64> undef, <2 x i32> zeroinitializer
  ret <2 x i64> %splat.splat
}

