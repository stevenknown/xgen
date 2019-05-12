
#VAR5(g):global,align(4),i32,decl:''
.common g, 4, 4
.align 2



.section .text, "ax", "progbits"
.type myprogram, %function
.global myprogram
myprogram:

.size myprogram, .-myprogram

.align 2



.section .text, "ax", "progbits"
.type main, %function
.global main
main:

#START BB1, entry

   add sp, sp, #-32
 
  str r6, [sp, #24]
 
  str r5, [sp, #20]
 
  str r4, [sp, #16]
 
 str r14, [sp, #12]
 
        movw r5, #1
 
        movt r5, #0
 
   str r5, [sp, #0]
 
   ldr r6, [sp, #4]
 
   ldr r4, [sp, #8]
 
         cmp r6, r4
 
       ble main__L1
 
#START BB2

        movw r4, #2
 
        movt r4, #0
 
   str r4, [sp, #0]
 
         b main__L2
 
#START BB4
main__L1:

        movw r4, #3
 
        movt r4, #0
 
   str r4, [sp, #0]
 
#START BB5
main__L2:

             bl foo
 
#START BB6, exit

   ldr r4, [sp, #0]
 
         mov r0, r4
 
 ldr r14, [sp, #12]
 
  ldr r4, [sp, #16]
 
  ldr r5, [sp, #20]
 
  ldr r6, [sp, #24]
 
    add sp, sp, #32
 
             bx r14
 
.size main, .-main

.align 2



.section .text, "ax", "progbits"
.type foo, %function
.global foo
foo:

#START BB1, entry

   add sp, sp, #-40
 
  str r8, [sp, #36]
 
  str r7, [sp, #32]
 
  str r6, [sp, #28]
 
  str r5, [sp, #24]
 
  str r4, [sp, #20]
 
 str r14, [sp, #16]
 
        movw r4, #5
 
        movt r4, #0
 
   str r4, [sp, #8]
 
     add r5, sp, #8
 
         mov r0, r5
 
             bl bar
 
#START BB2, exit

  ldr r8, [sp, #12]
 
   ldr r7, [sp, #8]
 
     add r6, r8, r7
 
   str r6, [sp, #8]
 
movw r5, #:lower16:g
 
movt r5, #:upper16:g
 
   ldr r4, [r5, #0]
 
   str r4, [sp, #8]
 
 ldr r14, [sp, #16]
 
  ldr r4, [sp, #20]
 
  ldr r5, [sp, #24]
 
  ldr r6, [sp, #28]
 
  ldr r7, [sp, #32]
 
  ldr r8, [sp, #36]
 
    add sp, sp, #40
 
.size foo, .-foo

.align 2



.section .text, "ax", "progbits"
.type bar, %function
.global bar
bar:

#START BB1, entry, exit

   add sp, sp, #-32
 
  str r0, [sp, #16]
 
  str r1, [sp, #20]
 
  str r2, [sp, #24]
 
  str r3, [sp, #28]
 
  str r5, [sp, #12]
 
   str r4, [sp, #8]
 
        movw r5, #1
 
        movt r5, #0
 
   str r5, [sp, #0]
 
        movw r4, #1
 
        movt r4, #0
 
   str r4, [sp, #4]
 
   ldr r4, [sp, #8]
 
  ldr r5, [sp, #12]
 
    add sp, sp, #32
 
.size bar, .-bar
