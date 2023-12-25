; RUN: llc -mtriple=riscv32 -mcpu=ventus-gpgpu -verify-machineinstrs < %s \
; RUN:   | FileCheck -check-prefix=VENTUS %s

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) vscale_range(1,2048)
define dso_local <16 x double> @func(<16 x double> noundef %x, <16 x double> noundef %y) local_unnamed_addr {
; VENTUS: vsw.v	v33, -4(v32)                    # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v34, -8(v32)                    # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v35, -12(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v36, -16(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v37, -20(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v38, -24(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v39, -28(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v40, -32(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v41, -36(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v42, -40(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v43, -44(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v44, -48(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v45, -52(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v46, -56(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v47, -60(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v48, -64(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v49, -68(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v50, -72(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v51, -76(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v52, -80(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v53, -84(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v54, -88(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v55, -92(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v56, -96(v32)                   # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v57, -100(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v58, -104(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v59, -108(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v60, -112(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v61, -116(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v62, -120(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 72
; VENTUS-NEXT: vsw.v	v63, -124(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v64, -128(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v65, -132(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v66, -136(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v67, -140(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v68, -144(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v69, -148(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v70, -152(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v71, -156(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v72, -160(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v73, -164(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v74, -168(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v75, -172(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v76, -176(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v77, -180(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v78, -184(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v79, -188(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v80, -192(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v81, -196(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v82, -200(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v83, -204(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v84, -208(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v85, -212(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v86, -216(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v87, -220(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v88, -224(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v89, -228(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v90, -232(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v91, -236(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v92, -240(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v93, -244(v32)                  # 4-byte Folded Spill
; VENTUS-NEXT: regext	zero, zero, 136
; VENTUS-NEXT: vsw.v	v94, -248(v32)                  # 4-byte Folded Spill
entry:
  %add = fadd <16 x double> %x, %y
  ret <16 x double> %add
}

; Function Attrs: convergent mustprogress nofree norecurse nounwind willreturn memory(argmem: readwrite) vscale_range(1,2048)
define dso_local ventus_kernel void @test_fn(ptr addrspace(1) nocapture noundef readonly align 128 %x, ptr addrspace(1) nocapture noundef readonly align 128 %y, ptr addrspace(1) nocapture noundef writeonly align 128 %dst) {
; VENTUS: addi	tp, tp, 128
; VENTUS=NEXT: li	t0, 4
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 2
; VENTUS=NEXT: vmv.v.x	v66, t0
; VENTUS=NEXT: regext	zero, zero, 80
; VENTUS=NEXT: vsw.v	v34, 0(v66)
; VENTUS=NEXT: li	t0, 8
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 136
; VENTUS=NEXT: vsw.v	v65, 0(v34)
; VENTUS=NEXT: li	t0, 12
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 136
; VENTUS=NEXT: vsw.v	v64, 0(v34)
; VENTUS=NEXT: li	t0, 16
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v63, 0(v34)
; VENTUS=NEXT: li	t0, 20
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v62, 0(v34)
; VENTUS=NEXT: li	t0, 24
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v61, 0(v34)
; VENTUS=NEXT: li	t0, 28
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v60, 0(v34)
; VENTUS=NEXT: li	t0, 32
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v59, 0(v34)
; VENTUS=NEXT: li	t0, 36
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v58, 0(v34)
; VENTUS=NEXT: li	t0, 40
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v57, 0(v34)
; VENTUS=NEXT: li	t0, 44
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v56, 0(v34)
; VENTUS=NEXT: li	t0, 48
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v55, 0(v34)
; VENTUS=NEXT: li	t0, 52
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v54, 0(v34)
; VENTUS=NEXT: li	t0, 56
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v53, 0(v34)
; VENTUS=NEXT: li	t0, 60
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v52, 0(v34)
; VENTUS=NEXT: li	t0, 64
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v51, 0(v34)
; VENTUS=NEXT: li	t0, 68
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v50, 0(v34)
; VENTUS=NEXT: li	t0, 72
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v49, 0(v34)
; VENTUS=NEXT: li	t0, 76
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v48, 0(v34)
; VENTUS=NEXT: li	t0, 80
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v47, 0(v34)
; VENTUS=NEXT: li	t0, 84
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v46, 0(v34)
; VENTUS=NEXT: li	t0, 88
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v45, 0(v34)
; VENTUS=NEXT: li	t0, 92
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v44, 0(v34)
; VENTUS=NEXT: li	t0, 96
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v43, 0(v34)
; VENTUS=NEXT: li	t0, 100
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v42, 0(v34)
; VENTUS=NEXT: li	t0, 104
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v41, 0(v34)
; VENTUS=NEXT: li	t0, 108
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v40, 0(v34)
; VENTUS=NEXT: li	t0, 112
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v39, 0(v34)
; VENTUS=NEXT: li	t0, 116
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v38, 0(v34)
; VENTUS=NEXT: li	t0, 120
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v37, 0(v34)
; VENTUS=NEXT: li	t0, 124
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v36, 0(v34)
; VENTUS=NEXT: li	t0, 128
; VENTUS=NEXT: sub	t0, tp, t0
; VENTUS=NEXT: regext	zero, zero, 1
; VENTUS=NEXT: vmv.v.x	v34, t0
; VENTUS=NEXT: regext	zero, zero, 72
; VENTUS=NEXT: vsw.v	v35, 0(v34)
; VENTUS=NEXT: call	_Z3minDv16_dS_
; VENTUS=NEXT: addi	tp, tp, -128
entry:
  %call = call i32 @_Z13get_global_idj(i32 noundef 0)
  %arrayidx = getelementptr inbounds <16 x double>, ptr addrspace(1) %x, i32 %call
  %0 = load <16 x double>, ptr addrspace(1) %arrayidx, align 128
  %arrayidx1 = getelementptr inbounds <16 x double>, ptr addrspace(1) %y, i32 %call
  %1 = load <16 x double>, ptr addrspace(1) %arrayidx1, align 128
  %call2 = call <16 x double> @_Z3minDv16_dS_(<16 x double> noundef %0, <16 x double> noundef %1)
  %arrayidx3 = getelementptr inbounds <16 x double>, ptr addrspace(1) %dst, i32 %call
  store <16 x double> %call2, ptr addrspace(1) %arrayidx3, align 128
  ret void
}

; Function Attrs: convergent mustprogress nofree nounwind willreturn memory(none)
declare dso_local i32 @_Z13get_global_idj(i32 noundef) local_unnamed_addr #2

; Function Attrs: convergent mustprogress nofree nounwind willreturn memory(none)
declare dso_local <16 x double> @_Z3minDv16_dS_(<16 x double> noundef, <16 x double> noundef) local_unnamed_addr #2
