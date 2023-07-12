	.text
	.attribute	4, 16
	.attribute	5, "rv32i2p0_m2p0_a2p0_zfinx1p0_zdinx1p0_zve32f1p0_zve32x1p0_zvl32b1p0_zhinx1p0"
	.file	"float-intrinsics.cl"
	.globl	_fmax
	.p2align	2
	.type	_fmax,@function
_fmax:
	addi	sp, sp, 4
	addi	tp, tp, 20
	regext	zero, zero, 1
	vmv.v.x	v32, tp
	sw	ra, -4(sp)
	regext	zero, zero, 1
	vadd.vx	v33, v3, zero
	regext	zero, zero, 1
	vadd.vx	v34, v2, zero
	regext	zero, zero, 1
	vadd.vx	v35, v1, zero
	regext	zero, zero, 1
	vadd.vx	v36, v0, zero
	vadd.vx	v2, v0, zero
	vadd.vx	v3, v1, zero
	call	__unorddf2@plt
	regext	zero, zero, 1
	vmsne.vi	v37, v0, 0
	regext	zero, zero, 64
	vadd.vx	v0, v36, zero
	regext	zero, zero, 64
	vadd.vx	v1, v35, zero
	regext	zero, zero, 64
	vadd.vx	v2, v34, zero
	regext	zero, zero, 64
	vadd.vx	v3, v33, zero
	call	__ltdf2@plt
	vmv.v.x	v1, zero
	vmslt.vi	v0, v0, 0
	regext	zero, zero, 64
	vor.vv	v0, v37, v0
	vbne	v0, v1, .LBB0_2
	regext	zero, zero, 65
	vadd.vx	v34, v36, zero
.LBB0_2:
	vmv.v.x	v1, zero
	vbne	v0, v1, .LBB0_4
	regext	zero, zero, 65
	vadd.vx	v33, v35, zero
.LBB0_4:
	regext	zero, zero, 64
	vadd.vx	v0, v34, zero
	regext	zero, zero, 64
	vadd.vx	v1, v33, zero
	lw	ra, -4(sp)
	addi	sp, sp, -4
	addi	tp, tp, -20
	ret
.Lfunc_end0:
	.size	_fmax, .Lfunc_end0-_fmax

	.globl	_fmin
	.p2align	2
	.type	_fmin,@function
_fmin:
	addi	sp, sp, 4
	addi	tp, tp, 20
	regext	zero, zero, 1
	vmv.v.x	v32, tp
	sw	ra, -4(sp)
	regext	zero, zero, 1
	vadd.vx	v34, v3, zero
	regext	zero, zero, 1
	vadd.vx	v36, v2, zero
	regext	zero, zero, 1
	vadd.vx	v33, v1, zero
	regext	zero, zero, 1
	vadd.vx	v35, v0, zero
	vadd.vx	v2, v0, zero
	vadd.vx	v3, v1, zero
	call	__unorddf2@plt
	regext	zero, zero, 1
	vmsne.vi	v37, v0, 0
	regext	zero, zero, 64
	vadd.vx	v0, v35, zero
	regext	zero, zero, 64
	vadd.vx	v1, v33, zero
	regext	zero, zero, 64
	vadd.vx	v2, v36, zero
	regext	zero, zero, 64
	vadd.vx	v3, v34, zero
	call	__ltdf2@plt
	vmv.v.x	v1, zero
	vmslt.vi	v0, v0, 0
	regext	zero, zero, 64
	vor.vv	v0, v37, v0
	vbne	v0, v1, .LBB1_2
	regext	zero, zero, 65
	vadd.vx	v35, v36, zero
.LBB1_2:
	vmv.v.x	v1, zero
	vbne	v0, v1, .LBB1_4
	regext	zero, zero, 65
	vadd.vx	v33, v34, zero
.LBB1_4:
	regext	zero, zero, 64
	vadd.vx	v0, v35, zero
	regext	zero, zero, 64
	vadd.vx	v1, v33, zero
	lw	ra, -4(sp)
	addi	sp, sp, -4
	addi	tp, tp, -20
	ret
.Lfunc_end1:
	.size	_fmin, .Lfunc_end1-_fmin

	.globl	rint
	.p2align	2
	.type	rint,@function
rint:
	addi	sp, sp, 4
	addi	tp, tp, 16
	regext	zero, zero, 1
	vmv.v.x	v32, tp
	sw	ra, -4(sp)
	regext	zero, zero, 1
	vadd.vx	v33, v1, zero
	vadd.vx	v2, v0, zero
	vsrl.vi	v0, v1, 20
	li	t0, 2047
	regext	zero, zero, 1
	vand.vx	v36, v0, t0
	li	t1, 1074
	vmv.v.x	v0, t1
	regext	zero, zero, 8
	vbltu	v0, v36, .LBB2_3
	lui	t0, %hi(rint.TWO52)
	addi	t0, t0, %lo(rint.TWO52)
	regext	zero, zero, 64
	vsrl.vi	v0, v33, 28
	vand.vi	v0, v0, 8
	vadd.vx	v0, v0, t0
	regext	zero, zero, 1
	vlw12.v	v34, 0(v0)
	regext	zero, zero, 1
	vlw12.v	v35, 4(v0)
	regext	zero, zero, 64
	vadd.vx	v0, v34, zero
	regext	zero, zero, 64
	vadd.vx	v1, v35, zero
	regext	zero, zero, 64
	vadd.vx	v3, v33, zero
	call	__adddf3@plt
	regext	zero, zero, 64
	vadd.vx	v2, v34, zero
	regext	zero, zero, 64
	vadd.vx	v3, v35, zero
	call	__subdf3@plt
	li	t0, 1022
	vmv.v.x	v2, t0
	regext	zero, zero, 8
	vbltu	v2, v36, .LBB2_6
	lui	t0, 524288
	addi	t0, t0, -1
	vand.vx	v1, v1, t0
	li	t1, 1
	add	t0, t1, t0
	regext	zero, zero, 64
	vand.vx	v2, v33, t0
	vor.vv	v1, v1, v2
	j	.LBB2_6
.LBB2_3:
	li	t0, 2047
	regext	zero, zero, 64
	vmsne.vx	v0, v36, t0
	vmv.v.x	v3, t0
	regext	zero, zero, 64
	vadd.vx	v1, v33, zero
	regext	zero, zero, 64
	vbeq	v36, v3, .LBB2_5
	lui	t0, 524288
	vmv.v.x	v1, t0
.LBB2_5:
	vadd.vi	v0, v0, -1
	vand.vv	v0, v0, v2
	regext	zero, zero, 64
	vadd.vx	v3, v33, zero
	call	__adddf3@plt
.LBB2_6:
	lw	ra, -4(sp)
	addi	sp, sp, -4
	addi	tp, tp, -16
	ret
.Lfunc_end2:
	.size	rint, .Lfunc_end2-rint

	.globl	round
	.p2align	2
	.type	round,@function
round:
	vadd.vx	v3, v1, zero
	vadd.vx	v2, v0, zero
	vsrl.vi	v0, v1, 20
	li	t0, 2047
	vand.vx	v4, v0, t0
	li	t0, -1023
	vadd.vx	v0, v4, t0
	vmsltu.vv	v1, v0, v4
	li	t0, 1074
	vmv.v.x	v5, t0
	vadd.vi	v1, v1, -1
	vbltu	v5, v4, .LBB3_16
	li	t0, 1022
	vmv.v.x	v5, t0
	vbltu	v5, v4, .LBB3_5
	lui	t0, 524288
	vand.vv	v0, v0, v1
	li	t1, -1
	vmv.v.x	v1, t1
	vand.vx	v3, v3, t0
	vbne	v0, v1, .LBB3_4
	lui	t0, 261888
	vor.vx	v3, v3, t0
.LBB3_4:
	vmv.v.x	v2, zero
	j	.LBB3_15
.LBB3_5:
	vmv.v.x	v8, zero
	vsll.vv	v7, v2, v0
	li	t0, -32
	vadd.vx	v5, v0, t0
	vmslt.vi	v1, v5, 0
	vrsub.vi	v4, v1, 0
	li	t0, 31
	vxor.vx	v6, v0, t0
	vblt	v5, v8, .LBB3_7
	vsll.vv	v9, v2, v5
	j	.LBB3_8
.LBB3_7:
	vsrl.vi	v8, v2, 1
	vsrl.vv	v8, v8, v6
	vsll.vv	v9, v3, v0
	vor.vv	v9, v9, v8
.LBB3_8:
	vmv.v.x	v10, zero
	vand.vv	v8, v4, v7
	lui	t2, 256
	addi	t0, t2, -1
	vand.vx	v9, v9, t0
	lui	t0, 128
	vblt	v5, v10, .LBB3_10
	vmv.v.x	v7, t0
	vsrl.vv	v7, v7, v5
	j	.LBB3_11
.LBB3_10:
	vmv.v.x	v7, t2
	vsll.vv	v7, v7, v6
.LBB3_11:
	vmv.v.x	v10, zero
	vor.vv	v8, v8, v9
	lui	t2, 1048320
	vblt	v5, v10, .LBB3_13
	vmv.v.x	v6, t2
	vsra.vv	v5, v6, v5
	vmv.v.x	v6, zero
	vbne	v8, v6, .LBB3_14
	j	.LBB3_15
.LBB3_13:
	lui	t1, 1048064
	vmv.v.x	v5, t1
	vsll.vv	v5, v5, v6
	vmv.v.x	v6, zero
	vbeq	v8, v6, .LBB3_15
.LBB3_14:
	vmv.v.x	v6, t0
	vsrl.vv	v6, v6, v0
	vand.vv	v4, v4, v6
	vadd.vv	v2, v7, v2
	vmsltu.vv	v6, v2, v7
	vadd.vv	v3, v4, v3
	vadd.vv	v3, v3, v6
	vmv.v.x	v4, t2
	vsra.vv	v0, v4, v0
	vadd.vi	v1, v1, -1
	vor.vv	v0, v1, v0
	vand.vv	v2, v2, v5
	vand.vv	v3, v3, v0
.LBB3_15:
	vadd.vx	v0, v2, zero
	vadd.vx	v1, v3, zero
	ret
.LBB3_16:
	vmv.v.x	v4, zero
	li	t0, 1024
	vxor.vx	v0, v0, t0
	vor.vv	v5, v0, v1
	vmsne.vi	v0, v5, 0
	vadd.vx	v1, v3, zero
	vbeq	v5, v4, .LBB3_18
	lui	t0, 524288
	vmv.v.x	v1, t0
.LBB3_18:
	addi	sp, sp, 4
	sw	ra, -4(sp)
	vadd.vi	v0, v0, -1
	vand.vv	v0, v0, v2
	call	__adddf3@plt
	vadd.vx	v2, v0, zero
	vadd.vx	v3, v1, zero
	lw	ra, -4(sp)
	addi	sp, sp, -4
	j	.LBB3_15
.Lfunc_end3:
	.size	round, .Lfunc_end3-round

	.globl	ceil
	.p2align	2
	.type	ceil,@function
ceil:
	vadd.vx	v3, v1, zero
	vadd.vx	v2, v0, zero
	vsrl.vi	v0, v1, 20
	li	t0, 2047
	vand.vx	v1, v0, t0
	li	t0, -1023
	li	t1, 1074
	vmv.v.x	v4, t1
	vadd.vx	v0, v1, t0
	vbltu	v4, v1, .LBB4_4
	li	t0, 1022
	vmv.v.x	v4, t0
	vbltu	v4, v1, .LBB4_7
	vmv.v.x	v0, zero
	vbge	v3, v0, .LBB4_9
	lui	t0, 524288
	vmv.v.x	v3, t0
	j	.LBB4_22
.LBB4_4:
	li	t0, 1024
	vmsne.vx	v4, v0, t0
	vmv.v.x	v5, t0
	vadd.vx	v1, v3, zero
	vbeq	v0, v5, .LBB4_6
	lui	t0, 524288
	vmv.v.x	v1, t0
.LBB4_6:
	addi	sp, sp, 4
	sw	ra, -4(sp)
	vadd.vi	v0, v4, -1
	vand.vv	v0, v0, v2
	call	__adddf3@plt
	vadd.vx	v2, v0, zero
	vadd.vx	v3, v1, zero
	lw	ra, -4(sp)
	addi	sp, sp, -4
	j	.LBB4_23
.LBB4_7:
	vmv.v.x	v5, zero
	lui	t0, 256
	addi	t0, t0, -1
	vsll.vv	v1, v2, v0
	li	t1, -32
	vadd.vx	v4, v0, t1
	vmslt.vi	v6, v4, 0
	vrsub.vi	v6, v6, 0
	vand.vv	v1, v6, v1
	vblt	v4, v5, .LBB4_10
	vsll.vv	v4, v2, v4
	j	.LBB4_11
.LBB4_9:
	vor.vv	v1, v2, v3
	vmseq.vi	v1, v1, 0
	vadd.vi	v1, v1, -1
	lui	t0, 261888
	vand.vx	v3, v1, t0
	j	.LBB4_22
.LBB4_10:
	li	t1, 31
	vxor.vx	v4, v0, t1
	vsrl.vi	v5, v2, 1
	vsrl.vv	v4, v5, v4
	vsll.vv	v5, v3, v0
	vor.vv	v4, v5, v4
.LBB4_11:
	vmv.v.x	v5, zero
	vand.vx	v4, v4, t0
	vor.vv	v1, v1, v4
	vbeq	v1, v5, .LBB4_23
	vmv.v.x	v1, zero
	li	t2, -1
	vbeq	v3, v1, .LBB4_14
	vmv.v.x	v1, zero
	vmslt.vv	v1, v1, v3
	j	.LBB4_15
.LBB4_14:
	vmsne.vi	v1, v2, 0
.LBB4_15:
	vmv.v.x	v6, zero
	addi	s0, t2, 1
	sltu	t1, s0, t2
	add	t0, t0, t1
	li	t1, -32
	vadd.vx	v4, v0, t1
	li	t1, 31
	vxor.vx	v5, v0, t1
	vblt	v4, v6, .LBB4_17
	vmv.v.x	v6, t0
	vsrl.vv	v6, v6, v4
	j	.LBB4_18
.LBB4_17:
	slli	t1, t0, 1
	vmv.v.x	v6, t1
	vsll.vv	v6, v6, v5
	vmv.v.x	v7, s0
	vsrl.vv	v7, v7, v0
	vor.vv	v6, v7, v6
.LBB4_18:
	vmv.v.x	v8, zero
	vmv.v.x	v7, t0
	vsrl.vv	v9, v7, v0
	vmslt.vi	v7, v4, 0
	vrsub.vi	v10, v7, 0
	vand.vv	v9, v10, v9
	vrsub.vi	v1, v1, 0
	vand.vv	v6, v1, v6
	vand.vv	v9, v1, v9
	vadd.vv	v1, v6, v2
	vmsltu.vv	v2, v1, v6
	vadd.vv	v3, v9, v3
	vadd.vv	v2, v3, v2
	lui	t0, 1048320
	vblt	v4, v8, .LBB4_20
	vmv.v.x	v3, t0
	vsra.vv	v3, v3, v4
	j	.LBB4_21
.LBB4_20:
	lui	t1, 1048064
	vmv.v.x	v3, t1
	vsll.vv	v3, v3, v5
.LBB4_21:
	vmv.v.x	v4, t0
	vsra.vv	v0, v4, v0
	vadd.vi	v4, v7, -1
	vor.vv	v4, v4, v0
	vand.vv	v0, v1, v3
	vand.vv	v3, v2, v4
.LBB4_22:
	vadd.vx	v2, v0, zero
.LBB4_23:
	vadd.vx	v0, v2, zero
	vadd.vx	v1, v3, zero
	ret
.Lfunc_end4:
	.size	ceil, .Lfunc_end4-ceil

	.globl	trunc
	.p2align	2
	.type	trunc,@function
trunc:
	vadd.vx	v3, v1, zero
	vadd.vx	v2, v0, zero
	vsrl.vi	v0, v1, 20
	li	t0, 2047
	vand.vx	v1, v0, t0
	li	t0, -1023
	li	t1, 1074
	vmv.v.x	v4, t1
	vadd.vx	v0, v1, t0
	vbltu	v4, v1, .LBB5_3
	li	t0, 1022
	vmv.v.x	v4, t0
	lui	t0, 524288
	vbltu	v4, v1, .LBB5_6
	vmv.v.x	v0, zero
	vand.vx	v1, v3, t0
	ret
.LBB5_3:
	vmv.v.x	v4, zero
	vmsltu.vv	v1, v0, v1
	vadd.vi	v1, v1, -1
	li	t0, 1024
	vxor.vx	v0, v0, t0
	vor.vv	v5, v0, v1
	vmsne.vi	v0, v5, 0
	vadd.vx	v1, v3, zero
	vbeq	v5, v4, .LBB5_5
	lui	t0, 524288
	vmv.v.x	v1, t0
.LBB5_5:
	addi	sp, sp, 4
	sw	ra, -4(sp)
	vadd.vi	v0, v0, -1
	vand.vv	v0, v0, v2
	call	__adddf3@plt
	lw	ra, -4(sp)
	addi	sp, sp, -4
	ret
.LBB5_6:
	vmv.v.x	v4, zero
	li	t1, -32
	vadd.vx	v1, v0, t1
	lui	t2, 1048320
	vblt	v1, v4, .LBB5_8
	vmv.v.x	v4, t2
	vsra.vv	v4, v4, v1
	j	.LBB5_9
.LBB5_8:
	li	t1, 31
	vxor.vx	v4, v0, t1
	lui	t1, 1048064
	vmv.v.x	v5, t1
	vsll.vv	v4, v5, v4
.LBB5_9:
	vmv.v.x	v5, t2
	vsra.vv	v0, v5, v0
	vmslt.vi	v1, v1, 0
	vadd.vi	v1, v1, -1
	vor.vv	v0, v1, v0
	vor.vx	v1, v0, t0
	vand.vv	v0, v4, v2
	vand.vv	v1, v1, v3
	ret
.Lfunc_end5:
	.size	trunc, .Lfunc_end5-trunc

	.globl	__fma
	.p2align	2
	.type	__fma,@function
__fma:
	addi	sp, sp, 4
	sw	ra, -4(sp)
.Lfunc_end6:
	.size	__fma, .Lfunc_end6-__fma

	.globl	sqrt
	.p2align	2
	.type	sqrt,@function
sqrt:
	addi	sp, sp, 4
	addi	tp, tp, 8
	regext	zero, zero, 1
	vmv.v.x	v32, tp
	sw	ra, -4(sp)
	regext	zero, zero, 1
	vadd.vx	v34, v1, zero
	lui	t0, 524032
	vand.vx	v1, v1, t0
	vmv.v.x	v2, t0
	regext	zero, zero, 1
	vadd.vx	v33, v0, zero
	vbne	v1, v2, .LBB7_3
	regext	zero, zero, 64
	vadd.vx	v0, v33, zero
	regext	zero, zero, 64
	vadd.vx	v1, v34, zero
	regext	zero, zero, 64
	vadd.vx	v2, v33, zero
	regext	zero, zero, 64
	vadd.vx	v3, v34, zero
	call	__muldf3@plt
	regext	zero, zero, 64
	vadd.vx	v2, v33, zero
	regext	zero, zero, 64
	vadd.vx	v3, v34, zero
	call	__adddf3@plt
.LBB7_2:
	regext	zero, zero, 1
	vadd.vx	v33, v0, zero
	regext	zero, zero, 1
	vadd.vx	v34, v1, zero
	j	.LBB7_33
.LBB7_3:
	vmv.v.x	v0, zero
	regext	zero, zero, 8
	vbge	v0, v34, .LBB7_12
.LBB7_4:
	vmv.v.x	v0, zero
	regext	zero, zero, 64
	vsrl.vi	v1, v34, 20
	vbne	v1, v0, .LBB7_11
	vmv.v.x	v0, zero
	li	t0, 1
	regext	zero, zero, 64
	vbne	v34, v0, .LBB7_8
	li	t0, 1
	vmv.v.x	v0, zero
.LBB7_7:
	regext	zero, zero, 65
	vsrl.vi	v34, v33, 11
	regext	zero, zero, 65
	vsll.vi	v33, v33, 21
	addi	t0, t0, -21
	regext	zero, zero, 64
	vbeq	v34, v0, .LBB7_7
.LBB7_8:
	vmv.v.x	v0, t0
	vmv.v.x	v1, zero
	regext	zero, zero, 64
	vsll.vi	v2, v34, 11
	vblt	v2, v1, .LBB7_15
	li	t0, 0
	vmv.v.x	v3, zero
.LBB7_10:
	regext	zero, zero, 64
	vsll.vi	v1, v34, 1
	addi	t0, t0, 1
	regext	zero, zero, 64
	vsll.vi	v4, v34, 12
	regext	zero, zero, 1
	vadd.vx	v34, v1, zero
	vmv.v.x	v2, t0
	vbge	v4, v3, .LBB7_10
	j	.LBB7_16
.LBB7_11:
	regext	zero, zero, 64
	vsra.vi	v0, v34, 20
	j	.LBB7_17
.LBB7_12:
	vmv.v.x	v0, zero
	lui	t0, 524288
	addi	t0, t0, -1
	regext	zero, zero, 64
	vand.vx	v1, v34, t0
	regext	zero, zero, 8
	vor.vv	v1, v1, v33
	vbeq	v1, v0, .LBB7_33
	vmv.v.x	v0, zero
	regext	zero, zero, 64
	vbge	v34, v0, .LBB7_4
	regext	zero, zero, 64
	vadd.vx	v0, v33, zero
	regext	zero, zero, 64
	vadd.vx	v1, v34, zero
	regext	zero, zero, 64
	vadd.vx	v2, v33, zero
	regext	zero, zero, 64
	vadd.vx	v3, v34, zero
	call	__subdf3@plt
	vadd.vx	v2, v0, zero
	vadd.vx	v3, v1, zero
	call	__divdf3@plt
	j	.LBB7_2
.LBB7_15:
	vmv.v.x	v2, zero
	regext	zero, zero, 64
	vadd.vx	v1, v34, zero
.LBB7_16:
	vrsub.vi	v3, v2, 0
	vsub.vv	v0, v0, v2
	li	t0, 31
	vand.vx	v3, v3, t0
	vmv.v.x	v4, t0
	regext	zero, zero, 64
	vsrl.vv	v3, v33, v3
	regext	zero, zero, 1
	vor.vv	v34, v3, v1
	vand.vv	v1, v2, v4
	regext	zero, zero, 65
	vsll.vv	v33, v33, v1
.LBB7_17:
	vmv.v.x	v1, zero
	li	t0, -1023
	vadd.vx	v0, v0, t0
	lui	t0, 256
	addi	t1, t0, -1
	regext	zero, zero, 64
	vand.vx	v3, v34, t1
	vand.vi	v2, v0, 1
	vor.vx	v3, v3, t0
	vbeq	v2, v1, .LBB7_19
	vsll.vi	v1, v3, 1
	regext	zero, zero, 64
	vsrl.vi	v3, v33, 31
	vor.vv	v3, v1, v3
.LBB7_19:
	vmv.v.x	v1, zero
	vmv.v.x	v4, zero
	regext	zero, zero, 64
	vsll.vv	v2, v33, v2
	vsll.vi	v3, v3, 1
	vsrl.vi	v5, v2, 31
	vor.vv	v3, v3, v5
	vsll.vi	v2, v2, 1
	lui	t1, 512
	li	t0, 1
	j	.LBB7_21
.LBB7_20:
	vmslt.vv	v6, v3, v5
	vadd.vi	v6, v6, -1
	vand.vx	v7, v6, t2
	vadd.vv	v1, v7, v1
	vand.vv	v5, v6, v5
	vsub.vv	v3, v3, v5
	vsll.vi	v3, v3, 1
	vsrl.vi	v5, v2, 31
	vor.vv	v3, v3, v5
	srli	t1, t2, 1
	vsll.vi	v2, v2, 1
	bgeu	t0, t2, .LBB7_23
.LBB7_21:
	mv	t2, t1
	vadd.vx	v5, v4, t1
	vblt	v3, v5, .LBB7_20
	vadd.vx	v4, v5, t2
	j	.LBB7_20
.LBB7_23:
	vmv.v.x	v5, zero
	vmv.v.x	v6, zero
	lui	t1, 524288
	li	t0, 1
	j	.LBB7_26
.LBB7_24:
	vmv.v.x	v9, t2
	vadd.vx	v6, v7, t2
	vmslt.vi	v10, v7, 0
	vmsgt.vi	v11, v6, 0
	vand.vv	v10, v10, v11
	vsub.vv	v3, v3, v4
	vadd.vv	v4, v4, v10
	vsub.vv	v3, v3, v8
	vsub.vv	v2, v2, v7
	vadd.vv	v5, v5, v9
.LBB7_25:
	vsrl.vi	v7, v2, 31
	vsll.vi	v3, v3, 1
	vor.vv	v3, v3, v7
	vsll.vi	v2, v2, 1
	srli	t1, t2, 1
	bgeu	t0, t2, .LBB7_28
.LBB7_26:
	mv	t2, t1
	vadd.vx	v7, v6, t1
	vmsltu.vv	v8, v2, v7
	vblt	v4, v3, .LBB7_24
	vmv.v.x	v9, zero
	vmsne.vv	v10, v3, v4
	vor.vv	v10, v10, v8
	vbeq	v10, v9, .LBB7_24
	j	.LBB7_25
.LBB7_28:
	vmv.v.x	v4, zero
	vor.vv	v2, v2, v3
	vbeq	v2, v4, .LBB7_32
	li	t0, -1
	vmv.v.x	v2, t0
	vbeq	v5, v2, .LBB7_31
	vand.vi	v2, v5, 1
	vadd.vv	v5, v2, v5
	j	.LBB7_32
.LBB7_31:
	vmv.v.x	v5, zero
	vadd.vi	v1, v1, 1
.LBB7_32:
	vsra.vi	v2, v1, 1
	vsrl.vi	v3, v5, 1
	vsll.vi	v1, v1, 31
	regext	zero, zero, 1
	vor.vv	v33, v1, v3
	vsll.vi	v0, v0, 19
	lui	t0, 1048320
	vand.vx	v0, v0, t0
	vadd.vv	v0, v0, v2
	lui	t0, 261632
	regext	zero, zero, 1
	vadd.vx	v34, v0, t0
.LBB7_33:
	regext	zero, zero, 64
	vadd.vx	v0, v33, zero
	regext	zero, zero, 64
	vadd.vx	v1, v34, zero
	lw	ra, -4(sp)
	addi	sp, sp, -4
	addi	tp, tp, -8
	ret
.Lfunc_end7:
	.size	sqrt, .Lfunc_end7-sqrt

	.type	rint.TWO52,@object
	.section	.rodata.cst16,"aM",@progbits,16
	.p2align	3, 0x0
rint.TWO52:
	.quad	0x4330000000000000
	.quad	0xc330000000000000
	.size	rint.TWO52, 16

	.ident	"clang version 16.0.0 (git@git.tpt.com:/git/ventus-llvm.git e6ae0f7ac69db8ef92c23950a364a10ec6c79cb7)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
