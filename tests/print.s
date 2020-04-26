.global print

# arguments:
# r0 = array's starting address
# r1 = array's size
#
# return:
# void

.text
print:
	push r2
	push r3
	push r4
	push r5
	
	mov r5, r0		# r5 = array
	mov r4, r1		# r4 = size
	mov r3, r4		# r3 = iterator
	
loop:
	jeq exit
	mov r2, r4
	sub r2, r3		# r2 = current index
	shl r2, 1
	add r2, r5
	mov r0, [r2]	# r0 = current element
	call printn
	
	mov data_out, 32
	
	sub r3, 1
	jmp loop
	
exit:
	mov data_out, 10
	pop r5
	pop r4
	pop r3
	pop r2
	ret

# helper recursive function
# that prints just one number
# 
# arguments:
# r0 = number to print
#
# return:
# void

	.equ ascii_zero, 48
	.equ data_out, 0xFF00

printn:
	push r2
	push r3
	push r4
	push r5
	
	mov r4, r0		# r4 = number
	jeq return
	div r4, 10		# remainder is in r5 = digit to print
	mov r0, r4
	call printn
	
	add r5, &ascii_zero
	mov data_out, r5
	
return:
	pop r5
	pop r4
	pop r3
	pop r2
	ret

.end