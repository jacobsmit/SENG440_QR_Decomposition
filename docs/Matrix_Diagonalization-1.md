SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

SENG440 Embedded Systems – Lesson 112: Matrix Diagonalization – 

## **Mihai SIMA** 

**msima@ece.uvic.ca** 

Academic Course 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Copyright © 2024 Mihai SIMA 

All rights reserved. 

No part of the materials including graphics or logos, available in these notes may be copied, photocopied, reproduced, translated or reduced to any electronic medium or machine-readable form, in whole or in part, without specific permission. Distribution for commercial purposes is prohibited. 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Disclaimer 

The purpose of this course is to present general techniques and concepts for the analysis, design, and utilization of embedded systems. The requirements of any real embedded system can be intimately connected with the environment in which the embedded system is deployed. The presented design examples should not be used as the full design for any real embedded system. 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

Project SVD Example References 

Introduction 

Jacobi 

Implementation 

## Lesson 112: Matrix Diagonalization 

- 1 Introduction 

- 2 Singular Value Decomposition 

- 3 Jacobi Method 

- 4 Cyclic Jacobi Method 

- 5 Singular Value Decomposition – Example 

- 6 SVD – Implementation Techniques 

- 7 Project Requirements 

- 8 Appendix – SVD Numerical Example 

- 9 References 

SENG440 Embedded Systems (112: Matrix Diagonalization) 

© 2024 Mihai SIMA 

Mihai SIMA 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Introduction 

Matrix diagonalization is required in many domains Wireless communications Control applications 

Singular Value Decomposition (SVD) achieves diagonalization It is difficult to calculate it fast SVD can be parallelized to a certain extend It poses numerical stability issues (especially in integer arithmetic) 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Singular Value Decomposition 

The Singular Value Decomposition (SVD) of a n × n matrix M is: M = U Σ V[T] 

where Σ is a diagonal matrix of singular values, and U and V are orthogonal / unitary matrices when M is real / complex-valued Carl Gustav Jacob Jacobi proposed a method to calculate the SVD The **Jacobi method** systematically reduces the off-diagonal elements to zero by applying a sequence of plane rotations to M to transform it into Σ Several sweeps over the entire matrix M may be necessary 

Within each sweep, the matrix elements need to be paired and appropriate rotations needs to be calculated. The n × n matrix is partitioned in n/2 × n/2 blocks, each block being a 2 × 2 matrix. 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

Project SVD Example References 

Introduction 

Jacobi 

Implementation 

## Singular Value Decomposition – Jacobi method 

|Consider the following<br>real-valued matrixM:|M=|<br><br><br><br><br><br><br><br><br><br><br><br><br>|m00<br>...<br>mi0<br>...<br>mj0<br>...<br>mn0|...<br>...<br>...<br>...|m0i<br>...<br>mii<br>...<br>mji<br>...<br>mni|...<br>...<br>...<br>...|m0j<br>...<br>mij<br>...<br>mjj<br>...<br>mnj|...<br>...<br>...<br>...|m0n<br>...<br>min<br>...<br>mjn<br>...<br>mnn|<br><br><br><br><br><br><br><br><br><br><br><br><br>|
|---|---|---|---|---|---|---|---|---|---|---|



Choose (i, j) such that |mij | is the maximum non-diagonal element For the following 2 × 2 matrix, the elements mij and mji are forced to zero 

**==> picture [50 x 31] intentionally omitted <==**

Propagate the computation along the rows i and j and columns i and j 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

Implementation Project SVD Example 

References 

Introduction 

Jacobi 

## Singular Value Decomposition – the core operation I 

The basic operation is the two-sided rotation of each 2 × 2 matrix 

**==> picture [183 x 33] intentionally omitted <==**

where θl and θr are the left and right rotation angles, respectively. The rotation matrices have the following forms: 

**==> picture [275 x 33] intentionally omitted <==**

Two issues need to be addressed: 

Calculation of the rotation angles Performing the rotations 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Singular Value Decomposition – the core operation II 

The direct two-angle method calculates θl and θr by computing the inverse tangents of the data elements of M: 

**==> picture [166 x 76] intentionally omitted <==**

The two angles, θl and θr , can be separated from the sum and difference results and applied to the two-sided rotation module to diagonalize M 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Cyclic Jacobi method 

The Jacobi method requires at each step the scanning of n(n − 1)/2 numbers for one of maximum modulus 

This can be time consuming for large matrices 

**Cyclic Jacobi method** : select the pairs (i, j) in some cyclic order The following order is called cyclic-by-rows: 

1 − 2 , 1 − 3 , ... , 1 − n , 2 − 3 , ... , 2 − n , 3 − 4 , ... , (n − 1) − n More than one sweep is likely to be needed 

Although some on-diagonal energy may go off-diagonal at some iterations, the process converges in a few sweeps - There is no needed to completely vanish an off diagonal element! The off-diagonal energy moves toward the main diagonal The magnitude of the off-diagonal elements approaches zero 

© 2024 Mihai SIMA 

Mihai SIMA 

SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Singular Value Decomposition – example I 

The matrix for which SVD is to be calculated is M 

**==> picture [272 x 71] intentionally omitted <==**

**----- Start of picture text -----**<br>
1 0 0 0 31 77 −11 26 1 0 0 0<br>0 1 0 0 −42 14 79 −53 0 1 0 0<br>0 0 1 0 · −68 −10 45 90 · 0 0 1 0<br>0 0 0 1 34 16 38 −19 0 0 0 1<br>     <br>� � � � � � � � � �� �<br>U M V [T]<br>**----- End of picture text -----**<br>


In the initial state of the algorithm Σ = M and U = V = I Plane rotations will be applied to diagonalize M These plane rotations will be incorporated into U and V 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Singular Value Decomposition – example II 

Cyclic Jacobi method is used with the following six iterations: 

1 − 2, 1 − 3, 1 − 4, 2 − 3, 2 − 4, 3 − 4 (this constitutes one sweep) The calculations for the first sweep are presented in the appendix 

The evolution of the diagonalization process over four sweeps is shown in the next slide 

Observe that the off-diagonal energy accumulates on the main diagonal 

- The magnitudes of the off-diagonal elements decrease The magnitudes of the diagonal elements increase, which creates difficulties in calculating M in fixed-point arithmetic 

The orthogonal matrices U and V are shown in the appendix 

Their elements range from -1.0 to 1.0 **→** this guarantees stability in calculating U and V in fixed-point arithmetic 

© 2024 Mihai SIMA 

Mihai SIMA 

SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Singular Value Decomposition – multiple sweeps 

|M <br>M1|=<br> =|<br><br><br><br>31<br>77<br>−11<br>−42<br>14<br>79<br>−68<br>−10<br>45<br>34<br>16<br>38<br><br><br><br><br>100.529<br>−15.789<br>−24.869<br>104.453<br>−11.624<br>−0.992<br>2.764<br>−0.250|26<br>−53<br>90<br>−19<br><br><br><br><br>8.876<br>2.662<br>−110.708<br>0|29.798<br>9.100<br>0<br>37.710<br><br><br><br>|
|---|---|---|---|---|
|M2|=|<br><br><br><br>85.636<br>1.354<br>−4.998<br>126.299<br>−1.134<br>0.190<br>−0.265<br>0|0.696<br>0.027<br>−110.873<br>0|0.160<br>0<br>0<br>34.008<br><br><br><br>|
|M3|=|<br><br><br><br>85.570<br>0.009<br>0<br>126.429<br>0<br>0<br>0<br>0|0<br>0<br>−110.905<br>0|0<br>0<br>0<br>34.008<br><br><br><br>|
|Σ|=|<br><br><br><br>85.570<br>0<br>0<br>126.429<br>0<br>0<br>0<br>0|0<br>0<br>−110.905<br>0|0<br>0<br>0<br>34.008<br><br><br><br>|



The original matrix is M The matrices M1, M2, and M3 are the matrices at the end of sweeps 1, 2, and 3, respectively 

At the end of Sweep 4 the matrix is completely diagonalized; therefore, it is labeled Σ The algorithm converges fast The negative diagonal elements can be forced positive through a simple multiplication 

A final permutation can order the singular ~~va~~ lues 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Singular Value Decomposition – operation count I 

## **Calculation of the rotation angles requires** : 

The evaluation of arctan 

## arctan is a transcendental function 

Is series expansion appropriate to evaluate arctan? Are there alternatives to a series expansion? 

## **Performing the rotations requires** : 

The evaluation of cos and sin Matrix multiplication 

cos and sin are transcendental functions 

Is series expansion appropriate to evaluate cos and sin? Are there alternatives to a series expansion? 

Can matrix multiplication be implemented with standard instructions? 

Multiply-and-ACcumulate (MAC) operations are good candidates SIMD operations are good candidates, since the code can be parallelized 

© 2024 Mihai SIMA 

Mihai SIMA 

SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Singular Value Decomposition – operation count II 

In a serial computer, the **calculation of the rotation angles** and **performing the rotations** are both expensive tasks 

Specialized processors, with architectural support for DSP and SIMD operations, speed up matrix multiplication, but the calculation of trigonometric functions remains difficult 

An Application-Specific Instruction set Processor (ASIP), which provides architectural support for trigonometric functions would be an excellent choice, but such a processor is not readily available 

A good understanding of the SVD algorithm is required in order to simplify the calculation of the trigonometric functions 

- There is no needed to completely vanish an off-diagonal element as long as the off-diagonal energy keeps moving toward the main diagonal **→** incomplete rotations should be simpler to implement, but more sweeps might me needed 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

Project SVD Example 

References 

Introduction 

Jacobi 

Implementation 

## How to calculate arctan(x)? I 

**==> picture [215 x 28] intentionally omitted <==**

A domain like (−∞, +∞) should be avoided 

Calculate arctan(x) when |x| ≤ 1 Calculate arccot(x) when |x| > 1 and adjust the angle accordingly 

In C using floating point: 

**#include** <math.h> 

... 

**int** x, y, angle; 

**if** (x > y) 

angle = arctan( y/x); 

/* arctan() returns a float 

## **else** 

angle = PI/2 - arctan( x/y); /* arctan() returns a float 

© 2024 Mihai SIMA 

Mihai SIMA 

SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

Project SVD Example References 

Introduction 

Jacobi 

Implementation 

## How to calculate arctan(x)? II 

## Integer / fixed-point arithmetic is required! 

C standard library ( **math.h** ): arctan() is a floating-point function "/" is not a good option to divide integers π is a fractional number 

How to implement arctan() in integer / fixed-point arithmetic? Taylor series expansion about a point – approximation good for 1 point 

**==> picture [161 x 21] intentionally omitted <==**

Tchebishev polynomial – approximation good for an interval (homework) Piecewise linear approximation with three middle points: 

**==> picture [220 x 59] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA 

SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## arctan(x) – piecewise linear approximation I 

Piecewise linear approximation using fractional numbers 

 0.644 x + 0.142 if 0.5 < x ≤ 1.0, arctan(x) = 0.928 x if −0.5 ≤ x ≤ 0.5,    0.644 x − 0.142 if −1.0 ≤ x < −0.5.  

Consider 12-bit signed integers 

Conversion from fractional representation to integer representation 

1.0 is represented as 2[11] (in fact, as 2[11] − 1) 0.928 is represented as 1900 = 76Ch 

0.644 is represented as 1319 = 527h 

0.142 is represented as 291 = 123h 0.5 is represented as 1024 = 400h x is represented as X = 2[11] x 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## arctan(x) – piecewise linear approximation II 

Piecewise linear approximation using integer arithmetic 

 1319 X + 291 if 1024 < X ≤ 2048, arctan(X ) = 1900 X if −1024 ≤ X ≤ 1024,   1319 X − 291 if −2048 ≤ X < −1024.  

arctan(X ) is a signed integer ranging −1, 350, 947 ··· + 1, 350, 947, which in hex is −149D23h ··· + 149D23h 

How many bits are needed to represent arctan(X )? 

Questions to be posed: 

What is the computing time in an all-software implementation versus in a hardware implementation? 

What is the precision of the piecewise linear approximation when integer / fixed-point arithmetic is used? 

## The same considerations can be extended to sine and cosine 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## arctan(x) – piecewise linear approximation III 

Calculating the arctangent (that is, the rotation angles) with low accuracy amounts with a partial rotation, which slightly slows the convergence of the SVD algorithm 

With what accuracy should sine and cosine be calculated? 

In any case with equal accuracy, or the rotation will no longer be a rotation Low (but equal) accuracy for sine and cosine amounts with a partial rotation, which is acceptable as discussed 

Maybe a better idea would be to restrict the rotation angles to a set of predefined angles, whose arctangent, sine, and cosine functions can be either easily calculated or prestored in memory 

The student(s) doing the SVD project should consult the student(s) doing the CORDIC project 

© 2024 Mihai SIMA 

Mihai SIMA 

SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Jacobi method – side effects 

It works fine with rectangular matrices, too. 

If the matrix is symmetric, the algorithm finds the eigenvalues. The SVD of a complex matrix can be calculated in a similar way This is beyond the course scope 

Matrix triangularization can be achieved with one-side rotations Upper triangularization with left-side rotations Lower triangularization with right-side rotations 

The algorithm can be parallelized, which will increase the speed of the algorithm implementation 

SIMD (vector) operations are good candidates for this 

© 2024 Mihai SIMA 

Mihai SIMA 

SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Matrix diagonalization – project requirements I 

Generate a square matrix of integers (12-bit wordlength, for example) 

Consider the piecewise linear approximation for arctan, sin, and cos, and determine the maximum error for three middle points 

Implement piecewise linear approximation using integer arithmetic for sin, cos, and arctan in: 

software (write C routines) horizontal firmware with two issue slots custom hardware (write VHDL/Verilog) 

Define a new instruction that will return the trigonometric function 

You must comply with the ARM architecture (you can have at most two arguments and one result per instruction call) 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Matrix diagonalization – project requirements II 

Rewrite the high-level code and instantiate the new instruction Use assembly inlining 

Diagonalize a square matrix using piecewise linear approximation of trigonometric functions and estimate: 

the performance improvement of hardware-based solution versus software-based solution 

the performance improvement of a 2-issue slot firmware-based solution versus software-based solution 

Estimate the penalty for the hardware solution 

Determine the number of gates needed to implement the new instruction 

© 2024 Mihai SIMA 

Mihai SIMA 

SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example References 

Introduction 

Jacobi 

Implementation 

Project 

## Appendix – SVD numerical example 

The matrix for which SVD is to be calculated is M 

**==> picture [272 x 71] intentionally omitted <==**

**----- Start of picture text -----**<br>
1 0 0 0 31 77 −11 26 1 0 0 0<br>0 1 0 0 −42 14 79 −53 0 1 0 0<br>0 0 1 0 · −68 −10 45 90 · 0 0 1 0<br>0 0 0 1 34 16 38 −19 0 0 0 1<br>     <br>� � � � � � � � � �� �<br>U M V [T]<br>**----- End of picture text -----**<br>


In the initial state of the algorithm Σ = M and U = V = I Plane rotations will be applied to diagonalize M These plane rotations will be incorporated into U and V 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example Implementation Project SVD Example References 

Introduction 

Jacobi 

## − SVD – Iteration 1, Sweep 1: pair (1 2) is selected I 

Elements on Rows and Columns 1 and 2 are selected to form a 2 × 2 matrix The left and right rotation angles, θl,12 and θr ,12 are calculated 

**==> picture [304 x 142] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example References 

Introduction 

Jacobi 

Implementation 

Project 

− SVD – Iteration 1, Sweep 1: pair (1 2) is selected II 

The orthogonal matrix U12 is built with the left rotation angle, θl,12 The orthogonal matrix V12 is built with the right rotation angle, θr ,12 The left and right rotations are applied to M to give M[′] The matrices U12 and V12 are incorporated into U and V to give U[′] and V[′] U, M, and V are updated with U[′] , M[′] , and V[′] , to be used in the next iteration 

**==> picture [283 x 104] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## − SVD – Iteration 1, Sweep 1: pair (1 2) is selected III 

The matrices U, M, and V at the end of iteration (1 − 2) are shown below The off-diagonal energy accumulates on the main diagonal 

The magnitudes of the off-diagonal elements decrease The magnitudes of the diagonal elements increase 

The elements of the orthogonal matrices U and V range from -1.0 to 1.0 

**==> picture [268 x 89] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

Project SVD Example References 

Introduction 

Jacobi 

Implementation 

## − SVD – Iteration 2, Sweep 1: pair (1 3) is selected I 

Elements on Rows and Columns 1 and 3 are selected to form a 2 × 2 matrix The left and right rotation angles, θl,13 and θr ,13 are calculated 

**==> picture [327 x 138] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example References 

Introduction 

Jacobi 

Implementation 

Project 

− SVD – Iteration 2, Sweep 1: pair (1 3) is selected II 

The orthogonal matrix U13 is built with the left rotation angle, θl,13 The orthogonal matrix V13 is built with the right rotation angle, θr ,13 The left and right rotations are applied to M to give M[′] The matrices U13 and V13 are incorporated into U and V to give U[′] and V[′] U, M, and V are updated with U[′] , M[′] , and V[′] , to be used in the next iteration 

**==> picture [283 x 104] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## − SVD – Iteration 2, Sweep 1: pair (1 3) is selected III 

The matrices U, M, and V at the end of iteration (1 − 3) are shown below The off-diagonal energy accumulates on the main diagonal 

The magnitudes of the off-diagonal elements decrease The magnitudes of the diagonal elements increase 

The elements of the orthogonal matrices U and V range from -1.0 to 1.0 

**==> picture [325 x 89] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

Project SVD Example References 

Introduction 

Jacobi 

Implementation 

## − SVD – Iteration 3, Sweep 1: pair (1 4) is selected I 

Elements on Rows and Columns 1 and 4 are selected to form a 2 × 2 matrix The left and right rotation angles, θl,14 and θr ,14 are calculated 

**==> picture [331 x 138] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

Implementation Project SVD Example References 

Introduction 

Jacobi 

− SVD – Iteration 3, Sweep 1: pair (1 4) is selected II 

The orthogonal matrix U14 is built with the left rotation angle, θl,14 The orthogonal matrix V14 is built with the right rotation angle, θr ,14 The left and right rotations are applied to M to give M[′] The matrices U14 and V14 are incorporated into U and V to give U[′] and V[′] U, M, and V are updated with U[′] , M[′] , and V[′] , to be used in the next iteration 

**==> picture [283 x 104] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## − SVD – Iteration 3, Sweep 1: pair (1 4) is selected III 

The matrices U, M, and V at the end of iteration (1 − 4) are shown below The off-diagonal energy accumulates on the main diagonal 

The magnitudes of the off-diagonal elements decrease The magnitudes of the diagonal elements increase The elements of the orthogonal matrices U and V range from -1.0 to 1.0 

**==> picture [302 x 86] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example Implementation 

Project SVD Example References 

Introduction 

Jacobi 

## − SVD – Iteration 3, Sweep 1: pair (2 3) is selected I 

Elements on Rows and Columns 2 and 3 are selected to form a 2 × 2 matrix The left and right rotation angles, θl,23 and θr ,23 are calculated 

**==> picture [326 x 138] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example References 

Introduction 

Jacobi 

Implementation 

Project 

− SVD – Iteration 3, Sweep 1: pair (2 3) is selected II 

The orthogonal matrix U23 is built with the left rotation angle, θl,23 The orthogonal matrix V23 is built with the right rotation angle, θr ,23 The left and right rotations are applied to M to give M[′] The matrices U23 and V23 are incorporated into U and V to give U[′] and V[′] U, M, and V are updated with U[′] , M[′] , and V[′] , to be used in the next iteration 

**==> picture [283 x 104] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## − SVD – Iteration 3, Sweep 1: pair (2 3) is selected III 

The matrices U, M, and V at the end of iteration (2 − 3) are shown below The off-diagonal energy accumulates on the main diagonal 

The magnitudes of the off-diagonal elements decrease The magnitudes of the diagonal elements increase The elements of the orthogonal matrices U and V range from -1.0 to 1.0 

**==> picture [306 x 86] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi SVD Example Implementation 

Project SVD Example References 

Introduction 

Jacobi 

## − SVD – Iteration 3, Sweep 1: pair (2 4) is selected I 

Elements on Rows and Columns 2 and 4 are selected to form a 2 × 2 matrix The left and right rotation angles, θl,24 and θr ,24 are calculated 

**==> picture [336 x 138] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example References 

Introduction 

Jacobi 

Implementation 

Project 

− SVD – Iteration 3, Sweep 1: pair (2 4) is selected II 

The orthogonal matrix U24 is built with the left rotation angle, θl,24 The orthogonal matrix V24 is built with the right rotation angle, θr ,24 The left and right rotations are applied to M to give M[′] The matrices U24 and V24 are incorporated into U and V to give U[′] and V[′] U, M, and V are updated with U[′] , M[′] , and V[′] , to be used in the next iteration 

**==> picture [283 x 104] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## − SVD – Iteration 3, Sweep 1: pair (2 4) is selected III 

The matrices U, M, and V at the end of iteration (2 − 4) are shown below The off-diagonal energy accumulates on the main diagonal 

The magnitudes of the off-diagonal elements decrease The magnitudes of the diagonal elements increase The elements of the orthogonal matrices U and V range from -1.0 to 1.0 

**==> picture [306 x 86] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

Project SVD Example References 

Introduction 

Jacobi 

Implementation 

## − SVD – Iteration 3, Sweep 1: pair (3 4) is selected I 

Elements on Rows and Columns 3 and 4 are selected to form a 2 × 2 matrix The left and right rotation angles, θl,34 and θr ,34 are calculated 

**==> picture [338 x 138] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example References 

Introduction 

Jacobi 

Implementation 

Project 

− SVD – Iteration 3, Sweep 1: pair (3 4) is selected II 

The orthogonal matrix U34 is built with the left rotation angle, θl,34 The orthogonal matrix V34 is built with the right rotation angle, θr ,34 The left and right rotations are applied to M to give M[′] The matrices U34 and V34 are incorporated into U and V to give U[′] and V[′] U, M, and V are updated with U[′] , M[′] , and V[′] , to be used in the next iteration 

**==> picture [283 x 104] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## − SVD – Iteration 3, Sweep 1: pair (3 4) is selected III 

The matrices U, M, and V at the end of iteration (3 − 4) are shown below The off-diagonal energy accumulates on the main diagonal 

The magnitudes of the off-diagonal elements decrease The magnitudes of the diagonal elements increase The elements of the orthogonal matrices U and V range from -1.0 to 1.0 

**==> picture [306 x 86] intentionally omitted <==**

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example References 

Introduction 

Jacobi 

Implementation 

Project 

## Singular Value Decomposition – multiple sweeps 

|M <br>M1|=<br> =|<br><br><br><br>31<br>77<br>−11<br>−42<br>14<br>79<br>−68<br>−10<br>45<br>34<br>16<br>38<br><br><br><br><br>100.529<br>−15.789<br>−24.869<br>104.453<br>−11.624<br>−0.992<br>2.764<br>−0.250|26<br>−53<br>90<br>−19<br><br><br><br><br>8.876<br>2.662<br>−110.708<br>0|29.798<br>9.100<br>0<br>37.710<br><br><br><br>|
|---|---|---|---|---|
|M2|=|<br><br><br><br>85.636<br>1.354<br>−4.998<br>126.299<br>−1.134<br>0.190<br>−0.265<br>0|0.696<br>0.027<br>−110.873<br>0|0.160<br>0<br>0<br>34.008<br><br><br><br>|
|M3|=|<br><br><br><br>85.570<br>0.009<br>0<br>126.429<br>0<br>0<br>0<br>0|0<br>0<br>−110.905<br>0|0<br>0<br>0<br>34.008<br><br><br><br>|
|Σ|=|<br><br><br><br>85.570<br>0<br>0<br>126.429<br>0<br>0<br>0<br>0|0<br>0<br>−110.905<br>0|0<br>0<br>0<br>34.008<br><br><br><br>|



The original matrix is M The matrices M1, M2, and M3 are the matrices at the end of sweeps 1, 2, and 3, respectively 

At the end of Sweep 4 the matrix is completely diagonalized; therefore, it is labeled Σ The algorithm converges fast The negative diagonal elements can be forced positive through a simple multiplication 

A final permutation can order the singular ~~va~~ lues 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## References 

Gene H. Golub and Charles F. van Loan, Matrix Computations, Johns Hopkins University Press, fourth edition, 2012. 

Lloyd N. Trefethen and David Bau, III, Numerical Linear Algebra, Society for Industrial and Applied Mathematics, 1997. 

James W. Demmel, Applied Numerical Linear Algebra, Society for Industrial and Applied Mathematics, 1997. 

Carl Meyer, Matrix Analysis and Applied Linear Algebra, Society for Industrial and Applied Mathematics, 2001. 

Professor Richard P. Brent (publications on matrix factorization) http://web.comlab.ox.ac.uk/oucl/work/richard.brent/ 

© 2024 Mihai SIMA 

Mihai SIMA 

SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Project Specification Sheet 

**Summer 2023** 

**Student name:** . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . **Student ID:** . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . **Matrix size and type:** . . . . . . . . . . . . . . . . . . . . . . . . . . . 4-by-4 real-valued **Language:** . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . C/C++ **Processor:** . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 32-bit ARM **Wordlength:** . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 32 bits **Deadline:** . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Questions, feedback 

**?** Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

© 2024 Mihai SIMA 

SVD 

Cyclic Jacobi 

SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Notes I 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Notes II 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Notes III 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

SVD 

Cyclic Jacobi SVD Example 

SVD Example 

References 

Introduction 

Jacobi 

Implementation 

Project 

## Notes IV 

© 2024 Mihai SIMA 

Mihai SIMA SENG440 Embedded Systems (112: Matrix Diagonalization) 

