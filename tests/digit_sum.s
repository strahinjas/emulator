.global digit_sum

# arguments:
# r0 = target number
#
# return:
# r0 = sum of its digits

.text
digit_sum:
	push r2
	push r3
	push r4
	push r5
	
	mov r4, r0		# r4 = number
	mov r0, 0		# r0 = digit sum
	mov r4, r4
loop:
	jeq exit
	div r4, 10		# remainder is in r5 = digit to add
	add r0, r5
	mov r4, r4
	jmp loop
	
exit:
	pop r5
	pop r4
	pop r3
	pop r2
	ret

.end
